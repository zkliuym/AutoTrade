#include "SPTrader.h"
#include "OrderItem.h"
#include "TradeItem.h"
#include "PriceItem.h"
#include "Dispatcher.h" 

#pragma comment(lib,"ws2_32.lib")

// added by xie
bool SPOrderInterface::requestLinkState(LinkID linkID)
{
	char buf[256] = {0};
	_snprintf(buf, 256, "9000,%d\r\n", linkID);
	string req(buf);
	if (send(orderSocket, req.c_str(), req.length(),0) < 0){
		LogHandler::getLogHandler().alert(1, "Connection status", "Send message for checking connection status failed!");
		return false;
	}
	return true;
}

bool SPOrderInterface::getConnectStatus()
{
	return connectStatus;
}

void SPOrderInterface::setConnectStatus(bool status) 
{
	this->connectStatus = false;
}

int SPOrderInterface::login(string server, string account_no, string password, int orderPort, int pricePort, int tickPort)
{
		//starting socket library
	this->accountNo = account_no;
	this->server = server;
	this->orderPort = orderPort;
	this->pricePort = pricePort;
	this->tickPort = tickPort;
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;	
	wVersionRequested = MAKEWORD( 2, 0 );	
	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) 	{
		cout<<"Socket2.0  initialization failed，Exit!";
		return err;
	}
	if ( LOBYTE( wsaData.wVersion ) != 2 ||	HIBYTE( wsaData.wVersion ) != 0 ){
		WSACleanup();
		return -1; 
	}
//create the sokcet
	orderSocket = socket(AF_INET,SOCK_STREAM,0);
	if (orderSocket == INVALID_SOCKET ) {
		cout<<"Socket creation failed，Exit!";
		return -1;
	}
	//define the address of SPTrader
	sockaddr_in myaddr; 
	memset(&myaddr,0,sizeof(myaddr));
	myaddr.sin_family=AF_INET;
	myaddr.sin_addr.s_addr = inet_addr(server.c_str());  
	myaddr.sin_port=htons(orderPort);

	//connect to SPTrader
	if (connect(orderSocket,(sockaddr*)&myaddr,sizeof(myaddr)) == SOCKET_ERROR){
		cout<<"connect failed, exit!"<<endl;
		closesocket(orderSocket);
		//WSACleanup(); // the cleanup is change to the cleanup function
		exit(1);
	}
	else	{
		cout<<"connect succeed!"<<endl;
	}
	//char * loginfo = NULL;
	//loginfo = (char*)malloc(12+strlen(userid)+strlen(password)+strlen(server));
	//sprintf(loginfo,"3101,0,%s,%s,%s\r\n",userid,password,server);
	string loginfo = "3101,0,"+account_no+","+password+","+server+"\r\n";
	if(send(orderSocket,loginfo.c_str(),loginfo.length(),0)<0){
		cout<<"Send login info error!"<<endl;
	}

	char buf[256]="";	
	long number=0;
	int pkglen = 0;
	while(true){
		number++;
		pkglen = recv(orderSocket,buf,sizeof(buf),0);
		if (pkglen < 0){
			cout<<"Server Socket closed, Exit！"<<endl;
			break;
		}
		else if(strstr(buf,"3101")>=0 && strstr(buf,"OK")>0){
				char *acc_no = NULL;
				char *delims = ",";
				//strcpy(priceinfo2,pricerecord.c_str());
				acc_no = strtok(buf,delims);
				int c=0;
				while(acc_no!=NULL){					
					acc_no=strtok(NULL,delims);
					c++;
					if(c==9) break;
				}
				//cout<<acc_no<<endl;
				//account_no = (char*)malloc(strlen(acc_no)-2);
				//strncpy(account_no,acc_no,strlen(acc_no)-2);
				//account_no =((string)acc_no).substr(0,strlen(acc_no)-2);
				connectStatus = true;
				break;
		}
		memset(buf,0,sizeof(buf));
	}
	return 0;
}


HANDLE SPOrderInterface::startOrderThread()
{
	hOrderThread = CreateThread(NULL, 0, this->orderThreadAdapter, this, 0, &orderThreadId);
	return hOrderThread;
}

