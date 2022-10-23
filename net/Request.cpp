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
			//��ͷ�Խ������������д���
			Request_Handle_A(cptr, isFlood);
		}
		else
		{
			//��ͷδ��������������״̬
			cptr->recvStat = _PKG_HD_RECEIVE;
			cptr->bufptr += recpro;
			cptr->recvlen -= recpro;
		}
	}
	else if (cptr->recvStat == _PKG_HD_RECEIVE)
	{
		if (recpro == cptr->recvlen)
		{
			//��ͷ�Խ������������д���
			Request_Handle_A(cptr, isFlood);
		}
		else
		{
			//��ͷ��δ��������������״̬
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

			//�����������������д���
			Request_Handle_L(cptr, isFlood);
		}
		else
		{
			//����δ��������������״̬
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

			//�����������������д���
			Request_Handle_L(cptr, isFlood);
		}
		else
		{
			//������δ��������������״̬
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
	//recvϵͳ�������ڽ���socket����

	if (n == 0)
	{
		//�ͻ��������رգ���������
		Error_insert_File(LOG_NOTICE, "The client shuts down normally.");

		ActiveShutdown(cptr);

		return -1;
	}

	//������
	if (n < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			//��ETģʽ�¿��ܳ��֣���ʾû���յ�����
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
			//�Եȷ���������
			//���û�ǿ�ƹرյ��µ��������
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
	//ntohs ��2�ֽڵ������ֽ���ת��Ϊ�����ֽ���
	//net to host short  (ntohs) 2�ֽ�
	//net to host long   (ntohl) 4�ֽ�

	//�����/���������
	if (pkgLen < m_PKG_Hlen || pkgLen > (_PKG_MAX_LENGTH - 1000))
	{
		cptr->recvStat = _PKG_HD_INIT;
		cptr->bufptr = cptr->headInfo;
		cptr->recvlen = m_PKG_Hlen;
	}
	else
	{
		//�ԺϷ���ͷ���д���

		//Ϊ��Ϣ�������ڴ� ��Ϣͷ + ��ͷ + ����
		char* auxBuffer = new char[m_MSG_Hlen + pkgLen];

		cptr->isNewRecv = true;
		cptr->MsgInfo = auxBuffer;

		//��д��Ϣͷ
		MSG_Head* auxMsg = (MSG_Head*)auxBuffer;
		auxMsg->conn = cptr;
		auxMsg->serialNum = cptr->serialNum;

		//��д��ͷ
		auxBuffer += m_MSG_Hlen;
		memcpy(auxBuffer, pPkgHead, m_PKG_Hlen);
		//������ͷ

		if (pkgLen == m_PKG_Hlen)
		{
			if (m_floodCheckStart)
			{
				isFlood = TestFlood(cptr);
			}
			//�ް��壬ֻ�а�ͷ
			Request_Handle_L(cptr, isFlood);
		}
		else
		{
			//��ʼ�������
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
		//����Ϣ��������Ϣ�����в�����
		mthreadpool.GetMassage_And_Proc(cptr->MsgInfo);
	}
	else
	{
		delete[]cptr->MsgInfo;
	}

	//���һ�����Ľ��գ���ԭΪ��ʼֵ
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
			//�Զ����ӶϿ�
			return 0;
		}

		if (errno == EAGAIN)
		{
			return -1;
		}

		if (errno == EINTR)
		{
			//����δ֪�쳣
			Error_insert_File(LOG_ERR, "Send Message:> func SendProc() [errno == EINTR].");
		}
		else
		{
			//��������
			return -2;
		}
	}//end while
}


void MSocket::Write_requestHandle(pConnection cptr)
{
	ssize_t sended = SendProc(cptr, cptr->sendptr, cptr->sendLen);

	if (sended > 0 && sended != cptr->sendLen)
	{
		//����û�з�����
		cptr->sendptr = cptr->sendptr + sended;
		cptr->sendLen -= sended;
		return;
	}
	else if (sended == -1)
	{
		//ϵͳ֪ͨ���Է������ݣ�������ȴ��
		//�������쳣
		Error_insert_File(LOG_ERR, 
			"Write request:> send buffer is full.");
		return;
	}

	if (sended > 0 && sended == cptr->sendLen)
	{
		//���ݷ�����ϣ�EPOLLOUT������
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

	//���ź�����ֵ��һ,�ÿ���sem_wait���̼߳���ִ��
	if (sem_post(&m_semSendMsg) == -1)
	{
		Error_insert_File(LOG_CRIT,
			"MsgSend func:> sem_post made a mistake.");
	}
	
	return;
}


void MSocket::MsgSend(char* sendBuf)
{
	//������Ϣ����
	Mlock lock(&m_sendMsgMutex);

	//��ֹ�����������ݰ�����
	if (m_sendMsgCount > m_MAX_Send)
	{
		m_discardPkg++;
		delete[]sendBuf;

		return;
	}

	//��������Ƿ���ڶ��������հ�����Ϊ
	MSG_Head* pmsghdr = (MSG_Head*)sendBuf;
	pConnection aptr = pmsghdr->conn;

	//��������������δ�������ݹ���
	if (aptr->requestSent > m_MAX_rqSend)
	{
		m_discardPkg++;
		delete[]sendBuf;

		ActiveShutdown(aptr);

		return;
	}


	//��Ϣ������Ϣ����
	++aptr->requestSent;
	m_sendQueue.push_back(sendBuf);
	++m_sendMsgCount;

	//���ź�����ֵ��һ,�ÿ���sem_wait���̼߳���ִ��
	if (sem_post(&m_semSendMsg) == -1)
	{
		Error_insert_File(LOG_CRIT,
			"MsgSend func:> sem_post made a mistake.");
	}

	return;
}



void* MSocket::MessageSend_Thread(void* threadData)
{
	//��ȡ��ָ��
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
		//ʹ���ź������̵߳ȴ�
		if (sem_wait(&psock->m_semSendMsg) == -1)
		{
			if (errno == EINTR)
			{
				Error_insert_File(LOG_WARN,
					"epoll_wait():> interrupted system call.");
			}
		}

		//���ǳ���Ҫ�˳�
		if (IsExit != 0)
		{
			break;
		}

		if (psock->m_sendMsgCount > 0)
		{
			//����
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

				//�ӷ������ݻ������л�ȡ��Ϣ
				msghdr = (MSG_Head*)pMsgbuf;
				pkghdr = (PKG_Head*)(pMsgbuf + psock->m_MSG_Hlen);
				cptr = msghdr->conn;

				//�ж��ų����ڰ�
				if (cptr->serialNum != msghdr->serialNum)
				{
					//������ڣ�ɾ��������ָ�����Ϣ
					aupos = pos;
					pos++;
					psock->m_sendQueue.erase(aupos);
					--psock->m_sendMsgCount;
					delete[]pMsgbuf;
					continue;
				}

				if (cptr->throwSendCount > 0)
				{
					//ӵ�иñ�ǵ���Ϣ��ϵͳ�źż������ͻ
					//�˴�ֱ������
					pos++;
					continue;
				}

				//����Ϊ��Ϣ����
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
						//���·�������
						cptr->sendptr = cptr->sendptr + sended;
						cptr->sendLen -= sended;

						//����ϵͳ֪ͨ������Ϣ
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
					//Ԥ��֮����쳣����
					Error_insert_File(LOG_ERR, "Send message:> sended is equal 0.");

					delete[]cptr->delptr;
					cptr->delptr = NULL;

					continue;
				}

				else if (sended == -1)
				{
					//��������������ϵͳ֪ͨ������Ϣ
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
					//ͨ��Ϊ�Զ˶Ͽ�,����Ϊ-2
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


//���಻����ʵ�֣���������̳�
void MSocket::ThreadReceive_Proc(char* msgInfo)
{
	return;
}