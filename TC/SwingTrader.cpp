#include "QuoteItem.h"
#include "PriceItem.h"
#include "TradeItem.h"
#include "OrderItem.h"
#include "Position.h"
#include "TradeUnit.h"
#include "SwingTrader.h"
#include "Dispatcher.h"
#include "TradeCube.h"
#include "Strategy.h"
#include "Bar.h"
#include <tchar.h>

SwingTrader::SwingTrader(int traderId)
{
	this->traderId = traderId;
	hTraderThread = NULL;
	tradeUnit = NULL;
	dispatcher = NULL;
	strategy = NULL;
	optimizeOrderFlow = NO_OPTIMIZE_OFP; 
	ascOrderRefId = 0;
}

DWORD WINAPI SwingTrader::traderThreadAdapter(LPVOID lpParam)
{
	SwingTrader *pt = (SwingTrader *)lpParam;
	pt->process();
	return 0; 
}

DWORD SwingTrader::startTraderThread()
{
      CreateThread(NULL, 0, traderThreadAdapter, this, 0, &TraderThreadId);
	return TraderThreadId;
}

void SwingTrader::process()
{
	MSG msg;
	while(GetMessage(&msg,NULL,WM_USER+1, WM_USER+4))
	{
		switch(msg.message)
		{
			// 接收订单
		case ORDER_ACCEPT_MSG:
			{
				OrderItem* new_oi = (OrderItem*)msg.lParam;
				OrderItem* ori_oi = tradeUnit->getOrder(new_oi->getOrderRefId());
				// 为什么是在队列中查找？
				if(ori_oi != NULL)
				{
					ori_oi->setOrderNo(new_oi->getOrderNo());
					ori_oi->setReturnCode(new_oi->getReturnCode());
					ori_oi->setReturnMessage(new_oi->getReturnMessage());
					ori_oi->setStatus(new_oi->getStatus());
				}
				// 哪里发送订单？？？
				else
				{
					//MessageBox(NULL,_T("Returned Order from server is not found in local order queues"),_T("Process ORDER_ACCEPT_MSG error"),MB_OK|MB_ICONSTOP);
				}		
				delete new_oi;
				break;
			}
			//订单成交
		case TRADE_DONE_MSG:
			{
				TradeItem* ti = (TradeItem*)msg.lParam;
				if(tradeUnit != NULL)
				{
					OrderItem* oi = tradeUnit->getOrder(ti->getOrderRefId());

					if(oi != NULL)
					{
						if(ti->getStatus()!=DELETED)
						{							
							Position* cpos = tradeUnit->getPosition(0);
							if(cpos != NULL)
							{
								if(cpos->getSize()==0 && ti->getPositionSize()!=0) //  open a position
								{
									Position* pos = new Position();
									pos->setQuoteId(ti->getQuoteId());
									pos->setSize(ti->getPositionSize());
									pos->setLongShort(ti->getPositionSize()>0?1:-1);
									pos->setTradePlatform(ti->getTradePlatform());
									pos->getTradeList().push_front(ti);
									tradeUnit->addPosition(pos);
								}
								else if(cpos->getSize() > 0)  // current position is long
								{
									if(ti->getPositionSize() > cpos->getSize())  // add long position size
									{
										cpos->setSize(ti->getPositionSize());
										cpos->getTradeList().push_front(ti);
									}
									else if(ti->getPositionSize() < cpos->getSize() && ti->getPositionSize()>=0) // close some long position
									{
										cpos->setSize(ti->getPositionSize());
										cpos->getTradeList().push_front(ti);
									}
									else if(ti->getPositionSize()<0) // reverse position
									{
										Position* pos = new Position();
										pos->setQuoteId(ti->getQuoteId());
										pos->setSize(ti->getPositionSize());
										pos->setLongShort(ti->getPositionSize()>0?1:-1);
										pos->setTradePlatform(ti->getTradePlatform());
										pos->getTradeList().push_front(ti);
										tradeUnit->addPosition(pos);
									}
								}
								else if(cpos->getSize() < 0)
								{
									if(ti->getPositionSize() < cpos->getSize())
									{
										cpos->setSize(ti->getPositionSize());
										cpos->getTradeList().push_front(ti);
									}
									else if(ti->getPositionSize() > cpos->getSize() && ti->getPositionSize()<=0)
									{
										cpos->setSize(ti->getPositionSize());
										cpos->getTradeList().push_front(ti);
									}
									else if(ti->getPositionSize()>0)
									{
										Position* pos = new Position();
										pos->setQuoteId(ti->getQuoteId());
										pos->setSize(ti->getPositionSize());
										pos->setLongShort(ti->getBuySell()==BUY?1:-1);
										pos->setTradePlatform(ti->getTradePlatform());
										pos->getTradeList().push_front(ti);
										tradeUnit->addPosition(pos);
									}
								}
							}
							else
							{
								Position* pos = new Position();
								pos->setQuoteId(ti->getQuoteId());
								pos->setSize(ti->getPositionSize());
								pos->setLongShort(ti->getBuySell()==BUY?1:-1);
								pos->setTradePlatform(ti->getTradePlatform());
								pos->getTradeList().push_front(ti);
								tradeUnit->addPosition(pos);
							}
							tradeUnit->addTrade(ti);
						}
						
						

						if(optimizeOrderFlow==OPTIMIZE_OFP && oi->getParentRefId() != 0)
						{
							OrderItem* parent_oi = tradeUnit->getOrder(oi->getParentRefId());
							if(parent_oi != NULL)
							{
								parent_oi->addTradedQty(ti->getQty());
								if(parent_oi->getTradedQty() == parent_oi->getQty())		
								{
									tradeUnit->deleteOrder(parent_oi->getOrderRefId());
									parent_oi->setStatus(ALLTRADED);
								}
								else
									parent_oi->setStatus(PARTIALTRADED);
							}
						}
						else if(optimizeOrderFlow==NO_OPTIMIZE_OFP && oi->getParentRefId() == 0)
						{
							oi->addTradedQty(ti->getQty());
							if(oi->getTradedQty()==oi->getQty())
							{
								oi->setStatus(ALLTRADED);
								tradeUnit->deleteOrder(oi->getOrderRefId());
							}
							else
								oi->setStatus(PARTIALTRADED);
						}
					}
					else
					{
						//MessageBox(NULL,_T("Done trade is not found in local order queues"),_T("Process TRADE_DONE_MSG error"),MB_OK|MB_ICONSTOP);
					}
				}
				break;
			}
			//价格更新k
		case PRICE_MSG:
			{
				PriceItem* pi = (PriceItem*)msg.lParam;			
				if(tradeUnit != NULL)
				{
					tradeUnit->updatePrice(pi);						
					triggerWaitingOrder();

					// k线内交易
					if(strategy!=NULL && strategy->getAutoTrading() && strategy->getIntraBarTrading())
					{
						strategy->addCounter();
						strategy->signal();
						deleteStrategyOrder();
					}
				}
				else
				{
					//MessageBox(NULL,_T("Trade Unit is not found for price message"),_T("Process PRICE_MSG error"),MB_OK|MB_ICONSTOP);
				}
				break;
			}
			// 定时器 策略执行
		case STRATEGY_MSG:
			{
				if(strategy!=NULL && strategy->getAutoTrading() && !strategy->getIntraBarTrading())
					{
						strategy->addCounter();
						strategy->signal();
						deleteStrategyOrder();
					}
				break;
			}
		}
	}
}

