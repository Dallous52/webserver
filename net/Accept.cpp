#include "Socket.h"
#include "Error.h"

//新建连接专用函数
void MSocket::Event_accept(pConnection old)
{
	struct sockaddr serv_sockaddr;//服务器socket地址
	socklen_t socklen;
	int err;
	int level;
	int a_sock;
	static bool use_accept4 = true; //是否使用accept4函数
	
	pConnection newptr;

	socklen = sizeof(serv_sockaddr);

	Error_insert_File(LOG_DEBUG, "Debug:> A connect comming...");

	do
	{
		if (use_accept4)
		{
			//使用accept4函数可以直接将套接字设置为非阻塞
			a_sock = accept4(old->fd, &serv_sockaddr, &socklen, SOCK_NONBLOCK);
		}
		else
		{
			//使用accept函数需要在后期手动设置非阻塞
			a_sock = accept(old->fd, &serv_sockaddr, &socklen);
		}

		//连接错误处理
		if (a_sock == -1)
		{
			err = errno;

			if (err == EAGAIN)
			{
				//发生此错误表示accept没有准备好，直接退出
				return;
			}

			level = LOG_URGENT;

			//软件引起的连接终止
			if (err == ECONNABORTED)
			{
				level = LOG_ERR;
			}
			else if  (err == ENFILE || err == EMFILE)
			{
				//进程fd用尽
				level = LOG_CRIT;
			}

			char* errs = new char[200];
			sprintf(errs, "event accept failed:> %m", errno);
			Error_insert_File(level, errs);
			delete[]errs;

			if (use_accept4 && ENOSYS)
			{
				//系统不支持acceot4函数，使用accept函数再来一次
				use_accept4 = false;
				continue;
			}

			if (err == ECONNABORTED)
			{
				//可忽略错误 ： do nothing
			}

			if (err == EMFILE || err == ENFILE)
			{
				//........
			}

			return;
		}//end if (a_sock == -1)
		
		//----------------------------accept成功----------------------
		Error_insert_File(LOG_INFO, "accept success");
		
		//工作连接数大于最大连接数，系统繁忙不再接收连接
		if (m_connection_work >= m_work_connections)
		{
			close(a_sock);
			return;
		}

		//为用户连接套接字申请连接池连接
		newptr = Get_connection(a_sock);

		//因为连接池空间不足，申请连接失败
		if (newptr == NULL)
		{
			//关闭连接
			if (close(a_sock) == -1)
			{
				Error_insert_File(LOG_URGENT, "epoll accept:> a_socket close failed.");
			}

			return;
		}

		//连接池中申请连接成功
		
		//将客户端地址拷贝至连接对象
		memcpy(&newptr->s_addr, &serv_sockaddr, socklen);

		//设置accept函数的非阻塞
		if (!use_accept4)
		{
			if (Set_NoBlock(a_sock) == false)
			{
				//设置非阻塞失败
				Close_Connection(newptr);
				return;
			}
		}

		//关联连接对象与监听对象
		newptr->listening = old->listening;
		
		//可写标记
		//newptr->w_ready = 1;

		newptr->rHandle = &MSocket::Read_requestHandle;

		newptr->wHandle = &MSocket::Write_requestHandle;

		//增加epoll事件
		//后续可将第4个参数otherflag 设置为EPOLLET 边缘高速模式
		if (Epoll_Add_event(a_sock, EPOLL_CTL_ADD, EPOLLIN  | EPOLLRDHUP, 0,
			newptr) == -1)
		{
			Close_Connection(newptr);
			return;
		}

		if (m_heartbeatStart)
		{
			HeartQueue_ADD(newptr);
		}

		m_connection_work++;
		break;

	} while (true);

	return;
}