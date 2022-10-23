#include "SigHandle.h"
#include "Error.h"

//各信号以及相应处理函数集
Signal_t signals[] = {
	{SIGHUP,	"SIGHUP",			signal_handle},
	{SIGINT,	"SIGINT",			signal_handle},
	{SIGQUIT,	"SIGQUIT",			signal_handle},
	{SIGTERM,	"SIGTERM",			signal_handle},
	{SIGCHLD,	"SIGCHLD",			signal_handle},
	{SIGSYS,	"SIGSYS, SIG_IGN",	signal_handle},
	{SIGIO,		"SIGIO",			signal_handle},
	{	0,		 NULL,				NULL},
};

//记录子进程是否退出
sig_atomic_t this_reap = 0;

int Init_signals()
{
	Signal_t *sig;

	struct sigaction sa;
	//系统定义与信号处理有关结构体

	for (sig = signals; sig->signal != 0; sig++)
	{
		memset(&sa, 0, sizeof(struct sigaction));

		if (sig->sighandle)
		{
			sa.sa_sigaction = sig->sighandle;
			//将信号对应的处理函数附加给系统结构体
			
			sa.sa_flags = SA_SIGINFO;
			//表示信号处理程序生效
		}
		else
		{
			sa.sa_handler = SIG_IGN;
			//表示忽略型号处理程序
		}

		sigemptyset(&sa.sa_mask);
		//将屏蔽信号集清空

		//将上面程序设置的sa配置到系统上去
		if (sigaction(sig->signal, &sa, NULL) == -1)
		{
			return -1;
		}
	}

	return 0;
}


void signal_handle(int sig, siginfo_t* siginfo, void* ucontext)
{
	Signal_t* sigs;

	//信息集指向到接收到的信息，用于记录日志
	for (sigs = signals; sigs->signal != 0; sigs++)
	{
		if (sigs->signal == sig)
		{
			break;
		}
	}

	if (this_process == PROCESS_MASTER)
	{
		switch (sig)
		{
		case SIGCHLD:
			this_reap = 1;
			//接收到该信号时，reap记录子进程状态，以便主机程恢复
			break;

		default:
			
			break;
		}
	}
	else if (this_process == PROCESS_CHILD)
	{
		//.....
	}
	else
	{
		//.....
	}
	
	//如果发送来的信号信息不为空，发送信号来的pid不为空
	if (siginfo && siginfo->si_pid)
	{
		char* notice = new char[100];
		sprintf(notice, "signal %d %s is received from process %d.",
			sigs->signal, sigs->signame, siginfo->si_pid);

		Error_insert_File(LOG_NOTICE, notice);
		
		delete[]notice;
	}
	else
	{
		char* notice = new char[100];
		sprintf(notice, "signal %d %s is received unknow process.",
			sigs->signal, sigs->signame);

		Error_insert_File(LOG_NOTICE, notice);

		delete[]notice;
	}

	if (sig == SIGCHLD)
	{
		Process_Get_Status();
	}
	
	//cout << "signal is comming" << endl;
	//cout << this_pid << endl;

	return;
}