HANDLE SPOrderInterface::startPriceThread()
{
	initPriceConnection();
      hPriceThread = CreateThread(NULL, 0, this->priceThreadAdapter, this, 0, &priceThreadId);
	return hPriceThread;
}

// added by xie
HANDLE SPOrderInterface::startCheckConnectionThread()
{
	hCheckConnectionThread = CreateThread(NULL, 0, this->checkConnectionThreadAdapter, this, 0, &checkConnectionThreadId);
	return hCheckConnectionThread;
}

// added by xie
HANDLE SPOrderInterface::startTickerThread()
{
	hTickerThread = CreateThread(NULL, 0, this->tickerThreadAdapter, this, 0, &tickerThreadId);
	return hTickerThread;
}

DWORD WINAPI SPOrderInterface::orderThreadAdapter(LPVOID lpParam)
{
	SPOrderInterface *spoi = (SPOrderInterface *)lpParam;
	spoi->processOrder();
	return 0; 
}

DWORD WINAPI SPOrderInterface::priceThreadAdapter(LPVOID lpParam)
{
	SPOrderInterface *spoi = (SPOrderInterface *)lpParam;
	spoi->processPrice();
	return 0; 
}

/** 
  *检查与服务器的连接情况 by xie
  */
DWORD WINAPI SPOrderInterface::checkConnectionThreadAdapter(LPVOID lpParam)
{
	SPOrderInterface *spoi = (SPOrderInterface *)lpParam;
	SetTimer(NULL, 1, spoi->timerInterval * 1000, NULL);
	MSG  msg; 

	map<LinkID, string> linkInfo;
	linkInfo[TRANSACTION_CLIENT_LINK] = "Transaction client link";
	//linkInfo[TRANSACTION_ADMIN_LINK] = "Transaction admin link";
	linkInfo[PRICE_LINK] = "Price link";
	//linkInfo[LONG_DEPTH_LINK] = "Long depth link";
	//linkInfo[INFORMATION_LINK] = "Information link";
	//linkInfo[MARGIN_CHECK_LINK] = "Margin check link";
	map<LinkID, string>::iterator it = linkInfo.begin();
		
	int id = 0, status = -1;
	char* pstr = NULL;
	char buf[256] = {0};
	while(GetMessage(&msg, NULL, 0, 0)) {
		if (msg.message == WM_TIMER) {
			//LogHandler::getLogHandler().log("3 seconds-----------");
			for (it = linkInfo.begin(); it != linkInfo.end(); ++it) {
				//LogHandler::getLogHandler().log("Check " + it->second);
				if (!spoi->requestLinkState(it->first)) {
					spoi->setConnectStatus(false);
				}
			}
		} else if (msg.message == LINK_STATE_MSG) {
				memset(buf, 0, sizeof(buf));
				memcpy(buf, (char*)msg.lParam, 256);
				//LogHandler::getLogHandler().log(buf);
				pstr = strtok(buf, ",");
				pstr = strtok(NULL, ",");
				if (pstr != NULL) {
					id = atoi(pstr);
				}
				it = linkInfo.find((LinkID)id);
				if (it != linkInfo.end()) {
					pstr = strtok(NULL, ",");
					if (pstr != NULL) {
						status = atoi(pstr);
					}
					if (status == 0) {
						//LogHandler::getLogHandler().log(it->second + " is connected!");
					} else if (status == 1) {
						spoi->setConnectStatus(false);
						LogHandler::getLogHandler().alert(3, "Connection status", it->second + " is broken!");
					}
				}
				if (!spoi->getConnectStatus()) {
					// 失去连接 
					LogHandler::getLogHandler().alert(3, "Lost Connection!", "Lost Connection!");
					spoi->dispatcher->loseConnection();
				}
		}
	}
	return 0; 
}

// added by xie
DWORD WINAPI SPOrderInterface::tickerThreadAdapter(LPVOID lpParam)
{
	SPOrderInterface *spoi = (SPOrderInterface *)lpParam;
	if (spoi->initTickConnection() < 0) return 0;

	spoi->processTickerMessage();

	return 0; 
}

