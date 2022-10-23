#include "Socket.h"
#include "ProfileCtl.h"
#include "Error.h"
#include "MLock.h"

MSocket::MSocket()
{
	m_listen_port_count = 1;
	m_work_connections = 1;

	//epoll相关成员变量初始化
	m_epollHandle = -1;

	m_PKG_Hlen = sizeof(PKG_Head);
	m_MSG_Hlen = sizeof(MSG_Head);

	//数值类型初始化
	m_sendMsgCount = 0;
	m_connection_all = 0;
	m_connection_free = 0;
	m_connection_reco = 0;
	m_connection_work = 0;
	m_MAX_Send = 0;

	return;
}

bool MSocket::Initialize()
{
	m_listen_port_count = stoi(findProVar("listen_port_count"));

	m_work_connections = stoi(findProVar("work_connections"));

	m_reWaitTime = stoi(findProVar("recovery_wait_time"));

	m_heartbeatStart = stoi(findProVar("heartbeat_start"));

	int wait = stoi(findProVar("heartbeat_wait"));

	m_heartbeatWait = wait > 5 ? wait : 5;

	m_floodCheckStart = stoi(findProVar("flood_check_start"));

	m_floodInterval = stoi(findProVar("flood_interval"));

	m_floodPackages = stoi(findProVar("flood_packages"));

	m_MAX_Send = stoi(findProVar("max_send"));

	m_MAX_rqSend = stoi(findProVar("max_request_send"));

	return Establish_listenSocket();
}


bool MSocket::WrokProcRelevent_Init()
{
	//工作进程相关互斥锁初始化
	if (pthread_mutex_init(&m_recoveryMutex, NULL) != 0)
	{
		Error_insert_File(LOG_CRIT,
			"MSocket initialization:> recycle queue mutex init failed.");
		return false;
	}
	if (pthread_mutex_init(&m_connectMutex, NULL) != 0)
	{
		Error_insert_File(LOG_CRIT,
			"MSocket initialization:> connection mutex init failed.");
		return false;
	}
	if (pthread_mutex_init(&m_sendMsgMutex, NULL) != 0)
	{
		Error_insert_File(LOG_CRIT,
			"MSocket initialization:> message send mutex init failed.");
		return false;
	}
	if (pthread_mutex_init(&m_timeMutex, NULL) != 0)
	{
		Error_insert_File(LOG_CRIT,
			"MSocket initialization:> message send mutex init failed.");
		return false;
	}

	//信号量初始化
	if (sem_init(&m_semSendMsg, 0, 0) == -1)
	{
		Error_insert_File(LOG_URGENT, 
			"MSocket initialization:> The semaphore init failed.");
		return false;
	}

	//创建线程
	int err;

	//数据发送线程
	ThreadItem* sendConn;
	sendConn = new ThreadItem(this);
	m_auxthread.push_back(sendConn);

	err = pthread_create(&sendConn->Handle, NULL,
		MessageSend_Thread, sendConn);
	if (err != 0)
	{
		Error_insert_File(LOG_URGENT, 
			"MSocket initialization:> Message send thread create failed.");
		return false;
	}


	//连接回收线程
	ThreadItem* recyConn;
	recyConn = new ThreadItem(this);
	m_auxthread.push_back(recyConn);

	err = pthread_create(&recyConn->Handle, NULL, 
		ReConnection_Thread, recyConn);
	if (err != 0)
	{
		Error_insert_File(LOG_URGENT, 
			"MSocket initialization:> Recovery thread create failed.");
		return false;
	}

	if (m_heartbeatStart)
	{
		//心跳包检测线程
		ThreadItem* heartConn;
		heartConn = new ThreadItem(this);
		m_auxthread.push_back(heartConn);

		err = pthread_create(&heartConn->Handle, NULL,
			HeartbeatTime_Thread, heartConn);
		if (err != 0)
		{
			Error_insert_File(LOG_URGENT,
				"MSocket initialization:> Heartbeat time thread create failed.");
			return false;
		}
	}

	return true;
}


