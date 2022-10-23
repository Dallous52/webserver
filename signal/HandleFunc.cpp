#include "SigHandle.h"
#include "Error.h"

void Process_Get_Status()
{
	pid_t	pid;
	int		stat;
	int		err;
	int		one = 0;

	while (true)
	{
		pid_t pid = waitpid(-1, &stat, WNOHANG);
		//参数1：-1 表示等待任何子进程
		//参数2：存储状态信息
		//参数3：不阻断程序
		
		//pid = 0表示子进程没结束，直接返回
		if (pid == 0)
		{
			return;
		}
		if (pid == -1)
		{
			err = errno;

			//调用被某个信号终端
			if (err == EINTR)
			{
				continue;
			}

			//函数已经执行过了，没有子进程直接返回
			if (err == ECHILD && one)
			{
				return;
			}

			//没有子进程
			if (err == ECHILD)
			{
				Error_insert_File(LOG_ERR, "waitpid() failed 1.");
				return;
			}

			Error_insert_File(LOG_ERR, "waitpid() failed 2.");
			return;
		}

		//此处表示子进程已经成功退出，记录退出日志

		one = 1;

		char* notice = new char[100];
		if (WTERMSIG(stat))//获取子进程终止信号编号
		{
			sprintf(notice, "process %d is exited on signal %d",
				pid, WTERMSIG(stat));

			//记录子进程退出日志
			Error_insert_File(LOG_WARN, notice);
		}
		else
		{
			sprintf(notice, "process %d is exited on signal %d",
				pid, WTERMSIG(stat));

			//记录子进程退出日志
			Error_insert_File(LOG_NOTICE, notice);
		}
		delete[]notice;
	}

	return;
}