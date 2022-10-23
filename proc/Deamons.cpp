#include "Process.h"
#include "Error.h"

int Create_daemon()//守护进程创建函数
{
	int fd;

	switch (fork())//创建子进程，子进程返回0创建成功，父进程返回子进程PID
	{
	case -1:
		Error_insert_File(LOG_FATAL, 
			"Daemons create:> child process create failed.");
		//子进程创建失败记录日志
		return -1;

	case 0:
		//子进程创建完成,直接跳出
		break;

	default:
		//父进程退出
		return 1;
	}

	/*----------只有子进程在执行--------*/

	this_ppid = this_pid;
	this_pid = getpid();
	//记录子进程的pid ppid

	if (setsid() == -1)//为子进程建立新会话，使其脱离终端
	{
		Error_insert_File(LOG_FATAL,
			"Daemons create:> Terminal disconnection failed, new session establishment error.");
		//新会话创建失败，记录错误日志
		return -1;
	}

	umask(0);//开发文件权限，以免引起混乱

	fd = open("/dev/null", O_RDWR);
	//打开空设备，准备重定向子进程输入输出
	
	if (fd == -1)
	{
		Error_insert_File(LOG_URGENT, 
			"Daemons create:> file:/dev/null open failed.");
		//打开空设备失败，记录错误日志
		return -1;
	}

	//将进程的标准输入定向到空设备
	if (dup2(fd, STDIN_FILENO) == -1)//此处用dup2因为它会先将STDIN_FILENO关闭
	{											//防止发生意外
		Error_insert_File(LOG_URGENT, 
			"Daemons create:> Standard input orientation failed.");
		//重定向输入失败，记录错误日志
		return -1;
	}

	//将进程的标准输出定向到空设备
	if (dup2(fd, STDOUT_FILENO) == -1)
	{
		Error_insert_File(LOG_URGENT, 
			"Daemons create:> Standard output orientation failed.");
		//重定向输出失败，记录错误日志
		return -1;
	}

	if (fd > STDERR_FILENO)//如果fd所占文件描述符>系统最初分配标准文件描述符
	{						//说明程序运行正确

		if (close(fd) == -1)//释放fd所占文件描述符以便资源复用
		{ 
			Error_insert_File(LOG_ERR, 
				"Daemons create:> File descriptor release failed.");
			//释放文件描述符失败,记录错误日志
			return -1;
		}
	}

	return 0;//守护进程创建成功
}