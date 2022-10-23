#include "Process.h"
#include "Manage.h"

//处理网络事件与定时器事件
void Process_Events_Timer()
{
	msocket.epoll_process_events(-1);

	//........
}