int SPOrderInterface::sendOrder(OrderItem* pOrderItem)
{
	int len=0;
	if(connectStatus==true){				
		string orderStr = this->orderItem2Str(pOrderItem);
		//LogHandler::getLogHandler().log(orderStr);
		pOrderItem->log();
		if((len=send(orderSocket,orderStr.c_str(),orderStr.length(),0))<0){
			cout<<"Send ORDER orders error, please check the order socket!"<<endl;
			//exit(-1);
		}
	}
	else
	{
		cout<<"Connection to SPTrader is not established or broken!"<<endl;
		//exit(-1);
	}
	return len;
}

void SPOrderInterface::processPrice()
{
	char buff[300]="";	
	int pkglen = 0;
	// Get price update continually via a loop
	string rmd = "", priceStr="";
	while(true)
	{
		pkglen = recv(priceSocket,buff,sizeof(buff),0);
		if (pkglen < 0){
			cout<<"receive price data failed, exit！"<<endl;
			break;
		}
		else
		{
			
			//if price is correctly received, then reply to server  a 4102 message
			int start = 0;
			priceStr="";
			string tmp(buff);
			//LogHandler::getLogHandler().log(tmp);
			while(true)
			{
				int end = tmp.find("\r\n",start);
				if(end == string::npos){
					rmd = tmp.substr(start,pkglen-start);				
					break;
				}
				priceStr = tmp.substr(start,end-start);
				
				if(start == 0)
					priceStr = rmd+priceStr;
				//cout<<priceStr<<endl;
				
				if(priceStr.find("4107,3",0)!=string::npos && quoteEvent!=NULL)
				{
					cout<<"Quote is added"<<endl;
					SetEvent(quoteEvent);
				}
				else if(priceStr.find("4108,3",0)!=string::npos &&  quoteEvent!=NULL)
				{
					cout<<"Quote is deleted"<<endl;
					SetEvent(quoteEvent);
				}
				else if(priceStr.find("4102,0",0) != string::npos)
				{
					PriceItem* pi = str2PriceItem(priceStr);	
					//LogHandler::getLogHandler().log(priceStr);
					//pi->log();
					confirmPriceInfo(pi->quoteId);
					dispatcher->forwardPrice(pi);
				}
				start = end +2;
			}
		}
	}
}

void SPOrderInterface::processOrder()
{
	char recvbuff[200]="";
	int pkglen=0;
	string rmd="", orderStr = "", tmp;
	char* pstr = NULL;
	while(true){
		if(connectStatus==true){
			recvbuff[0]='\0';
			pkglen = recv(orderSocket,recvbuff,sizeof(recvbuff),0);
			if(pkglen<0){
				cout<<"error occured when receive data from order socket!"<<endl;
				//exit(-1);
			}
			else
			{
				int start = 0;
				orderStr = "";
				tmp = recvbuff;
				//cout<<buff<<endl;
				while(true){
					int end = tmp.find("\r\n",start);
					if(end == string::npos){
						rmd = tmp.substr(start,pkglen-start);
						//cout<<"*******************************"<<endl;
						//cout<<rmd<<endl;					
						break;
					}
					orderStr = tmp.substr(start,end-start);
					if(start == 0)
						orderStr = rmd+orderStr;					

					/** decode ordermsg string to an order instance and forward to OrderItem Dispatcher Thread **/
					if(orderStr.find("3103,3",0)!=string::npos) {
						OrderItem* po = str2OrderItem(orderStr);
						// 接收订单ORDER_ACCEPT_MSG
						dispatcher->returnOrder(po);
					}
					else if(orderStr.find("3109,0",0) != string::npos) {
						TradeItem* ti = str2TradeItem(orderStr);
						confirmTradeInfo(ti->getTradeNo());
						// 订单完成TRADE_DONE_MSG
						dispatcher->returnTrade(ti);
					}
					//by xie
					else if(orderStr.find("9000",0) != string::npos){
						char linkStateReply[256] = {0};
						strncpy(linkStateReply, orderStr.c_str(), 256);

						// 发送返回信息到连接状态检测线程
						PostThreadMessage(checkConnectionThreadId, LINK_STATE_MSG, 0, (LPARAM)linkStateReply);
					}
					start = end +2;
				}
			}
		}	
		else
		{
			cout<<"Connection to SPTrader is not established or broken!"<<endl;
			//exit(-1);
		}
	}	
}