bool MSocket::Establish_listenSocket()
{
	int auxSocket;
	int auxPort;				

	struct sockaddr_in serv_addr;//服务器地址结构体
	memset(&serv_addr, 0, sizeof(serv_addr));

	//设置本地服务器要监听的地址和端口
	serv_addr.sin_family = AF_INET;							//选择协议族为IPV4
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);//监听本地所有IP地址

	for (int i = 0; i < m_listen_port_count; i++)
	{
		//第一个参数：使用IPV4协议
		//第二个参数：使用TCP连接
		//第三个参数：不知道。。。
		auxSocket = socket(AF_INET, SOCK_STREAM, 0);
		//建立socket套接字
		if (auxSocket == -1)
		{
			//套接字建立失败记录日志；
			char* err = new char[200];
			sprintf(err, "The %d socket of process %d establish failed:> %m.",
				i, this_pid, errno);
			Error_insert_File(LOG_FATAL, err);
			delete[]err;

			return false;
		}

		//setsockopt():设置套接字参数
		//第二个参数和第三个参数配套使用,一一对应
		//此处SO_REUSEADDR允许同一个端口上套接字的复用
		//用于解决bind函数因TCP状态TIME_WAIT而出错的问题
		int reuseaddr = 1;
		if (setsockopt(auxSocket, SOL_SOCKET, SO_REUSEADDR,
			(const void*)&reuseaddr, sizeof(int)) == -1)
		{
			//套接字复用设置失败，记录日志
			char* err = new char[200];
			sprintf(err, "The %d socket of process %d reuse address setting failed:> %m.",
				i, this_pid, errno);
			Error_insert_File(LOG_URGENT, err);
			delete[]err;

			return false;
		}

		int reuseport = 1;
		if (setsockopt(auxSocket, SOL_SOCKET, SO_REUSEPORT,
			(const void*)&reuseport, sizeof(int)) == -1)
		{
			//端口复用设置失败，记录日志
			char* err = new char[200];
			sprintf(err, "The %d socket of process %d reuse port setting failed:> %m.",
				i, this_pid, errno);
			Error_insert_File(LOG_URGENT, err);
			delete[]err;
		}

		if (Set_NoBlock(auxSocket) == false)
		{
			//套接字非阻塞设置失败，记录日志
			char* err = new char[200];
			sprintf(err, "The %d socket of process %d no block setting failed:> %m.",
				i, this_pid, errno);
			Error_insert_File(LOG_URGENT, err);
			delete[]err;

			return false;
		}

		//设置要监听的地址与端口
		string port = "listen_port";
		port += to_string(i);
		auxPort = stoi(findProVar(port));
		serv_addr.sin_port = htons((in_port_t)auxPort);

		//绑定服务器地址结构
		if (bind(auxSocket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
		{
			//服务器地址结构绑定失败，记录日志
			char* err = new char[200];
			sprintf(err, "The %d socket of process %d server address bind failed:> %m.",
				i, this_pid, errno);
			Error_insert_File(LOG_URGENT, err);
			delete[]err;

			return false;
		}

		//参数2表示服务器可以积压未处理完的连接请求个数
		if (listen(auxSocket, ListenBackLog) == -1)
		{
			//监听端口失败
			char* err = new char[200];
			sprintf(err, "The %d socket of process %d listen() to port %d failed:> %m.",
				i, this_pid, auxPort, errno);
			Error_insert_File(LOG_URGENT, err);
			delete[]err;

			return false;
		}

		Listening_t* socketItem = new Listening_t;
		memset(socketItem, 0, sizeof(Listening_t));
		socketItem->fd = auxSocket;
		socketItem->port = auxPort;

		//监听端口设置成功，记录日志
		char* notice = new char[100];
		sprintf(notice, "The %d socket of process %d listening to port %d Succeeded.",
			i, this_pid, auxPort);
		Error_insert_File(LOG_NOTICE, notice);
		delete[]notice;

		m_listenSocketList.push_back(socketItem);
	}

	return true;
}



int MSocket::epoll_init()
{
	//创建epoll对象
	m_epollHandle = epoll_create(m_work_connections);
	if (m_epollHandle == -1)
	{
		Error_insert_File(LOG_FATAL, "epoll_init():> Epoll create failed.");
		exit(2);
	}

	InitConnections();

	//m_connection = new vConnection[m_work_connections];
	////为连接池分配空间

	//pConnection nexts = NULL;
	pConnection head;
	//连接池数组首地址
	
	vector<Listening_p>::iterator it;
	for (it = m_listenSocketList.begin(); it != m_listenSocketList.end(); it++)
	{
		head = Get_connection((*it)->fd);
		if (head == NULL)
		{
			//在Get_connection函数中已经记录了错误，此处不再记录
			exit(2);
		}

		//将连接对象与监听对象进行关联
		head->listening = (*it);
		(*it)->connection = head;

		//对监听端口的读事件建立处理方案
		head->rHandle = &MSocket::Event_accept;
	
		if (Epoll_Add_event((*it)->fd, EPOLL_CTL_ADD, EPOLLIN | EPOLLRDHUP, 0,
		 head) == -1)
		{
			exit(2);
		}
	}//end for

	return 1;
}


int MSocket::epoll_process_events(int time)
{
	//等待并获取事件
	int event = epoll_wait(m_epollHandle, m_events, MAX_EVENTS, time);
	//event 返回事件数量

	//有错误发生处理错误
	if (event == -1)
	{
		//....
		if (errno == EINTR)
		{
			Error_insert_File(LOG_WARN, "epoll_wait():> interrupted system call.");
			//错误产生正常，返回1
			return 1;
		}
		else
		{
			string err = "epoll_wait():>";
			err += strerror(errno);
			Error_insert_File(LOG_ERR, err);

			//异常错误
			return 0;
		}
	}

	//等待时间用尽
	if (event == 0)
	{
		//等待时间正常用尽
		if (time != -1)
		{
			return 1;
		}
		else
		{
			//异常状况：无限等待时间用尽
			Error_insert_File(LOG_ERR, "epoll_wait():> infinite waiting time exhausted.");
			return 0;
		}
	}

	//cerr << "Debug:> envents = " << event << " | "
		//<< "pid = " << this_pid << endl;

	//事件正常到达，进行处理
	pConnection cptr;
	//uintptr_t instance;
	uint32_t revents;
	
	for (int i = 0; i < event; i++)
	{
		//将先前存入连接池的连接和instance提取出来
		cptr = (pConnection)(m_events[i].data.ptr);

		//instance = (uintptr_t)cptr & 1;				
		////与运算，【xxxx...xx(1/0) & 0000...001】
		//cptr = (pConnection)((uintptr_t)cptr & (uintptr_t)~1);
		////【xxxxx...xx[1/0] & 1111...110】

		//if (cptr->fd == -1)
		//{
		//	//遇到已经处理的过期事件，直接跳过
		//	Error_insert_File(LOG_WARN, "epoll_wait:> skip expiration event.");
		//	continue;
		//}

		//if (cptr->instance != instance)
		//{
		//	Error_insert_File(LOG_WARN, 
		//		"epoll_wait:> skip expiration event(var: instance is changed).");
		//	continue;
		//}

		revents = m_events[i].events;
		
		//接收到用户的关闭连接请求等一些请求时
		if (revents & (EPOLLERR | EPOLLHUP))
		{
			revents |= EPOLLIN | EPOLLOUT;
			//做好关闭连接四次挥手的 读 写 准备
		}

		//接收到读事件
		if (revents & EPOLLIN)
		{
			//调用读事件处理函数Event_accept
			(this->*(cptr->rHandle))(cptr);
		}

		if (revents & EPOLLOUT)
		{
			if (revents & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
			{
				//客户端关闭
				--cptr->throwSendCount;
			}
			else
			{
				(this->*(cptr->wHandle))(cptr);
			}
		}
	}//end for
	
	return 1;
}


int MSocket::Epoll_Add_event(int sock, uint32_t eventTp,
	uint32_t flag, int addAction, pConnection cptr)
{
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));

	if (eventTp == EPOLL_CTL_ADD)
	{
		//epoll中增加节点
		//ev.data.ptr = (void*)(cptr);
		ev.events = flag;
		cptr->events = flag;
	}
	else if (eventTp == EPOLL_CTL_MOD)
	{
		//修改节点信息
		ev.events = cptr->events;
		
		if (addAction == 0)
		{
			//增加标记
			ev.events |= flag;
		}
		else if (addAction == 1)
		{
			//删除标记
			ev.events &= ~flag;
		}
		else
		{
			//完全覆盖标记
			ev.events = flag;
		}

		cptr->events = ev.events;//更新记录标记
	}
	else
	{
		//删除节点
		//....
		return 1;
	}

	ev.data.ptr = (void*)(cptr);

	if (epoll_ctl(m_epollHandle, eventTp, sock, &ev) == -1)
	{
		Error_insert_File(LOG_FATAL, "Epoll event addition failed.");
		return -1;
	}

	return 1;
}


