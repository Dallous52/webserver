#include "Logic.h"
#include "MLock.h"
#include "Error.h"

void MSocket::HeartQueue_ADD(pConnection cptr)
{
	time_t auxtime = time(NULL);
	auxtime += m_heartbeatWait;
	//���ϵȴ�ʱ��֮���ʱ��

	Mlock lock(&m_timeMutex);

	MSG_Head* msghdr = new MSG_Head;
	msghdr->conn = cptr;
	msghdr->serialNum = cptr->serialNum;

	//������������
	m_heartQueue.insert(make_pair(auxtime, msghdr));
	m_heartSize++;
	m_firstHtime = EarlyTime_Get();
}


time_t MSocket::EarlyTime_Get()
{
	multimap<time_t, MSG_Head*>::iterator it;
	it = m_heartQueue.begin();

	return it->first;
}


void* MSocket::HeartbeatTime_Thread(void* threadData)
{
	//��ȡ��ָ��
	ThreadItem* pthread = static_cast<ThreadItem*>(threadData);
	MSocket* psock = pthread->pThis;

	time_t absolute, current;
	int err;

	while (!IsExit)
	{
		//�������в�Ϊ��
		if (psock->m_heartSize > 0)
		{
			absolute = psock->m_firstHtime;
			current = time(NULL);

			//�����һ��ʱ�䵽��Ԥ��ʱ��
			if (absolute < current)
			{
				list<MSG_Head*> disposable;
				//�洢�ɴ�������

				MSG_Head* temporary;

				err = pthread_mutex_lock(&psock->m_timeMutex);//���û�����
				if (err != 0)
				{
					char* er = new char[100];
					sprintf(er, "Mutex lock set failed, the error code is %d.", err);
					Error_insert_File(LOG_FATAL, er);
					delete[]er;
				}

				//�����г�ʱ�ڵ��ҵ����������
				while ((temporary = psock->Overtime_Get(current)) != NULL)
				{
					disposable.push_back(temporary);
				}

				//����
				err = pthread_mutex_unlock(&psock->m_timeMutex);
				if (err != 0)
				{
					char* er = new char[100];
					sprintf(er, "Mutex unlock set failed, the error code is %d.", err);
					Error_insert_File(LOG_FATAL, er);
					delete[]er;
				}

				MSG_Head* auxmsg;
				while (!disposable.empty())
				{
					auxmsg = disposable.front();
					disposable.pop_front();

					psock->PingTime_Check(auxmsg, current);
				}
			}//end if (absolute < current)
		}//end if (psock->m_heartSize > 0)
		
		usleep(500 * 1000);
	}//end while

	return NULL;
}


MSG_Head* MSocket::Overtime_Get(time_t current)
{
	MSG_Head* ansMsg;

	//����Ϊ��
	if (m_heartSize == 0 || m_heartQueue.empty())
	{
		return NULL;
	}

	//�ٴ��ж�ʱ���Ƿ���
	time_t early = EarlyTime_Get();
	if (early <= current)
	{
		ansMsg = FirstTime_Remove();

		//��ʱ����и���
		time_t newtime = current + m_heartbeatWait;

		//���·������
		MSG_Head* newMsg = new MSG_Head;
		newMsg->conn = ansMsg->conn;
		newMsg->serialNum = ansMsg->serialNum;
		m_heartQueue.insert(make_pair(newtime, newMsg));
		m_heartSize++;

		if (m_heartSize > 0)
		{
			m_firstHtime = EarlyTime_Get();
		}

		return ansMsg;
	}
	return NULL;
}


MSG_Head* MSocket::FirstTime_Remove()
{
	if (m_heartSize <= 0)
	{
		return NULL;
	}

	//ȡ�ö�ͷ��Ӧ��Ϣָ�룬ɾ����ͷ
	multimap<time_t, MSG_Head*>::iterator it = m_heartQueue.begin();
	MSG_Head* temporary = it->second;
	m_heartQueue.erase(it);
	--m_heartSize;

	return temporary;
}


void MSocket::ActiveShutdown(pConnection cptr)
{
	if (m_heartbeatStart)
	{
		HeardQueue_Delete(cptr);
	}
	if (cptr->fd != -1)
	{
		close(cptr->fd);
		cptr->fd = -1;
	}

	if (cptr->throwSendCount > 0)
	{
		cptr->throwSendCount = 0;
	}

	IntoRecovery(cptr);

	return;
}


void MSocket::HeardQueue_Delete(pConnection cptr)
{
	multimap<time_t, MSG_Head*>::iterator pos;

	Mlock lock(&m_timeMutex);

	//cerr << m_heartQueue.size() << endl;

	for (pos = m_heartQueue.begin(); pos != m_heartQueue.end(); pos++)
	{
		if (pos->second->conn == cptr)
		{
			delete pos->second;
			m_heartQueue.erase(pos);
			--m_heartSize;

			if (m_heartQueue.empty())
			{
				break;
			}
			else
			{
				pos = m_heartQueue.begin();
			}
		}
	}

	if (m_heartSize > 0)
	{
		m_firstHtime = EarlyTime_Get();
	}
	
	return;
}


void MSocket::PingTime_Check(MSG_Head* pmsghdr, time_t current)
{
	delete pmsghdr;
}