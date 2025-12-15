#include "MyMdSpi.h"
#include "config.h"
#include <json.hpp>
#include <thread>
#include <chrono>

int main()
{
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
    std::cerr << "Initializing CThostFtdcMdApi..." << std::endl;
    pMdApi->Init();

    // 5. 等待API线程结束
    // Join()会阻塞当前线程，直到API内部线程退出。
    // 在实际应用中，你可能需要一个更复杂的机制来控制程序的生命周期，
    // 例如通过一个信号量或条件变量来等待登录成功或接收到一定数量的行情数据。
    // 这里为了演示，我们让主线程等待一段时间，或者等待API内部线程自然结束。
    
    // 简单的等待登录和接收行情
    std::cerr << "Main thread waiting for API events... (Press Ctrl+C to exit)" << std::endl;
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
    std::cerr << "Releasing CThostFtdcMdApi..." << std::endl;
    pMdApi->Release();
    pMdApi = nullptr; // 避免悬空指针

    std::cerr << "Program exited." << std::endl;
    return 0;
}
