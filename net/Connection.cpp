#include "Socket.h"
#include "Error.h"
#include "MLock.h"


Connection::Connection()
{
	serialNum = 0;
	pthread_mutex_init(&logicMutex, NULL);
}


Connection::~Connection()
{
	pthread_mutex_destroy(&logicMutex);
}


void Connection::GetOne()
{
	++serialNum;

	recvStat = _PKG_HD_INIT;
	//为获取的空闲连接赋值(初始化)

	bufptr = headInfo;
	recvlen = sizeof(PKG_Head);
	throwSendCount = 0;
	events = 0;
	isNewRecv = false;
	lastPingTime = time(NULL);

	floodCount = 0;
	floodCheckLast = 0;
	requestSent = 0;

	MsgInfo = NULL;
	sendptr = NULL;
	delptr = NULL;
}


void Connection::FreeOne()
{
	++serialNum;
	
	//释放前期手动分配过的内存
	if (MsgInfo != NULL)
	{
		delete[]MsgInfo;
		MsgInfo = NULL;
	}

	if (delptr == NULL && sendptr != NULL)
	{
		//delete[]sendptr;
		sendptr = NULL;
	}
	else if (delptr != NULL)
	{
		delete[]delptr;
		delptr = NULL;
		sendptr = NULL;
	}
	
	throwSendCount = 0;
}


void MSocket::InitConnections()
{
	pConnection pConn;

	for (int i = 0; i < m_work_connections; i++)
	{
		pConn = new Connection;  

		memset(pConn, 0, sizeof(Connection));

		pConn->GetOne();

		m_connection.push_back(pConn);
		m_fconnection.push_back(pConn);
	}

	m_connection_all = m_connection_free = m_connection.size();
	
	return;
}


pConnection MSocket::Get_connection(int sock)
{
	Mlock lock(&m_connectMutex);

	pConnection auxptr;

	//如果空闲链表中有链接
	if (!m_fconnection.empty())
	{
		auxptr = m_fconnection.front();
		m_fconnection.pop_front();
		--m_connection_free;

		auxptr->GetOne();
		auxptr->fd = sock;

		return auxptr;
	}

	//如果空闲链表中没有链接
	//创建一个返回

	auxptr = new Connection;

	memset(auxptr, 0, sizeof(Connection));
	auxptr->GetOne();
	m_connection.push_back(auxptr);
	++m_connection_all;

	auxptr->fd = sock;

	return auxptr;
}


void MSocket::Close_Connection(pConnection cptr)
{
	if (close(cptr->fd) == -1)
	{
		Error_insert_File(LOG_URGENT, "Close accept connection:> socket close failed.");
	}

	cptr->fd = -1;

	Free_connection(cptr);

	return;
}


void MSocket::IntoRecovery(pConnection cptr)
{
	list<pConnection>::iterator pos;
	bool isExist = false;

	//cerr << m_reConnection.size() << endl;

	Mlock lock(&m_recoveryMutex);

	for (pos = m_reConnection.begin(); pos != m_reConnection.end(); pos++)
	{
		if (*pos == cptr)
		{
			isExist = true;
			break;
		}
	}
	
	//保证连接不重复接入
	if (isExist)
	{
		return;
	}

	cptr->recyTime = time(NULL);
	++cptr->serialNum;

	m_reConnection.push_back(cptr);
	++m_connection_reco;
	--m_connection_work;

	return;
}


