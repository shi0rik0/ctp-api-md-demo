#ifndef MY_MD_SPI_H
#define MY_MD_SPI_H

#include <ThostFtdcMdApi.h>
#include <ThostFtdcUserApiStruct.h>

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <iconv.h>
#include <mutex>

class MyMdSpi : public CThostFtdcMdSpi
{
public:
    CThostFtdcMdApi* m_pMdApi;
    int m_nRequestID;          // 请求ID，用于匹配请求和响应
    bool m_bIsLogin;           // 登录状态
    bool m_bIsConnected;       // 连接状态

private:
    std::mutex m_coutMutex;    // 用于保护 std::cout 的互斥锁

public:
    MyMdSpi();
    void SetMdApi(CThostFtdcMdApi* pMdApi);

    // 辅助函数：将 GBK 编码转换为 UTF-8
    std::string ConvertGBKToUTF8(const char* gbkStr);

    ///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
    virtual void OnFrontConnected() override;

    ///当客户端与交易后台通信连接断开时，该方法被调用。
    virtual void OnFrontDisconnected(int nReason) override;

    ///心跳超时警告
    virtual void OnHeartBeatWarning(int nTimeLapse) override;

    ///登录请求响应
    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override;

    ///登出请求响应
    virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override;

    virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override;

    ///订阅行情应答
    virtual void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override;

    ///取消订阅行情应答
    virtual void OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override;

    ///深度行情通知
    virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData) override;

    // --- 辅助方法 ---

    void ReqUserLogin();

    void ReqUserLogout();

    void SubscribeMarketData();

    void UnSubscribeMarketData();
};

#endif // MY_MD_SPI_H