void SPOrderInterface::processTickerMessage()
{
	char recvbuff[200] = {0};
	int pkglen = 0;
	char* pstr = NULL;
	while(true){
		if(connectStatus==true){
			recvbuff[0]='\0';
			pkglen = recv(tickSocket, recvbuff, sizeof(recvbuff), 0);
			if(pkglen < 0){
				LogHandler::getLogHandler().alert(3, "Ticker error", "error occured when receive data from order socket!");
			} else if ((pstr = strstr(recvbuff, "5107,3")) != NULL) {
				pstr = strtok(recvbuff, ",");
				pstr = strtok(NULL, ",");
				if (pstr == NULL) continue;
				pstr = strtok(NULL, ",");
				if (pstr == NULL) continue;
				int retCode = atoi(pstr);
				pstr = strtok(NULL, ",");
				if (pstr == NULL) continue;
				if (retCode == 0) {
					LogHandler::getLogHandler().log(string(pstr) + " ticker request sucessfully!");
				} else {
					// todo: resend
					//this->addQuote(pstr);
					LogHandler::getLogHandler().alert(3, "Ticker error", string(pstr) + " ticker request failed");
				}
			} else if ((pstr = strstr(recvbuff, "5108,3")) != NULL) {
				pstr = strtok(recvbuff, ",");
				pstr = strtok(NULL, ",");
				if (pstr == NULL) continue;
				pstr = strtok(NULL, ",");
				if (pstr == NULL) continue;
				int retCode = atoi(pstr);
				pstr = strtok(NULL, ",");
				if (pstr == NULL) continue;
				if (retCode == 0) {
					LogHandler::getLogHandler().log(string(pstr) + " release ticker request sucessfully!");
				} else {
					// todo: resend
					//this->addQuote(pstr);
					LogHandler::getLogHandler().alert(3, "Ticker error", string(pstr) + " release ticker request failed");
				}
			} else if ((pstr = strstr(recvbuff, "5102,3")) != NULL) {
				pstr = strtok(recvbuff, ",");
				pstr = strtok(NULL, ",");
				if (pstr == NULL) continue;
				pstr = strtok(NULL, ",");
				if (pstr == NULL) continue;
				PriceItem* pi = new PriceItem();
				pi->quoteId = pstr;
				pstr = strtok(NULL, ",");
				if (pstr == NULL) {
					delete pi;
					continue;
				}
				pi->lastPrice1 = atof(pstr);
				pstr = strtok(NULL, ",");
				if (pstr == NULL) {
					delete pi;
					continue;
				}
				pi->lastQty1 = atoi(pstr);
				pstr = strtok(NULL, ",");
				if (pstr == NULL) {
					delete pi;
					continue;
				}
				pi->currentTime = atoi(pstr);
				pi->tradePlatform = SPTRADER;
				if (dispatcher != NULL) {
					dispatcher->forwardTickPrice(pi);
				}
			}
		}
	}
}
int SPOrderInterface::confirmTradeInfo(int tradeRecordNo)
{
	int len=0;
	char confirmStr[50];
	if(connectStatus==true){		
		sprintf(confirmStr,"3109,3,%d\r\n",tradeRecordNo);				
		if((len=send(orderSocket,confirmStr,strlen(confirmStr),0))<0){
			cout<<"Send Trade confirmation error, please check the order socket!"<<endl;
			//exit(-1);
		}
	}
	else
	{
		cout<<"Connection to SPTrader is not established or broken!"<<endl;
		//exit(-1);
	}
	return len;

}

