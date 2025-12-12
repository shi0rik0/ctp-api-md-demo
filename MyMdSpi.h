// MyMdSpi.h
#ifndef MY_MD_SPI_H
#define MY_MD_SPI_H

#include <ThostFtdcMdApi.h> // 包含您提供的头文件
#include <ThostFtdcUserApiStruct.h> // 包含CTP API的数据结构头文件

#include <iostream>
#include <string>
#include <vector>
#include <cstring> // For strcpy_s or strncpy
#include <iconv.h> // 添加 iconv 头文件

// 定义一些常量，实际使用时应从配置文件或命令行参数读取
const char* FRONT_ADDR = "tcp://182.254.243.31:30013"; // 示例行情前置机地址，请替换为实际地址
const char* BROKER_ID = "9999"; // 示例经纪公司代码
const char* USER_ID = "anon"; // 示例用户ID
const char* PASSWORD = "123456"; // 示例密码

// 要订阅的合约列表
const char* INSTRUMENT_IDS[] = {"rb2405", "au2601"}; // 示例合约代码
const int INSTRUMENT_COUNT = sizeof(INSTRUMENT_IDS) / sizeof(INSTRUMENT_IDS[0]);

class MyMdSpi : public CThostFtdcMdSpi
{
public:
    CThostFtdcMdApi* m_pMdApi; // 指向MdApi实例的指针
    int m_nRequestID;          // 请求ID，用于匹配请求和响应
    bool m_bIsLogin;           // 登录状态
    bool m_bIsConnected;       // 连接状态

public:
    MyMdSpi() : m_pMdApi(nullptr), m_nRequestID(0), m_bIsLogin(false), m_bIsConnected(false) {}

    void SetMdApi(CThostFtdcMdApi* pMdApi) {
        m_pMdApi = pMdApi;
    }

    // 辅助函数：将 GBK 编码转换为 UTF-8
    std::string ConvertGBKToUTF8(const char* gbkStr) {
        if (!gbkStr || strlen(gbkStr) == 0) return "";

        iconv_t cd = iconv_open("UTF-8", "GBK");
        if (cd == (iconv_t)-1) {
            std::cerr << "iconv_open failed" << std::endl;
            return gbkStr; // 返回原字符串
        }

        size_t inLen = strlen(gbkStr);
        size_t outLen = inLen * 2; // 估算输出缓冲区大小
        char* outBuf = new char[outLen + 1];
        char* inBuf = const_cast<char*>(gbkStr);
        char* outPtr = outBuf;

        size_t ret = iconv(cd, &inBuf, &inLen, &outPtr, &outLen);
        if (ret == (size_t)-1) {
            std::cerr << "iconv conversion failed" << std::endl;
            delete[] outBuf;
            iconv_close(cd);
            return gbkStr; // 返回原字符串
        }

        *outPtr = '\0';
        std::string utf8Str(outBuf);
        delete[] outBuf;
        iconv_close(cd);
        return utf8Str;
    }

    ///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
    virtual void OnFrontConnected() override {
        std::cout << "=== OnFrontConnected ===" << std::endl;
        m_bIsConnected = true;
        // 连接成功后，发送登录请求
        ReqUserLogin();
    }

    ///当客户端与交易后台通信连接断开时，该方法被调用。
    virtual void OnFrontDisconnected(int nReason) override {
        std::cerr << "=== OnFrontDisconnected, Reason: " << std::hex << nReason << std::dec << " ===" << std::endl;
        m_bIsConnected = false;
        m_bIsLogin = false;
        // 在实际应用中，这里可能需要实现自动重连逻辑
    }

    ///心跳超时警告
    virtual void OnHeartBeatWarning(int nTimeLapse) override {
        std::cout << "=== OnHeartBeatWarning, TimeLapse: " << nTimeLapse << " ===" << std::endl;
    }

