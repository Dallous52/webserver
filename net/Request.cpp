#include "Socket.h"
#include "Manage.h"
#include "Error.h"
#include "Process.h"
#include "MLock.h"

void MSocket::Read_requestHandle(pConnection cptr)
{
	ssize_t recpro = ReceiveProc(cptr, cptr->bufptr, cptr->recvlen);

	bool isFlood = false;

	if (recpro <= 0)
	{
		return;
	}

	if (cptr->recvStat == _PKG_HD_INIT)
	{
		if (recpro == m_PKG_Hlen)
		{
			//包头以接收完整，进行处理
			Request_Handle_A(cptr, isFlood);
		}
		else
		{
			//包头未接收完整，调整状态
			cptr->recvStat = _PKG_HD_RECEIVE;
			cptr->bufptr += recpro;
			cptr->recvlen -= recpro;
		}
	}
	else if (cptr->recvStat == _PKG_HD_RECEIVE)
	{
		if (recpro == cptr->recvlen)
		{
			//包头以接收完整，进行处理
			Request_Handle_A(cptr, isFlood);
		}
		else
		{
			//包头仍未接收完整，调整状态
			cptr->bufptr += recpro;
			cptr->recvlen -= recpro;
		}
	}
	else if (cptr->recvStat == _PKG_BD_INIT)
	{
		if (recpro == cptr->recvlen)
		{
			if (m_floodCheckStart)
			{
				isFlood = TestFlood(cptr);
			}

			//整包接收完整，进行处理
			Request_Handle_L(cptr, isFlood);
		}
		else
		{
			//整包未接收完整，调整状态
			cptr->recvStat = _PKG_BD_RECEIVE;
			cptr->bufptr += recpro;
			cptr->recvlen -= recpro;
		}
	}
	else if (cptr->recvStat == _PKG_BD_RECEIVE)
	{
		if (recpro == cptr->recvlen)
		{
			if (m_floodCheckStart)
			{
				isFlood = TestFlood(cptr);
			}

			//整包接收完整，进行处理
			Request_Handle_L(cptr, isFlood);
		}
		else
		{
			//整包仍未接收完整，调整状态
			cptr->bufptr += recpro;
			cptr->recvlen -= recpro;
		}
	}

	if (isFlood)
	{
		Error_insert_File(LOG_CRIT, "Flooding attack detected, close connection");
		ActiveShutdown(cptr);
	}

	return;
}


ssize_t MSocket::ReceiveProc(pConnection cptr, char* buffer, ssize_t buflen)
{
	ssize_t n = recv(cptr->fd, buffer, buflen, 0);
	//recv系统函数用于接收socket数据

	if (n == 0)
	{
		//客户端正常关闭，回收连接
		Error_insert_File(LOG_NOTICE, "The client shuts down normally.");

		ActiveShutdown(cptr);

		return -1;
	}

	//错误处理
	if (n < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			//在ET模式下可能出现，表示没有收到数据
			Error_insert_File(LOG_WARN, "receive data:> no data in Socket buffer.");
			return -1;
		}

		if (errno == EINTR)
		{
			Error_insert_File(LOG_WARN, "receive data:> interrupted system call.");
			return -1;
		}

		if (errno == ECONNRESET)
		{
			//对等放重置连接
			//由用户强制关闭导致的正常情况
			//do nothing
		}
		else
		{
			string err = "receive data:> ";
			err += strerror(errno);
			Error_insert_File(LOG_SERIOUS, err);
		}

		Error_insert_File(LOG_NOTICE, "The client abnormal shutdow.");

		ActiveShutdown(cptr);

		return -1;
	}//end if

	return n;
}


void MSocket::Request_Handle_A(pConnection cptr, bool& isFlood)
{
	PKG_Head* pPkgHead;
	pPkgHead = (PKG_Head*)cptr->headInfo;

	unsigned short pkgLen = ntohs(pPkgHead->pkgLen);
	//ntohs 将2字节的网络字节序转换为主机字节须
	//net to host short  (ntohs) 2字节
	//net to host long   (ntohl) 4字节

	//恶意包/错误包处理
	if (pkgLen < m_PKG_Hlen || pkgLen > (_PKG_MAX_LENGTH - 1000))
	{
		cptr->recvStat = _PKG_HD_INIT;
		cptr->bufptr = cptr->headInfo;
		cptr->recvlen = m_PKG_Hlen;
	}
	else
	{
		//对合法包头进行处理

		//为信息包分配内存 消息头 + 包头 + 包体
		char* auxBuffer = new char[m_MSG_Hlen + pkgLen];

		cptr->isNewRecv = true;
		cptr->MsgInfo = auxBuffer;

		//填写消息头
		MSG_Head* auxMsg = (MSG_Head*)auxBuffer;
		auxMsg->conn = cptr;
		auxMsg->serialNum = cptr->serialNum;

		//填写包头
		auxBuffer += m_MSG_Hlen;
		memcpy(auxBuffer, pPkgHead, m_PKG_Hlen);
		//拷贝包头

		if (pkgLen == m_PKG_Hlen)
		{
			if (m_floodCheckStart)
			{
				isFlood = TestFlood(cptr);
			}
			//无包体，只有包头
			Request_Handle_L(cptr, isFlood);
		}
		else
		{
			//开始包体接收
			cptr->recvStat = _PKG_BD_INIT;
			cptr->bufptr = auxBuffer + m_PKG_Hlen;
			cptr->recvlen = pkgLen - m_PKG_Hlen;
		}
	}//end if

	return;
}