PriceItem* SPOrderInterface::str2PriceItem(string priceStr)
{
	PriceItem* pi = new PriceItem();
	pi->tradePlatform = SPTRADER;
	char elem[30];
	char* pelem = &priceStr[0];
	char* delims = pelem;
	int c = 1, strcount=0;
	for(int i = 0; i <= priceStr.length(); i++)
	{
		if(*delims == ',' || *delims == '\0' )
		{
			memcpy(elem,pelem, strcount);	
			elem[strcount]='\0';
			
			if(c==3 && strcount>0) pi->quoteId = elem;
			else if(c==4 && strcount>0) pi->quoteName = elem;
			else if(c==5 && strcount>0) pi->qouteType = atoi(elem);
			else if(c==6 && strcount>0) pi->contractSize = atoi(elem);
			else if(c==7 && strcount>0) pi->ExpiryDate = atoi(elem);
			else if(c==8 && strcount>0) pi->instrumentCode = elem;
			else if(c==9 && strcount>0) pi->currency = elem;
			else if(c==10 && strcount>0) pi->strike = atof(elem);
			else if(c==11 && strcount>0) pi->callPut = elem[0];
			else if(c==12 && strcount>0) pi->underlying = elem;
			else if(c==13 && strcount>0) pi->bidPrice1 = atof(elem);
			else if(c==14 && strcount>0) pi->bidQty1 = atoi(elem);
			else if(c==15 && strcount>0) pi->bidPrice2 = atof(elem);
			else if(c==16 && strcount>0) pi->bidQty2 = atoi(elem);
			else if(c==17 && strcount>0) pi->bidPrice3 = atof(elem);
			else if(c==18 && strcount>0) pi->bidQty3 = atoi(elem);
			else if(c==19 && strcount>0) pi->bidPrice4 = atof(elem);
			else if(c==20 && strcount>0) pi->bidQty4 = atoi(elem);
			else if(c==21 && strcount>0) pi->bidPrice5 = atof(elem);
			else if(c==22 && strcount>0) pi->bidQty5 = atoi(elem);
			else if(c==23 && strcount>0) pi->askPrice1 = atof(elem);
			else if(c==24 && strcount>0) pi->askQty1 = atoi(elem);
			else if(c==25 && strcount>0) pi->askPrice2 = atof(elem);
			else if(c==26 && strcount>0) pi->askQty2 = atoi(elem);
			else if(c==27 && strcount>0) pi->askPrice3 = atof(elem);
			else if(c==28 && strcount>0) pi->askQty3 = atoi(elem);
			else if(c==29 && strcount>0) pi->askPrice4 = atof(elem);
			else if(c==30 && strcount>0) pi->askQty4 = atoi(elem);
			else if(c==31 && strcount>0) pi->askPrice5 = atof(elem);
			else if(c==32 && strcount>0) pi->askQty5 = atoi(elem);
			else if(c==33 && strcount>0) pi->lastPrice1 = atof(elem);
			else if(c==34 && strcount>0) pi->lastQty1 = atoi(elem);
			else if(c==35 && strcount>0) pi->lastPrice2 = atof(elem);
			else if(c==36 && strcount>0) pi->lastQty2 = atoi(elem);
			else if(c==37 && strcount>0) pi->lastPrice3 = atof(elem);
			else if(c==38 && strcount>0) pi->lastQty3 = atoi(elem);
			else if(c==39 && strcount>0) pi->lastPrice4 = atof(elem);
			else if(c==40 && strcount>0) pi->lastQty4 = atoi(elem);
			else if(c==41 && strcount>0) pi->lastPrice5 = atof(elem);
			else if(c==42 && strcount>0) pi->lastQty5 = atoi(elem);
			else if(c==43 && strcount>0) pi->openInterest = atoi(elem);
			else if(c==44 && strcount>0) pi->turnOverAmount = atof(elem);
			else if(c==45 && strcount>0) pi->turnOverVolume = atoi(elem);
			else if(c==46 && strcount>0) pi->equilibriumPrice = atof(elem);
			else if(c==47 && strcount>0) pi->openD = atof(elem);
			else if(c==48 && strcount>0) pi->highD = atof(elem);
			else if(c==49 && strcount>0) pi->lowD = atof(elem);
			else if(c==50 && strcount>0) pi->previousClose = atof(elem);
			else if(c==51 && strcount>0) pi->previousCloseDate = atof(elem);
			else if(c==53 && strcount>0) pi->tradeStateNo = atof(elem);

			delims++;
			pelem = delims;
			strcount = 0;
			c++;
		}
		else
		{
			strcount++;
			delims++;
		}
		
	}
	return pi;
}