int SwingTrader::deleteOrder(long orderRefId)
{
	OrderItem* oi = tradeUnit->getOrder(orderRefId);
	if(oi != NULL)
	{
		if(dispatcher->isSupport(oi->getTradePlatform(), oi->getOrderType()))
		{
			oi->setAction(DEL_ACTION);
			oi->setStatus(DELETING);
			dispatcher->submitOrder(oi);
		}
		else
		{
			tradeUnit->deleteOrder(orderRefId);
		}
	}
	return SUCCESS;
}

int SwingTrader::setTradeUnit(TradeUnit* tradeUnit)
{
	int flag = -1;

	if(dispatcher!=NULL && dispatcher->addQuote(tradeUnit->getQuote()))
	{
		this->tradeUnit = tradeUnit;
		flag = 0;
	}
	return flag;
}

void SwingTrader::setDispatcher(Dispatcher* dispatcher)
{
	this->dispatcher = dispatcher;
}

int SwingTrader::deleteTradeUnit()
{
	/** Clear all order and save trade record before delete the quote**/
	dispatcher->deleteQuote(tradeUnit->getQuote());
	delete tradeUnit;
	tradeUnit = NULL;
	return 0;
}


long SwingTrader::createOrder(char buysell, char openclose, double submitPrice, double qty, int orderType, int validType, int submitter)
{	
	int minqty = tradeUnit->getQuote()->getMinContractQty();
	if(!double_divide(qty, minqty))
	{
		//MessageBox(NULL,_T("The quantity is not the integer multiples of the unit contract"),_T("Create Order Error"),MB_OK|MB_ICONSTOP);
		return MY_ERROR;
	}

	OrderItem* oi = new OrderItem(tradeUnit->getQuote()->getTradePlatform(), tradeUnit->getQuoteId(), submitPrice, qty, buysell, orderType, validType, openclose);
	oi->setOrderRefId(ascOrderRefId++);
	oi->setSubmitter(submitter);
	oi->setStatus(ADDING);
	oi->setTraderId(traderId);
	if(orderType == MKT) // Market order
	{						
		if(buysell==BUY)
			oi->setSubmitPrice(tradeUnit->getPrice()->askPrice1);
		else if(buysell==SELL)
			oi->setSubmitPrice(tradeUnit->getPrice()->bidPrice1);

		// decompose large order into small ones to decrease impact cost
		if(optimizeOrderFlow==OPTIMIZE_OFP && qty>tradeUnit->getQuote()->getMinContractQty())
		{
			tradeUnit->addOrder(oi);
			decomposeOrder(oi);
		}
		else
		{
			oi->setAction(ADD_ACTION);
			tradeUnit->addOrder(oi);
			dispatcher->submitOrder(oi);
		}
	}
	else if(orderType != MKT ) 
	{		
		if(dispatcher->isSupport(tradeUnit->getTradePlatform(), orderType)) //Limit order
		{
			oi->setAction(ADD_ACTION);
			oi->setStatus(ADDING);
			tradeUnit->addOrder(oi);
			dispatcher->submitOrder(oi);

		}
		else
		{
			oi->setAction(WAITING);
			oi->setStatus(WAITING);
			tradeUnit->addOrder(oi);
		}
	}	

	return oi->getOrderRefId();
}


