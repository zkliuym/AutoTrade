#ifndef __DISPATCHER__
#define __DISPATCHER__
#include <iostream>
#include <string>    
#include <list>
#include <map>
#include <windows.h>
#include "TradeCube.h"

using namespace std; 

typedef struct
{
	int tradePlatform;
	string quoteId;
	DWORD threadId;
} quoteId_threadId;

class Dispatcher
{
public:
	Dispatcher(PlatformInfo& platformInfo);
	virtual ~Dispatcher();
	virtual int addQuote(QuoteItem *pOuoteItem) = 0;
	virtual bool isSupport(int orderType) = 0;
	bool getConnectStatus() { return connectStatus; }
	void setConnectStatus(bool) { this->connectStatus = false; }
	void loseConnection();
	virtual int sendOrder(OrderItem* pOrderItem) = 0;
	virtual int deleteQuote(QuoteItem *pOuoteItem) = 0;
	int addOrderThreadId(int traderId, DWORD traderThreadId);
	int addPriceThreadId(int tradePlatform, string quoteId, DWORD traderThreadId);
	void addUIThreadId(DWORD UIThreadID);
	void returnOrder(OrderItem* pOrderItem);
	void returnTrade(TradeItem* pTradeItem);
	void forwardPrice(PriceItem* pPriceItem);
	void forwardTickPrice(PriceItem* pPriceItem);
	PlatformInfo& getPlatformInfo() const { return platformInfo;}
	/** ��ȡ�����Ѿ��ɽ��Ķ��� */
	virtual map<int, TradeItem*>& getDoneTrades() = 0;
	/** ��ȡ��ǰ����δ�ɽ��Ķ��� */
	virtual map<int, OrderItem*>& getCurrentOrders() = 0;
	/** ��ȡָ��Ʒ�ֵĲ�λ������֮ǰ��position�������ú��û���AccountNo��Ʒ����quoteID */
	virtual void getPosition(Position& position) = 0;

protected:
	bool connectStatus;
	map<int, DWORD> orderThreadIdTable;
	map<int, TradeItem*> doneTrades;
	map<int, OrderItem*> currentOrders;
	int doneTradeCount;
	int currentOrderCount;
	map<string, Position> positions;
	PlatformInfo& platformInfo;
private:
	list<quoteId_threadId*> priceThreadIdQueue;
	DWORD UIThreadID;
};

#endif