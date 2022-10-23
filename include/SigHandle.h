#ifndef SIGHANDLE
#define SIGHANDLE

#include "Globar.h"

//存储信号信息结构
typedef struct Signal_t
{
	//信号对应数字
	int signal;

	//信号名
	const char* signame;

	//信号处理函数指针
	void (*sighandle)(int sig, siginfo_t* siginfo, void* ucontext);
}Signal_t;

//信号处理初始化
int Init_signals();

//信号处理函数
void signal_handle(int sig, siginfo_t* siginfo, void* ucontext);

//获取子进程状态防止产生僵尸进程
void Process_Get_Status();

#endif // !SIGHANDLE