void MSocket::Request_Handle_L(pConnection cptr, bool& isFlood)
{
	if (!isFlood)
	{
		//将信息包放入信息队列中并处理
		mthreadpool.GetMassage_And_Proc(cptr->MsgInfo);
	}
	else
	{
		delete[]cptr->MsgInfo;
	}

	//完成一个包的接收，还原为初始值
	cptr->isNewRecv = false;
	cptr->MsgInfo = NULL;
	cptr->recvStat = _PKG_HD_INIT;
	cptr->bufptr = cptr->headInfo;
	cptr->recvlen = m_PKG_Hlen;

	return;
}


ssize_t MSocket::SendProc(pConnection cptr, char* buffer, ssize_t buflen)
{
	ssize_t n;

	while (true)
	{
		n = send(cptr->fd, buffer, buflen, 0);

		if (n > 0)
		{
			return n;
		}

		if (n == 0)
		{
			//对端连接断开
			return 0;
		}

		if (errno == EAGAIN)
		{
			return -1;
		}

		if (errno == EINTR)
		{
			//发生未知异常
			Error_insert_File(LOG_ERR, "Send Message:> func SendProc() [errno == EINTR].");
		}
		else
		{
			//其他错误
			return -2;
		}
	}//end while
}


void MSocket::Write_requestHandle(pConnection cptr)
{
	ssize_t sended = SendProc(cptr, cptr->sendptr, cptr->sendLen);

	if (sended > 0 && sended != cptr->sendLen)
	{
		//数据没有发送完
		cptr->sendptr = cptr->sendptr + sended;
		cptr->sendLen -= sended;
		return;
	}
	else if (sended == -1)
	{
		//系统通知可以发送数据，缓冲区却满
		//意料外异常
		Error_insert_File(LOG_ERR, 
			"Write request:> send buffer is full.");
		return;
	}

	if (sended > 0 && sended == cptr->sendLen)
	{
		//数据发送完毕，EPOLLOUT标记清空
		if (Epoll_Add_event(cptr->fd, EPOLL_CTL_MOD, EPOLLOUT,
			1, cptr) == -1)
		{
			Error_insert_File(LOG_CRIT,
				"Send message:> epoll mod delete EPOLLOUT failed.");
		}

		//cerr << "Notice:> Detention message send compelete." << endl;
	}

	delete[]cptr->delptr;
	cptr->delptr = NULL;
	--cptr->throwSendCount;

	//将信号量的值加一,让卡在sem_wait的线程继续执行
	if (sem_post(&m_semSendMsg) == -1)
	{
		Error_insert_File(LOG_CRIT,
			"MsgSend func:> sem_post made a mistake.");
	}
	
	return;
}


void MSocket::MsgSend(char* sendBuf)
{
	//发送消息互斥
	Mlock lock(&m_sendMsgMutex);

	//防止滞留发送数据包过多
	if (m_sendMsgCount > m_MAX_Send)
	{
		m_discardPkg++;
		delete[]sendBuf;

		return;
	}

	//检查连接是否存在恶意请求不收包的行为
	MSG_Head* pmsghdr = (MSG_Head*)sendBuf;
	pConnection aptr = pmsghdr->conn;

	//单个连接滞留的未发送数据过多
	if (aptr->requestSent > m_MAX_rqSend)
	{
		m_discardPkg++;
		delete[]sendBuf;

		ActiveShutdown(aptr);

		return;
	}


	//消息放入消息队列
	++aptr->requestSent;
	m_sendQueue.push_back(sendBuf);
	++m_sendMsgCount;

	//将信号量的值加一,让卡在sem_wait的线程继续执行
	if (sem_post(&m_semSendMsg) == -1)
	{
		Error_insert_File(LOG_CRIT,
			"MsgSend func:> sem_post made a mistake.");
	}

	return;
}