void* MSocket::ReConnection_Thread(void* threadData)
{
	//cerr << "thread recovery function is running." << endl;

	ThreadItem* pThread = static_cast<ThreadItem*>(threadData);
	MSocket* psock = pThread->pThis;
	//用于取得指向本线程池类的指针(因为是静态成员函数无法使用 this指针)

	time_t times;

	int err;
	
	list<pConnection>::iterator pos;
	pConnection cptr;

	while (true)
	{
		usleep(200 * 1000);
		//休息200ms
		//cerr << psock->m_connection_reco << endl;
		if (psock->m_connection_reco > 0)
		{
			times = time(NULL);

			if (psock->m_reConnection.begin() != psock->m_reConnection.end())
			{
				pos = psock->m_reConnection.begin();
				cptr = (*pos);

				//判断是否到了退出时间
				//cerr << cptr->recyTime + psock->m_reWaitTime - times << endl;
				if ((cptr->recyTime + psock->m_reWaitTime) > times && !IsExit)
				{
					sleep(2);
					continue;
				}

				//....
				
				//加锁
				err = pthread_mutex_lock(&psock->m_recoveryMutex);
				if (err != 0)
				{
					char* er = new char[100];
					sprintf(er, "Mutex lock set failed, the error code is %d.", err);
					Error_insert_File(LOG_FATAL, er);
					delete[]er;
				}

				psock->m_reConnection.erase(pos);

				psock->Free_connection(cptr);
				--psock->m_connection_reco;
				//将回收完的链接归还

				//cerr << "connection was return 1." << endl;

				err = pthread_mutex_unlock(&psock->m_recoveryMutex);
				if (err != 0)
				{
					char* er = new char[100];
					sprintf(er, "Mutex unlock set failed, the error code is %d.", err);
					Error_insert_File(LOG_FATAL, er);
					delete[]er;
				}
			}//end if (psock->m_reConnection.begin() != psock->m_reConnection.end())

		}//end if (psock->m_connection_reco > 0)
		
		if (IsExit == 1)
		{
			if (psock->m_connection_reco > 0)
			{
				err = pthread_mutex_lock(&psock->m_recoveryMutex);
				if (err != 0)
				{
					char* er = new char[100];
					sprintf(er, "Mutex lock set failed, the error code is %d.", err);
					Error_insert_File(LOG_FATAL, er);
					delete[]er;
				}

				while (psock->m_reConnection.begin() != psock->m_reConnection.end())
				{
					pos = psock->m_reConnection.begin();
					cptr = (*pos);

					//....

					psock->m_reConnection.erase(pos);

					psock->Free_connection(cptr);
					--psock->m_connection_reco;
					//将回收完的链接归还
				}

				//cerr << "connection was return 2." << endl;

				err = pthread_mutex_unlock(&psock->m_recoveryMutex);
				if (err != 0)
				{
					char* er = new char[100];
					sprintf(er, "Mutex unlock set failed, the error code is %d.", err);
					Error_insert_File(LOG_FATAL, er);
					delete[]er;
				}
			}//end if
			break;
		}//end if (IsExit == 1)
	}//end while

	return (void*)0;
}


bool MSocket::TestFlood(pConnection cptr)
{
	struct timeval sCurrent;
	uint64_t msCurrent;
	bool isFlood = false;

	//获取当前时间结构
	gettimeofday(&sCurrent, NULL);
	//时间结构转换为ms
	msCurrent = sCurrent.tv_sec * 1000 + sCurrent.tv_usec / 1000;

	if (msCurrent - cptr->floodCheckLast < m_floodInterval)
	{
		cptr->floodCount++;
		cptr->floodCheckLast = msCurrent;
	}
	else
	{
		cptr->floodCount = 0;
		cptr->floodCheckLast = msCurrent;
	}

	if (cptr->floodCount > m_floodPackages)
	{
		isFlood = true;
	}

	return isFlood;
}


void MSocket::Free_connection(pConnection cptr)
{
	Mlock lock(&m_connectMutex);

	cptr->FreeOne();

	m_fconnection.push_back(cptr);

	//空闲节点数+1
	++m_connection_free;

	return;
}


void MSocket::ClearConnections()
{
	pConnection paux;

	while (!m_connection.empty())
	{
		paux = m_connection.front();
		m_connection.pop_front();

		paux->~Connection();

		delete paux;
	}

	return;
}