long SwingTrader::updateOrder( long orderRefId, char buysell, char openclose, double submitPrice, double qty, int validType)
{
	OrderItem* oi = tradeUnit->getOrder(orderRefId);
	oi->addCounter();
	int tradePlatform = tradeUnit->getTradePlatform();
	if(oi != NULL)
	{
		if(dispatcher->isSupport(tradePlatform, oi->getOrderType()))
		{				
			oi->setValidType(validType);
			oi->setBuySell(buysell);
			oi->setQty(qty);
			oi->setOpenClose(openclose);
			if(oi->getOrderType() != MKT)
			{
				oi->setStatus(CHANGING);
				oi->setAction(CHG_ACTION);
				oi->setSubmitPrice(submitPrice);
				dispatcher->submitOrder(oi); // transimit order 
			}

		}
		else if(!dispatcher->isSupport(tradePlatform, oi->getOrderType()))// tradeplatform don't support new ordertype
		{
			if(oi->getOrderType() == MKT && oi->getParentRefId()!=0)
			{				
				oi->setBuySell(buysell);
				oi->setQty(qty);
				oi->setOpenClose(openclose);
				oi->setStatus(CHANGING);
				oi->setAction(CHG_ACTION);
				if(buysell==BUY)
					oi->setSubmitPrice(tradeUnit->getPrice()->askPrice1);
				else if(buysell==SELL)
					oi->setSubmitPrice(tradeUnit->getPrice()->bidPrice1);
				dispatcher->submitOrder(oi); // transimit order 
			}  // tradeplatform don't support original ordertype				
			else	
			{
				oi->setValidType(validType);
				oi->setBuySell(buysell);
				oi->setQty(qty);
				oi->setOpenClose(openclose);
				oi->setSubmitPrice(submitPrice);			
			}					
		}
	}
	return orderRefId;
}