TradeItem* SPOrderInterface::str2TradeItem(string TradeStr)
{
	TradeItem* ti = new TradeItem();
	ti->setTradePlatform(SPTRADER);
	char elem[30];
	char* pelem = &TradeStr[0];
	char* delims = pelem;
	int c = 1, strcount=0;
	
	for(int i = 0; i <=TradeStr.length(); i++)
	{
		if(*delims == ',' || *delims == '\0')
		{
			//memset(tmp,0,count+1);
			strncpy(elem,pelem, strcount);	
			elem[strcount]='\0';
			if(c==3 && strcount>0) ti->setTradeRecordNo(atoi(elem));
			else if(c==4 && strcount>0) ti->setAccount(elem);
			else if(c==5 && strcount>0) ti->setOrderNo(atol(elem));
			else if(c==6 && strcount>0) ti->setQuoteId(elem);
			else if(c==7 && strcount>0) ti->setBuySell(elem[0]);
			else if(c==8 && strcount>0) ti->setTradePrice(atof(elem));
			else if(c==9 && strcount>0) ti->setQty(atoi(elem));							
			else if(c==10 && strcount>0) ti->setOpenClose(atoi(elem));
			else if(c==11 && strcount>0) 
			{
				char tmp2[100]= "";
				char *elem2 = NULL;
				char *delims2 = ":";
				strcpy(tmp2,elem);
				elem2 = strtok(tmp2,delims2);
				elem2 = strtok(NULL,delims2);
				ti->setTraderId(atoi(elem2));
				elem2 = strtok(NULL,delims2);
				ti->setOrderRefId(atol(elem2));
			}
			else if(c==14 && strcount>0) ti->setTradeNo(atoi(elem));
			else if(c==15 && strcount>0) ti->setStatus(atoi(elem));
			else if(c==16 && strcount>0) ti->setPositionSize(atoi(elem));			
			else if(c==17 && strcount>0) ti->setTradeTime(atol(elem));	
			delims++;
			pelem = delims;
			strcount = 0;
			c++;
		}
		else
		{
			strcount++;
			delims++;
		}

	}
	return ti;
}

OrderItem* SPOrderInterface::str2OrderItem(string orderStr)
{	
	OrderItem* oi = new OrderItem();
	oi->setTradePlatform(SPTRADER);
	char elem[30];
	char* pelem = &orderStr[0];
	char* delims = pelem;
	int c = 1, strcount=0;	
	for(int i = 0; i <= orderStr.length(); i++)
	{
		if(*delims == ',' || *delims == '\0')
		{
			strncpy(elem,pelem, strcount);	
			elem[strcount]='\0';
			if(c==3 && elem[0]!='0')
			{
				//cout<<"order is not correctly processed by server! Maybe the order has been deleted or traded."<<endl;
				LogHandler::getLogHandler()
								.alert(1, "", "order is not correctly processed by server! Maybe the order has been deleted or traded.(" + orderStr + ")");
				//oi->log();
				break;
			}
			/**  decode ordermsg string to an order instance ***/
			if(c==3 && strcount>0) oi->setReturnCode(atoi(elem));
			else if(c==4 && strcount>0) oi->setReturnMessage(elem);
			else if(c==5 && strcount>0) oi->setStatus(atoi(elem));
			else if(c==6 && strcount>0) oi->setAction(atoi(elem));
			else if(c==7 && strcount>0) oi->setAccount(elem);
			else if(c==8 && strcount>0) oi-> setOrderNo(atoi(elem));
			else if(c==9 && strcount>0) oi->setQuoteId(elem);
			else if(c==10 && strcount>0) oi->setBuySell(elem[0]);
			else if(c==11 && strcount>0) oi->setSubmitPrice(atof(elem));
			else if(c==12 && strcount>0) oi->setQty(atof(elem));		
			else if(c==13 && strcount>0) oi->setOpenClose(elem[0]);		
			else if(c==15 && strcount>0) oi->setValidType(atoi(elem));
			else if(c==17 && strcount>0)
			{
				char tmp2[20]= "";
				char *elem2 = NULL;
				char *delims2 = ":";
				strcpy(tmp2,elem);
				elem2 = strtok(tmp2,delims2);
				elem2 = strtok(NULL,delims2);
				oi->setTraderId(atoi(elem2));
				elem2 = strtok(NULL,delims2);
				oi->setOrderRefId(atol(elem2));
			}
			else if(c==18 && strcount>0) oi->setOriginalPrice(atof(elem));		
			else if(c==19 && strcount>0) oi->setOriginalQty(atof(elem));
			delims++;
			pelem = delims;
			strcount = 0;
			c++;
		}
		else
		{
			strcount++;
			delims++;		
		}		
	}
	return oi;
}

