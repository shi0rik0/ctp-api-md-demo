#include "MyMdSpi.h"
#include "config.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <iconv.h>
#include <json.hpp>

// 为了方便，使用 nlohmann::json 的别名
using json = nlohmann::json;

// 定义一个命名空间来包含我们的数据结构，这有助于避免全局命名冲突
namespace MarketData
{
    struct DepthMarketData
    {
        std::string InstrumentID;
        double LastPrice;
        int Volume;
        double BidPrice1;
        int BidVolume1;
        double AskPrice1;
        int AskVolume1;
        std::string UpdateTime;
        int UpdateMillisec;
    };

    // 使用 NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE 宏来自动生成 to_json 和 from_json 函数
    // 这个宏必须放在结构体所在的命名空间内（或者全局命名空间）
    // 第一个参数是结构体名称，后续参数是结构体的成员名称
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DepthMarketData, InstrumentID, LastPrice, Volume,
                                       BidPrice1, BidVolume1, AskPrice1, AskVolume1,
                                       UpdateTime, UpdateMillisec)
} // namespace MarketData

MyMdSpi::MyMdSpi() : m_pMdApi(nullptr), m_nRequestID(0), m_bIsLogin(false), m_bIsConnected(false) {}

void MyMdSpi::SetMdApi(CThostFtdcMdApi* pMdApi) {
    m_pMdApi = pMdApi;
}

