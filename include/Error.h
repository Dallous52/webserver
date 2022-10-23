#ifndef Error
#define Error

#include "Globar.h"

using namespace std;

#define LOG_FATAL		0 //致命
#define LOG_URGENT		1 //紧急
#define LOG_SERIOUS		2 //严重
#define LOG_CRIT		3 //批评
#define LOG_ERR			4 //错误
#define LOG_WARN		5 //警告 
#define LOG_NOTICE		6 //注意
#define LOG_INFO		7 //信息
#define LOG_DEBUG		8 //调试

#define ERR_NUM			9

//错误号相关名称
const string ErrorInfo[ERR_NUM] = {
	"fatal",
	"urgent",
	"serious",
	"critical",
	"error",
	"warn",
	"notice",
	"infomation",
	"debuge"
};

//向日志中输入程序错误    错误等级            错误内容
void Error_insert_File(int errLev, const string& error);

//获取当前时间
string GetTime();

#endif