#ifndef Process
#define Process

#include "Globar.h"
#include "ThreadPool.h"

extern MThreadPool mthreadpool;


//创建子进程主体函数
void Major_Create_WorkProc();

//按配置文件参数创建子进程
static void Create_WorkProc_ForNums();

//创建子进程
static int Create_WorkProc(int ordNum);

//子进程初始化
static void WorkProcess_Init(int ordNum);

//更改进程名
void SetProcName(string name);

//更改environ环境变量位置
void MoveEnviron();

//子进程工作环境
static void WorkProc_Cycle(int ordNum);

//创建守护进程
int Create_daemon();

//处理网络事件与定时器事件
void Process_Events_Timer();

#endif // !Process