// 分解订单
int SwingTrader::decomposeOrder(OrderItem* poi)
{
	if(poi->getOrderType() == MKT)
	{
		int minqty = tradeUnit->getQuote()->getMinContractQty();
		for(int i = minqty; i <=poi->getQty(); i+=minqty)
		{
			OrderItem* oiu = new OrderItem(poi->getTradePlatform(), poi->getQuoteId(), poi->getSubmitPrice(),minqty, poi->getBuySell(), poi->getOrderType(), poi->getValidType(),poi->getOpenClose());
			oiu->setOrderRefId(ascOrderRefId++);
			oiu->setParentRefId(poi->getOrderRefId());
			oiu->setStatus(ADDING);
			oiu->setAction(ADD_ACTION);		
			oiu->setTraderId(traderId);
			PriceItem* prc = tradeUnit->getPrice();
			
			if(poi->getBuySell()==BUY)
			{
				oiu->setOriginalPrice(prc->askPrice1);
				if(i<=prc->askQty1)
					oiu->setSubmitPrice(prc->askPrice1);
				else if(i<=prc->askQty1+prc->askQty2)
					oiu->setSubmitPrice(prc->askPrice2);
				else if(i<=prc->askQty1+prc->askQty2+prc->askQty3)
					oiu->setSubmitPrice(prc->askPrice3);
				else if(i<=prc->askQty1+prc->askQty2+prc->askQty3+prc->askQty4)
					oiu->setSubmitPrice(prc->askPrice4);
				else if(i<=prc->askQty1+prc->askQty2+prc->askQty3+prc->askQty4+prc->askQty5)
					oiu->setSubmitPrice(prc->askPrice5);
				else
					oiu->setSubmitPrice(prc->askPrice5+tradeUnit->getQuote()->getPriceScale());
			}
			else if(poi->getBuySell()==SELL)
			{
				oiu->setOriginalPrice(prc->bidPrice1);
				if(i<=prc->bidQty1)
					oiu->setSubmitPrice(prc->bidPrice1);
				else if(i<=prc->bidQty1+prc->bidQty2)
					oiu->setSubmitPrice(prc->bidPrice2);
				else if(i<=prc->bidQty1+prc->bidQty2+prc->bidQty3)
					oiu->setSubmitPrice(prc->bidPrice3);
				else if(i<=prc->bidQty1+prc->bidQty2+prc->bidQty3+prc->bidQty4)
					oiu->setSubmitPrice(prc->bidPrice4);
				else if(i<=prc->bidQty1+prc->bidQty2+prc->bidQty3+prc->bidQty4+prc->bidQty5)
					oiu->setSubmitPrice(prc->bidPrice5);
				else
					oiu->setSubmitPrice(prc->bidPrice5-tradeUnit->getQuote()->getPriceScale());
			}
			tradeUnit->addOrder(oiu);
			if(dispatcher->submitOrder(oiu)==MY_ERROR)
				return MY_ERROR;
		}
		return SUCCESS;
	}
	else
	{
		//MessageBox(NULL,_T("Current version support only for optimize MKT order flow"),_T("Decompose Order Error"),MB_OK|MB_ICONSTOP);
	}
}

void SwingTrader::deleteStrategyOrder()
{
	map<long,OrderItem*>::iterator iter;
	for(iter=tradeUnit->getOrderQueue().begin(); iter != tradeUnit->getOrderQueue().end(); iter++)
	{
		OrderItem* oi = iter->second;
		/* if order's counter is not equal to strategy's counter, indicating this order is not update at the last round execute */
		if(oi->getOrderType() != MKT && oi->getCounter() != strategy->getCounter()) 
		{
			deleteOrder(oi->getOrderRefId());
		}
		
	}
}

long SwingTrader::buy(long& orderRefId, string quoteId, double submitPrice, double qty, int orderType, int validType, int submitter)
{
	if(orderRefId == 0)
		return createOrder(BUY, OPEN, submitPrice, qty, orderType, validType, submitter);
	else
		return updateOrder(orderRefId,BUY, OPEN, submitPrice, qty, validType);
}

long SwingTrader::sellshort(long& orderRefId, string quoteId, double submitPrice, double qty, int orderType, int validType, int submitter)
{
	if(orderRefId == 0)
		return createOrder(SELL, OPEN, submitPrice, qty, orderType, validType, submitter);
	else
		return updateOrder(orderRefId,SELL, OPEN, submitPrice, qty, validType);
}

long SwingTrader::sell(long& orderRefId, string quoteId, double submitPrice, double qty, int orderType, int validType, int submitter)
{
	if(orderRefId == 0 )
		return createOrder(SELL, CLOSE, submitPrice, qty, orderType, validType, submitter);
	else
		return updateOrder(orderRefId, SELL,CLOSE, submitPrice, qty, validType);
}