    ///登录请求响应
    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override {
        std::cout << "=== OnRspUserLogin ===" << std::endl;
        if (pRspInfo && pRspInfo->ErrorID == 0) {
            std::cout << "Login successful!" << std::endl;
            std::cout << "BrokerID: " << pRspUserLogin->BrokerID << std::endl;
            std::cout << "UserID: " << pRspUserLogin->UserID << std::endl;
            std::cout << "TradingDay: " << m_pMdApi->GetTradingDay() << std::endl;
            m_bIsLogin = true;

            // 登录成功后，订阅行情
            SubscribeMarketData();
        } else {
            std::string utf8Msg = ConvertGBKToUTF8(pRspInfo ? pRspInfo->ErrorMsg : "Unknown error");
            std::cerr << "Login failed! ErrorID: " << (pRspInfo ? pRspInfo->ErrorID : -1)
                      << ", ErrorMsg: " << utf8Msg << std::endl;
            m_bIsLogin = false;
        }
        if (bIsLast) {
            // 最后一个响应包
        }
    }

    ///登出请求响应
    virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override {
        std::cout << "=== OnRspUserLogout ===" << std::endl;
        if (pRspInfo && pRspInfo->ErrorID == 0) {
            std::cout << "Logout successful!" << std::endl;
            m_bIsLogin = false;
        } else {
            std::string utf8Msg = ConvertGBKToUTF8(pRspInfo ? pRspInfo->ErrorMsg : "Unknown error");
            std::cerr << "Logout failed! ErrorID: " << (pRspInfo ? pRspInfo->ErrorID : -1)
                      << ", ErrorMsg: " << utf8Msg << std::endl;
        }
        if (bIsLast) {
            // 最后一个响应包
        }
    }

    virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override {
        std::cerr << "=== OnRspError ===" << std::endl;
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            std::string utf8Msg = ConvertGBKToUTF8(pRspInfo->ErrorMsg);
            std::cerr << "ErrorID: " << pRspInfo->ErrorID << ", ErrorMsg: " << utf8Msg << std::endl;
        }
        if (bIsLast) {
            // 最后一个响应包
        }
    }

    ///订阅行情应答
    virtual void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override {
        std::cout << "=== OnRspSubMarketData ===" << std::endl;
        if (pRspInfo && pRspInfo->ErrorID == 0) {
            std::cout << "Subscribe market data successful for instrument: " << (pSpecificInstrument ? pSpecificInstrument->InstrumentID : "N/A") << std::endl;
        } else {
            std::string utf8Msg = ConvertGBKToUTF8(pRspInfo ? pRspInfo->ErrorMsg : "Unknown error");
            std::cerr << "Subscribe market data failed! Instrument: " << (pSpecificInstrument ? pSpecificInstrument->InstrumentID : "N/A")
                      << ", ErrorID: " << (pRspInfo ? pRspInfo->ErrorID : -1)
                      << ", ErrorMsg: " << utf8Msg << std::endl;
        }
        if (bIsLast) {
            // 最后一个响应包
        }
    }

    ///取消订阅行情应答
    virtual void OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override {
        std::cout << "=== OnRspUnSubMarketData ===" << std::endl;
        if (pRspInfo && pRspInfo->ErrorID == 0) {
            std::cout << "Unsubscribe market data successful for instrument: " << (pSpecificInstrument ? pSpecificInstrument->InstrumentID : "N/A") << std::endl;
        } else {
            std::string utf8Msg = ConvertGBKToUTF8(pRspInfo ? pRspInfo->ErrorMsg : "Unknown error");
            std::cerr << "Unsubscribe market data failed! Instrument: " << (pSpecificInstrument ? pSpecificInstrument->InstrumentID : "N/A")
                      << ", ErrorID: " << (pRspInfo ? pRspInfo->ErrorID : -1)
                      << ", ErrorMsg: " << utf8Msg << std::endl;
        }
        if (bIsLast) {
            // 最后一个响应包
        }
    }

    ///深度行情通知
    virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData) override {
        if (!pDepthMarketData) return;

        // 打印行情数据，实际应用中会进行更复杂的处理
        std::cout << "--- OnRtnDepthMarketData ---" << std::endl;
        std::cout << "InstrumentID: " << pDepthMarketData->InstrumentID << std::endl;
        std::cout << "LastPrice: " << pDepthMarketData->LastPrice << std::endl;
        std::cout << "Volume: " << pDepthMarketData->Volume << std::endl;
        std::cout << "BidPrice1: " << pDepthMarketData->BidPrice1 << ", BidVolume1: " << pDepthMarketData->BidVolume1 << std::endl;
        std::cout << "AskPrice1: " << pDepthMarketData->AskPrice1 << ", AskVolume1: " << pDepthMarketData->AskVolume1 << std::endl;
        std::cout << "UpdateTime: " << pDepthMarketData->UpdateTime << "." << pDepthMarketData->UpdateMillisec << std::endl;
        std::cout << "----------------------------" << std::endl;
    }

    // --- 辅助方法 ---

    void ReqUserLogin() {
        if (!m_pMdApi) return;

        CThostFtdcReqUserLoginField req;
        memset(&req, 0, sizeof(req)); // 清空结构体
        // 填充登录请求字段
        strncpy(req.BrokerID, BROKER_ID, sizeof(req.BrokerID) - 1);
        strncpy(req.UserID, USER_ID, sizeof(req.UserID) - 1);
        strncpy(req.Password, PASSWORD, sizeof(req.Password) - 1);

        // 发送登录请求
        int ret = m_pMdApi->ReqUserLogin(&req, ++m_nRequestID);
        std::cout << "Sending user login request... " << (ret == 0 ? "success" : "failed") << std::endl;
    }

    void ReqUserLogout() {
        if (!m_pMdApi || !m_bIsLogin) return;

        CThostFtdcUserLogoutField req;
        memset(&req, 0, sizeof(req));
        strncpy(req.BrokerID, BROKER_ID, sizeof(req.BrokerID) - 1);
        strncpy(req.UserID, USER_ID, sizeof(req.UserID) - 1);

        int ret = m_pMdApi->ReqUserLogout(&req, ++m_nRequestID);
        std::cout << "Sending user logout request... " << (ret == 0 ? "success" : "failed") << std::endl;
    }

    void SubscribeMarketData() {
        if (!m_pMdApi || !m_bIsLogin) return;

        // 将 const char* 数组转换为 char* 数组，因为API接口需要 char*[]
        char** ppInstrumentID = new char*[INSTRUMENT_COUNT];
        for (int i = 0; i < INSTRUMENT_COUNT; ++i) {
            ppInstrumentID[i] = new char[strlen(INSTRUMENT_IDS[i]) + 1];
            strcpy(ppInstrumentID[i], INSTRUMENT_IDS[i]);
        }

        int ret = m_pMdApi->SubscribeMarketData(ppInstrumentID, INSTRUMENT_COUNT);
        std::cout << "Sending subscribe market data request... " << (ret == 0 ? "success" : "failed") << std::endl;

        // 释放内存
        for (int i = 0; i < INSTRUMENT_COUNT; ++i) {
            delete[] ppInstrumentID[i];
        }
        delete[] ppInstrumentID;
    }

    void UnSubscribeMarketData() {
        if (!m_pMdApi || !m_bIsLogin) return;

        char** ppInstrumentID = new char*[INSTRUMENT_COUNT];
        for (int i = 0; i < INSTRUMENT_COUNT; ++i) {
            ppInstrumentID[i] = new char[strlen(INSTRUMENT_IDS[i]) + 1];
            strcpy(ppInstrumentID[i], INSTRUMENT_IDS[i]);
        }

        int ret = m_pMdApi->UnSubscribeMarketData(ppInstrumentID, INSTRUMENT_COUNT);
        std::cout << "Sending unsubscribe market data request... " << (ret == 0 ? "success" : "failed") << std::endl;

        for (int i = 0; i < INSTRUMENT_COUNT; ++i) {
            delete[] ppInstrumentID[i];
        }
        delete[] ppInstrumentID;
    }
};

#endif // MY_MD_SPI_H