void* MSocket::MessageSend_Thread(void* threadData)
{
	//获取类指针
	ThreadItem* pthread = static_cast<ThreadItem*>(threadData);
	MSocket* psock = pthread->pThis;

	int err;
	int sended;
	char* pMsgbuf;

	MSG_Head* msghdr;
	PKG_Head* pkghdr;
	pConnection cptr;
	size_t pkglen;

	list<char*>::iterator pos, posEnd, aupos;

	while (IsExit == 0)
	{
		//使用信号量让线程等待
		if (sem_wait(&psock->m_semSendMsg) == -1)
		{
			if (errno == EINTR)
			{
				Error_insert_File(LOG_WARN,
					"epoll_wait():> interrupted system call.");
			}
		}

		//若是程序要退出
		if (IsExit != 0)
		{
			break;
		}

		if (psock->m_sendMsgCount > 0)
		{
			//加锁
			err = pthread_mutex_lock(&psock->m_sendMsgMutex);
			if (err != 0)
			{
				char* er = new char[100];
				sprintf(er, "Mutex lock set failed, the error code is %d.", err);
				Error_insert_File(LOG_FATAL, er);
				delete[]er;
			}

			pos = psock->m_sendQueue.begin();
			posEnd = psock->m_sendQueue.end();

			while (pos != posEnd)
			{
				pMsgbuf = (*pos);

				//从发送数据缓冲区中获取信息
				msghdr = (MSG_Head*)pMsgbuf;
				pkghdr = (PKG_Head*)(pMsgbuf + psock->m_MSG_Hlen);
				cptr = msghdr->conn;

				//判断排除过期包
				if (cptr->serialNum != msghdr->serialNum)
				{
					//如果过期，删除迭代器指向的消息
					aupos = pos;
					pos++;
					psock->m_sendQueue.erase(aupos);
					--psock->m_sendMsgCount;
					delete[]pMsgbuf;
					continue;
				}

				if (cptr->throwSendCount > 0)
				{
					//拥有该标记的信息由系统信号激发发送活动
					//此处直接跳过
					pos++;
					continue;
				}

				//以下为消息发送
				--cptr->requestSent;

				cptr->delptr = pMsgbuf;

				aupos = pos;
				pos++;
				psock->m_sendQueue.erase(aupos);
				--psock->m_sendMsgCount;

				cptr->sendptr = (char*)pkghdr;
				pkglen = ntohs(pkghdr->pkgLen);
				cptr->sendLen = pkglen;

				sended = psock->SendProc(cptr, cptr->sendptr, cptr->sendLen);

				if (sended > 0)
				{
					if (sended == cptr->sendLen)
					{
						delete[]cptr->delptr;
						cptr->delptr = NULL;
						//cerr << "SendProc:> Message send success." << endl;
					}
					else
					{
						//更新发送零量
						cptr->sendptr = cptr->sendptr + sended;
						cptr->sendLen -= sended;

						//依靠系统通知发送信息
						++cptr->throwSendCount;

						if (psock->Epoll_Add_event(cptr->fd, EPOLL_CTL_MOD, EPOLLOUT,
							0, cptr) == -1)
						{
							Error_insert_File(LOG_CRIT,
								"Send message:> epoll mod add EPOLLOUT failed.");
						}

						Error_insert_File(LOG_INFO,
							"Send message:> send buffer is full, use system call.");
					}//end if

					continue;
				}//end if (sended > 0)

				else if (sended == 0)
				{
					//预料之外的异常错误
					Error_insert_File(LOG_ERR, "Send message:> sended is equal 0.");

					delete[]cptr->delptr;
					cptr->delptr = NULL;

					continue;
				}

				else if (sended == -1)
				{
					//缓冲区满，依靠系统通知发送信息
					++cptr->throwSendCount;

					if (psock->Epoll_Add_event(cptr->fd, EPOLL_CTL_MOD, EPOLLOUT,
						0, cptr) == -1)
					{
						Error_insert_File(LOG_CRIT,
							"Send message:> epoll mod EPOLLOUT failed.");
					}

					Error_insert_File(LOG_INFO,
						"Send message:> send buffer is full, use system call.");

					continue;
				}//end else if

				else
				{
					//通常为对端断开,返回为-2
					delete[]cptr->delptr;
					cptr->delptr = NULL;

					continue;
				}
			}// end while

			err = pthread_mutex_unlock(&psock->m_sendMsgMutex);
			if (err != 0)
			{
				char* er = new char[100];
				sprintf(er, "Mutex unlock set failed, the error code is %d.", err);
				Error_insert_File(LOG_FATAL, er);
				delete[]er;
			}
		}//end if (psock->m_sendMsgCount > 0)
	}//end while (IsExit != 0)

	return (void*)0;
}


//父类不进行实现，用于子类继承
void MSocket::ThreadReceive_Proc(char* msgInfo)
{
	return;
}