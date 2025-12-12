#include "MyMdSpi.h"
#include <json.hpp>
#include <thread>
#include <chrono>

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


int main()
{
        // 1. 创建一个 DepthMarketData 结构体实例并填充数据
    MarketData::DepthMarketData marketDataInstance;
    marketDataInstance.InstrumentID = "rb2405";
    marketDataInstance.LastPrice = 3800.5;
    marketDataInstance.Volume = 123456;
    marketDataInstance.BidPrice1 = 3800.0;
    marketDataInstance.BidVolume1 = 500;
    marketDataInstance.AskPrice1 = 3801.0;
    marketDataInstance.AskVolume1 = 700;
    marketDataInstance.UpdateTime = "10:30:05";
    marketDataInstance.UpdateMillisec = 123;

    // 2. 将 C++ 结构体转换为 nlohmann::json 对象
    // 由于我们使用了 NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE 宏，
    // 这里的转换是隐式且直接的。
    json j_marketData = marketDataInstance;

    // 3. 将 nlohmann::json 对象转换为 JSON 字符串
    // dump() 方法用于将 JSON 对象序列化为字符串。
    // 参数 4 表示使用 4 个空格进行美化（pretty-printing），使其更易读。
    std::string json_string = j_marketData.dump(-1);

    // 4. 打印生成的 JSON 字符串
    std::cout << "生成的 JSON 字符串:\n" << json_string << std::endl;

    return 0;

    // 1. 创建CThostFtdcMdApi实例
    // 第一个参数是存储订阅信息文件的目录，默认为当前目录
    // 第二个参数是是否使用UDP，默认为false
    // 第三个参数是是否使用组播，默认为false
    CThostFtdcMdApi* pMdApi = CThostFtdcMdApi::CreateFtdcMdApi("", false, false);
    if (!pMdApi) {
        std::cerr << "Failed to create CThostFtdcMdApi instance." << std::endl;
        return -1;
    }

    // 2. 创建并注册回调实例
    MyMdSpi mdSpi;
    mdSpi.SetMdApi(pMdApi); // 将MdApi实例传递给Spi
    pMdApi->RegisterSpi(&mdSpi);

    // 3. 注册前置机地址
    // 请确保FRONT_ADDR是有效的行情前置机地址
    char frontAddr[256];
    strncpy(frontAddr, FRONT_ADDR, sizeof(frontAddr) - 1);
    frontAddr[sizeof(frontAddr) - 1] = '\0'; // 确保字符串以null结尾
    pMdApi->RegisterFront(frontAddr);

    // 4. 初始化API
    // Init()会启动API内部的线程，并尝试连接前置机
    std::cout << "Initializing CThostFtdcMdApi..." << std::endl;
    pMdApi->Init();

    // 5. 等待API线程结束
    // Join()会阻塞当前线程，直到API内部线程退出。
    // 在实际应用中，你可能需要一个更复杂的机制来控制程序的生命周期，
    // 例如通过一个信号量或条件变量来等待登录成功或接收到一定数量的行情数据。
    // 这里为了演示，我们让主线程等待一段时间，或者等待API内部线程自然结束。
    
    // 简单的等待登录和接收行情
    std::cout << "Main thread waiting for API events... (Press Ctrl+C to exit)" << std::endl;
    // 实际应用中，这里应该有一个循环或事件机制来保持程序运行，
    // 直到收到退出信号或完成所有操作。
    // 例如，可以等待一个标志位，当登录成功并接收到足够行情后，再执行登出和释放。
    
    // 为了演示，我们让主线程等待一段时间，以便观察行情数据
    pMdApi->Join(); // 等待API线程结束

    // 在程序退出前，执行登出和取消订阅操作
    if (mdSpi.m_bIsLogin) {
        mdSpi.UnSubscribeMarketData();
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 稍等片刻，确保取消订阅请求发出
        mdSpi.ReqUserLogout();
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 稍等片刻，确保登出请求发出
    }
    
    // 6. 释放API实例
    std::cout << "Releasing CThostFtdcMdApi..." << std::endl;
    pMdApi->Release();
    pMdApi = nullptr; // 避免悬空指针

    std::cout << "Program exited." << std::endl;
    return 0;
}
