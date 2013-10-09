#ifndef __SWINGTRADER__
#define __SWINGTRADER__
#include <time.h>
#include <iostream>
#include <string> 
#include <windows.h>
#include <sstream>
#include <tchar.h>
#include "TradeCube.h"
#include "Strategy.h"
#include "TradeUnit.h"
#include "Dispatcher.h"
#include "../TradeCube/iswingtradedialog.h"

class ISwingTradeDialog;

class SwingTrader : public Strategy
{
public:
    SwingTrader(int traderId, ISwingTradeDialog* iSwingTradeDialog);
    virtual ~SwingTrader();
    int setTradeUnit(TradeUnit* tradeUnit);
    TradeUnit* getTradeUnit();
    int deleteTradeUnit();
    void closeAllPositions();
    void deleteStrategyOrder();
protected:
    void triggerWaitingOrder();
    long createOrder(char buysell, char openclose, double submitPrice, double qty, int orderType, int validType, int submitter);
    int deleteOrder(long orderRefId);
    long updateOrder(long orderRefId, char buysell, char openclose, double submitPrice, double qty, int validType);
    bool double_divide(double divisor, double dividend);
    long buy(long& orderRefId, double submitPrice, double qty, int orderType, int validType, int submitter);
    long sell(long& orderRefId, double submitPrice, double qty, int orderType, int validType, int submitter);
    long buytocover(long& orderRefId, double submitPrice, double qty, int orderType, int validType, int submitter);
    long sellshort(long& orderRefId, double submitPrice, double qty, int orderType, int validType, int submitter);
    double marketposition(int pos_ago);

private:
    virtual void processTickPrice(MSG& msg);
    virtual void updateBars();
    virtual void processOrderAccepted(MSG& msg);
    virtual void processTradeDone(MSG& msg);
    virtual void processPrice(MSG& msg);
    virtual void signal();
    void executeStrategy();
    bool isBarsEnough();

    TradeUnit* tradeUnit;
    UINT_PTR iTimerID;

    //test
    long testOrderID;

    ISwingTradeDialog *iSwingTradeDialog;
};
#endif