//设置接收方式非阻塞
bool MSocket::Set_NoBlock(int sock)
{
	int nb = 1;
	
	//FIONBIO:设置非阻塞标记，0清除，1设置(nb)
	if (ioctl(sock, FIONBIO, &nb) == -1)
	{
		return false;
	}

	return true;
}


//关闭监听端口
void MSocket::Close_listenSocket()
{
	for (int i = 0; i < m_listen_port_count; i++)
	{
		close(m_listenSocketList[i]->fd);

		//关闭记录日志
		char* notice = new char[100];
		sprintf(notice, "The number %d listening port has been closed.",
			i);
		Error_insert_File(LOG_URGENT, notice);
		delete[]notice;
	}
}


void MSocket::ClearHeartQueue()
{
	multimap<time_t, MSG_Head*>::iterator it;

	for (it = m_heartQueue.begin(); it != m_heartQueue.end(); it++)
	{
		delete it->second;
		--m_heartSize;
	}
	
	m_heartQueue.clear();

	return;
}


void MSocket::ClearSendQueue()
{
	list<char*>::iterator it;

	for (it = m_sendQueue.begin(); it != m_sendQueue.end(); it++)
	{
		delete[](*it);
		--m_sendMsgCount;
	}

	m_sendQueue.clear();

	return;
}


void MSocket::WrokProcRelevent_Off()
{
	IsExit = true;

	vector<ThreadItem*>::iterator it;
	//等待所有线程终止
	for (it = m_auxthread.begin(); it != m_auxthread.end(); it++)
	{
		pthread_join((*it)->Handle, NULL);
	}

	//释放线程内存
	for (it = m_auxthread.begin(); it != m_auxthread.end(); it++)
	{
		if (*it)
		{
			delete (*it);
		}
	}
	m_auxthread.clear();
	
	//释放心跳队列
	ClearHeartQueue();

	//清空发送队列
	ClearSendQueue();

	//释放连接池
	ClearConnections();

	//释放互斥锁
	pthread_mutex_destroy(&m_connectMutex);
	pthread_mutex_destroy(&m_recoveryMutex);
	pthread_mutex_destroy(&m_sendMsgMutex);

	//释放信号量
	sem_destroy(&m_semSendMsg);

	return;
}


//析构函数释放内存
MSocket::~MSocket()
{
	for (vector<Listening_t*>::iterator it = m_listenSocketList.begin();
		it != m_listenSocketList.end(); it++)
	{
		delete (*it);
	}

	m_listenSocketList.clear();
	
	return;
}