long SwingTrader::buytocover(long& orderRefId, string quoteId, double submitPrice, double qty, int orderType, int validType, int submitter)
{
	if(orderRefId == 0 )
		return createOrder(BUY,CLOSE, submitPrice, qty, orderType, validType, submitter);
	else
		return updateOrder(orderRefId, BUY,CLOSE, submitPrice, qty, validType);
}

void SwingTrader::setOptimizeOrderFlow(int orderFlow)
{
	this->optimizeOrderFlow = orderFlow;
}

double SwingTrader::marketposition(int pos_ago)
{
	Position* pos = tradeUnit->getPosition(pos_ago);
	if(pos != NULL)
	{
		if(pos_ago == 0)
			return pos->getSize();
		else
			return pos->getLongShort();
	
	}
	else
		return 0;
}

SwingTrader::~SwingTrader()
{
}

// 追价
void SwingTrader::triggerWaitingOrder()
{
	map<long,OrderItem*>::iterator iter;
	for(iter=tradeUnit->getOrderQueue().begin(); iter != tradeUnit->getOrderQueue().end(); iter++)
	{
		OrderItem* oi = iter->second;
		if(!dispatcher->isSupport(oi->getTradePlatform(), oi->getOrderType()))
		{
			if(oi->getOrderType() == MKT && oi->getParentRefId() != 0)
			{

				if((oi->getBuySell()==BUY && oi->getSubmitPrice()<tradeUnit->getPrice()->lastPrice1) ||
					(oi->getBuySell()==SELL && oi->getSubmitPrice()>tradeUnit->getPrice()->lastPrice1))
				{
					oi->setStatus(CHANGING);
					oi->setAction(CHG_ACTION);
					if(oi->getBuySell()==BUY)
						oi->setSubmitPrice(tradeUnit->getPrice()->askPrice1);
					else if(oi->getBuySell()==SELL)
						oi->setSubmitPrice(tradeUnit->getPrice()->bidPrice1);
					dispatcher->submitOrder(oi);
				}
			}
			else if(oi->getOrderType() == STP || oi->getOrderType()==MIT)
			{
				if((oi->getBuySell()==BUY && oi->getSubmitPrice()>=tradeUnit->getPrice()->lastPrice1) ||
					(oi->getBuySell()==SELL && oi->getSubmitPrice()<=tradeUnit->getPrice()->lastPrice1))
				{
					if(optimizeOrderFlow==OPTIMIZE_OFP && oi->getQty()>tradeUnit->getQuote()->getMinContractQty())
					{
						oi->setStatus(ADDING);
						oi->setOrderType(MKT);
						decomposeOrder(oi);
					}
					else
					{
						oi->setOrderType(MKT);
						oi->setStatus(ADDING);
						oi->setAction(ADD_ACTION);
						if(oi->getBuySell()==BUY)
							oi->setSubmitPrice(tradeUnit->getPrice()->askPrice1);
						else if(oi->getBuySell()==SELL)
							oi->setSubmitPrice(tradeUnit->getPrice()->bidPrice1);
						dispatcher->submitOrder(oi);
					}
				}
			}	
		}	
	}
}

void SwingTrader::turnOffStrategy()
{
	if(strategy != NULL)
	strategy->setAutoTrading(false);
}

void SwingTrader::turnOnStrategy()
{
	if(strategy != NULL)
	{
		strategy->setAutoTrading(true);
		if(!strategy->getIntraBarTrading())
		{
			int timeInterval = tradeUnit->getBar(0)->getPeriod();
			//iTimerID = SetTimer(NULL, 0, );
		}
	}
}

void SwingTrader::postStrategySignal()
{
	PostThreadMessage(TraderThreadId,STRATEGY_MSG,0,0);
	if(!strategy->getIntraBarTrading() && strategy->getAutoTrading())
	{
		int timeInterval = tradeUnit->getBar(0)->getPeriod();
		//SetTimer();
	}
}

bool SwingTrader::double_divide(double divisor , double dividend)
{
	double result; 
	char result_buf[1000],*tmp;	 
	result = divisor/dividend;		
	sprintf(result_buf,"%lf",result);
	tmp = result_buf;
	while (*tmp++ != '.')
	 tmp++;
	while (*tmp != '\0') 
	{	 
		 if(*tmp != '0')
			return false;
		tmp++;
	 }	
	return true;
}