// 辅助函数：将 GBK 编码转换为 UTF-8
std::string MyMdSpi::ConvertGBKToUTF8(const char* gbkStr) {
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
void MyMdSpi::OnFrontConnected() {
    std::cerr << "=== OnFrontConnected ===" << std::endl;
    m_bIsConnected = true;
    // 连接成功后，发送登录请求
    ReqUserLogin();
}

///当客户端与交易后台通信连接断开时，该方法被调用。
void MyMdSpi::OnFrontDisconnected(int nReason) {
    std::cerr << "=== OnFrontDisconnected, Reason: " << std::hex << nReason << std::dec << " ===" << std::endl;
    m_bIsConnected = false;
    m_bIsLogin = false;
    // 在实际应用中，这里可能需要实现自动重连逻辑
}

///心跳超时警告
void MyMdSpi::OnHeartBeatWarning(int nTimeLapse) {
    std::cerr << "=== OnHeartBeatWarning, TimeLapse: " << nTimeLapse << " ===" << std::endl;
}

///登录请求响应
void MyMdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    std::cerr << "=== OnRspUserLogin ===" << std::endl;
    if (pRspInfo && pRspInfo->ErrorID == 0) {
        std::cerr << "Login successful!" << std::endl;
        std::cerr << "BrokerID: " << pRspUserLogin->BrokerID << std::endl;
        std::cerr << "UserID: " << pRspUserLogin->UserID << std::endl;
        std::cerr << "TradingDay: " << m_pMdApi->GetTradingDay() << std::endl;
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
void MyMdSpi::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    std::cerr << "=== OnRspUserLogout ===" << std::endl;
    if (pRspInfo && pRspInfo->ErrorID == 0) {
        std::cerr << "Logout successful!" << std::endl;
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

void MyMdSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
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
void MyMdSpi::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    std::lock_guard<std::mutex> lock(m_coutMutex);
    std::cerr << "=== OnRspSubMarketData ===" << std::endl;
    if (pRspInfo && pRspInfo->ErrorID == 0) {
        std::cerr << "Subscribe market data successful for instrument: " << (pSpecificInstrument ? pSpecificInstrument->InstrumentID : "N/A") << std::endl;
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
void MyMdSpi::OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    std::cerr << "=== OnRspUnSubMarketData ===" << std::endl;
    if (pRspInfo && pRspInfo->ErrorID == 0) {
        std::cerr << "Unsubscribe market data successful for instrument: " << (pSpecificInstrument ? pSpecificInstrument->InstrumentID : "N/A") << std::endl;
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
void MyMdSpi::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData) {
    if (!pDepthMarketData) return;

    // 创建 DepthMarketData 实例并填充数据
    MarketData::DepthMarketData marketDataInstance;
    marketDataInstance.InstrumentID = pDepthMarketData->InstrumentID;
    marketDataInstance.LastPrice = pDepthMarketData->LastPrice;
    marketDataInstance.Volume = pDepthMarketData->Volume;
    marketDataInstance.BidPrice1 = pDepthMarketData->BidPrice1;
    marketDataInstance.BidVolume1 = pDepthMarketData->BidVolume1;
    marketDataInstance.AskPrice1 = pDepthMarketData->AskPrice1;
    marketDataInstance.AskVolume1 = pDepthMarketData->AskVolume1;
    marketDataInstance.UpdateTime = pDepthMarketData->UpdateTime;
    marketDataInstance.UpdateMillisec = pDepthMarketData->UpdateMillisec;

    // 将结构体转换为 JSON
    json j_marketData = marketDataInstance;
    std::string json_string = j_marketData.dump(-1);

    // 构造完整的 SSE 格式字符串
    std::string output = "data: " + json_string + "\n\n";

    // 一次性输出
    {
        std::lock_guard<std::mutex> lock(m_coutMutex);
        std::cout << output;
        // 确保输出立即刷新
        std::cout.flush();
    }
}

// --- 辅助方法 ---

void MyMdSpi::ReqUserLogin() {
    if (!m_pMdApi) return;

    CThostFtdcReqUserLoginField req;
    memset(&req, 0, sizeof(req)); // 清空结构体
    // 填充登录请求字段
    strncpy(req.BrokerID, BROKER_ID, sizeof(req.BrokerID) - 1);
    strncpy(req.UserID, USER_ID, sizeof(req.UserID) - 1);
    strncpy(req.Password, PASSWORD, sizeof(req.Password) - 1);

    // 发送登录请求
    int ret = m_pMdApi->ReqUserLogin(&req, ++m_nRequestID);
    {
        std::lock_guard<std::mutex> lock(m_coutMutex);
        std::cerr << "Sending user login request... " << (ret == 0 ? "success" : "failed") << std::endl;
    }
}

void MyMdSpi::ReqUserLogout() {
    if (!m_pMdApi || !m_bIsLogin) return;

    CThostFtdcUserLogoutField req;
    memset(&req, 0, sizeof(req));
    strncpy(req.BrokerID, BROKER_ID, sizeof(req.BrokerID) - 1);
    strncpy(req.UserID, USER_ID, sizeof(req.UserID) - 1);

    int ret = m_pMdApi->ReqUserLogout(&req, ++m_nRequestID);
    {
        std::lock_guard<std::mutex> lock(m_coutMutex);
        std::cerr << "Sending user logout request... " << (ret == 0 ? "success" : "failed") << std::endl;
    }
}

void MyMdSpi::SubscribeMarketData() {
    if (!m_pMdApi || !m_bIsLogin) return;

    // 将 const char* 数组转换为 char* 数组，因为API接口需要 char*[]
    char** ppInstrumentID = new char*[INSTRUMENT_COUNT];
    for (int i = 0; i < INSTRUMENT_COUNT; ++i) {
        ppInstrumentID[i] = new char[strlen(INSTRUMENT_IDS[i]) + 1];
        strcpy(ppInstrumentID[i], INSTRUMENT_IDS[i]);
    }

    int ret = m_pMdApi->SubscribeMarketData(ppInstrumentID, INSTRUMENT_COUNT);
    {
        std::lock_guard<std::mutex> lock(m_coutMutex);
        std::cerr << "Sending subscribe market data request... " << (ret == 0 ? "success" : "failed") << std::endl;
    }

    // 释放内存
    for (int i = 0; i < INSTRUMENT_COUNT; ++i) {
        delete[] ppInstrumentID[i];
    }
    delete[] ppInstrumentID;
}

void MyMdSpi::UnSubscribeMarketData() {
    if (!m_pMdApi || !m_bIsLogin) return;

    char** ppInstrumentID = new char*[INSTRUMENT_COUNT];
    for (int i = 0; i < INSTRUMENT_COUNT; ++i) {
        ppInstrumentID[i] = new char[strlen(INSTRUMENT_IDS[i]) + 1];
        strcpy(ppInstrumentID[i], INSTRUMENT_IDS[i]);
    }

    int ret = m_pMdApi->UnSubscribeMarketData(ppInstrumentID, INSTRUMENT_COUNT);
    {
        std::lock_guard<std::mutex> lock(m_coutMutex);
        std::cerr << "Sending unsubscribe market data request... " << (ret == 0 ? "success" : "failed") << std::endl;
    }

    for (int i = 0; i < INSTRUMENT_COUNT; ++i) {
        delete[] ppInstrumentID[i];
    }
    delete[] ppInstrumentID;
}