string DoubleToString(double d)
{
    string str;
    stringstream ss;
    ss<<d;
    ss>>str;
    return str;
}

string SPOrderInterface::orderItem2Str(OrderItem* po)
{
	char buff[20];
	string orderStr = string("3103,0,")+itoa(po->getAction(),buff,RADIX)+","+accountNo+","+itoa(po->getOrderNo(),buff,RADIX)+",";
	orderStr += po->getQuoteId()+","+string(1,po->getBuySell())+","+DoubleToString(po->getSubmitPrice())+",";
	orderStr += DoubleToString(po->getQty())+","+string(1,po->getOpenClose())+",0,";
	orderStr += (string)itoa(po->getValidType(),buff,RADIX)+",,"+(string)PROGRAM_NAME+":";
	orderStr += (string)itoa(po->getTraderId(), buff, RADIX)+":";
	orderStr += (string)ltoa(po->getOrderRefId(),buff,RADIX)+"\r\n";
		//cout<<orderStr<<endl;

	return orderStr;
}

void SPOrderInterface::setDispatcher(Dispatcher* dispatcher)
{
	this->dispatcher = dispatcher;
}

void SPOrderInterface::stopOrderThread()
{
	if(TerminateThread(hOrderThread,0) == 0){
		cout<<"Terminate SPTrader's receive order thread failed, ERROR CODE="+GetLastError()<<endl;
	}
	WaitForSingleObject(hOrderThread, INFINITE);
	CloseHandle(hOrderThread);
	hOrderThread = INVALID_HANDLE_VALUE;
}

SPOrderInterface::SPOrderInterface():orderPort(8092), pricePort(8089), tickPort(8090), timerInterval(3)
{
	hOrderThread = INVALID_HANDLE_VALUE;
	connectStatus = false;
}

int SPOrderInterface::initPriceConnection()
{
	cout<<"begin price connection......"<<endl;
	priceSocket = socket(AF_INET, SOCK_STREAM,0);
	if(INVALID_SOCKET == priceSocket){
		cout<<"Creating price socket  failed, Exit!"<<endl;
		return -1;
	}
	sockaddr_in addr;
	memset(&addr,0,sizeof(addr));
	addr.sin_family=AF_INET;
	
	addr.sin_addr.s_addr = inet_addr(server.c_str());
	addr.sin_port=htons(pricePort);

	if(SOCKET_ERROR==connect(priceSocket,(sockaddr*)&addr,sizeof(addr))){
		cout<<"connecting to price 8089 port failed, exit!"<<endl;
		closesocket(priceSocket);
		return -1;
	}
	else{
		cout<<"establish price connection  successfully!"<<endl;
		return 0;
	}
}

