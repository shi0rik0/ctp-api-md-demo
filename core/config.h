#ifndef CONFIG_H
#define CONFIG_H

// 定义一些常量，实际使用时应从配置文件或命令行参数读取
extern const char* FRONT_ADDR; // 行情前置机地址
extern const char* BROKER_ID; // 经纪公司代码
extern const char* USER_ID; // 用户ID
extern const char* PASSWORD; // 密码

// 要订阅的合约列表
extern const char* INSTRUMENT_IDS[]; // 合约代码
extern const int INSTRUMENT_COUNT;

#endif // CONFIG_H