SPOrderInterface& SPOrderInterface::startSPTrader(string server, string account_no, string password, 
	int orderPort, int pricePort, int tickPort)
{
	static SPOrderInterface sptrader;
	sptrader.login(server,account_no,password, orderPort, pricePort, tickPort);
	sptrader.startOrderThread();
	sptrader.startPriceThread();
	sptrader.startCheckConnectionThread();
	sptrader.startTickerThread();
	//Sleep(3000);
	return sptrader;
}

int SPOrderInterface::initTickConnection()
{
	cout<<"begin tick connection......"<<endl;
	tickSocket = socket(AF_INET, SOCK_STREAM,0);
	if(INVALID_SOCKET == tickSocket){
		cout<<"Creating price socket  failed, Exit!"<<endl;
		return -1;
	}
	sockaddr_in addr;
	memset(&addr,0,sizeof(addr));
	addr.sin_family=AF_INET;
	
	addr.sin_addr.s_addr = inet_addr(server.c_str());
	addr.sin_port=htons(tickPort);

	if(SOCKET_ERROR==connect(tickSocket,(sockaddr*)&addr,sizeof(addr))){
		//cout<<"connecting to price 8089 port failed, exit!"<<endl;
		LogHandler::getLogHandler().alert(3, "Socket error", "connecting to ticker port failed, exit!");
		closesocket(tickSocket);
		return -1;
	}
	else{
		LogHandler::getLogHandler().log("establish tick connection  successfully!");
		return 0;
	}
	/*if((len=send(tickerSocket,orderStr.c_str(),orderStr.length(),0))<0){
			cout<<"Send ORDER orders error, please check the order socket!"<<endl;
			//exit(-1);
	}*/
}

int SPOrderInterface::addQuote(string quoteId)
{
	quoteEvent = CreateEvent(NULL,FALSE,FALSE,L"ADD_QUOTE");
	string quotemsg =  "4107,0,"+quoteId+"\r\n";		  // 4107 msg is to request the price 
	if(send(priceSocket,quotemsg.c_str(),quotemsg.length(),0)<0)
	{
		cout<<"Send price update msgid error, please check the connection to server!"<<endl;
		return MY_ERROR;
	}
	// added by xie
	quotemsg =  "5107,0,"+quoteId+"\r\n";		  // 5107 msg is to request the price 
	// 这里为什么socket 会还是0呢？线程应该早就启动了
	if(send(tickSocket,quotemsg.c_str(),quotemsg.length(),0)<0)
	{
		LogHandler::getLogHandler().alert(3, "Add quote", "Request tick price failed for new quote!");
		return MY_ERROR;
	}
	WaitForSingleObject(quoteEvent,INFINITE);
	CloseHandle(quoteEvent);
	return SUCCESS;
}
int SPOrderInterface::deleteQuote(string quoteId)
{
	quoteEvent = CreateEvent(NULL,FALSE,FALSE,L"DELETE_QUOTE");
	string quotemsg =string("4108,0,")+quoteId+"\r\n";  // 4108 msg is to stop the price update
	if(send(priceSocket,quotemsg.c_str(),quotemsg.length(),0)<0)
	{
		cout<<"Send price release msgid error, please check the connection to server!"<<endl;
		return -1;
	}

	// added by xie
	quotemsg =string("5108,0,")+quoteId+"\r\n";  // 4108 msg is to stop the price update
	if(send(tickSocket,quotemsg.c_str(),quotemsg.length(),0)<0)
	{
		LogHandler::getLogHandler().alert(3, "Delete quote", "Cancel tick price failed!");
		return -1;
	}
	WaitForSingleObject(quoteEvent,INFINITE);
	CloseHandle(quoteEvent);
	return 0;
}

int SPOrderInterface::confirmPriceInfo(string quoteId)
{
	string priceRep = "";
	priceRep = "4102,3,"+quoteId+"\r\n";				
	//cout<<pricereply<<endl;
	if(send(priceSocket,priceRep.c_str(),priceRep.length(),0)<0)
	{
		cout<<"Send price reply error, please check the connection to server!"<<endl;
		return -1;
	}
	return 0;
}

bool SPOrderInterface::isSupport(int orderType)
{
	if(orderType== LMT)
		return true;
	else
		return false;
}