#include "Logic.h"
#include "MLock.h"
#include "Error.h"

void MSocket::HeartQueue_ADD(pConnection cptr)
{
	time_t auxtime = time(NULL);
	auxtime += m_heartbeatWait;
	//加上等待时间之后的时间

	Mlock lock(&m_timeMutex);

	MSG_Head* msghdr = new MSG_Head;
	msghdr->conn = cptr;
	msghdr->serialNum = cptr->serialNum;

	//插入心跳队列
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
	//获取类指针
	ThreadItem* pthread = static_cast<ThreadItem*>(threadData);
	MSocket* psock = pthread->pThis;

	time_t absolute, current;
	int err;

	while (!IsExit)
	{
		//心跳队列不为空
		if (psock->m_heartSize > 0)
		{
			absolute = psock->m_firstHtime;
			current = time(NULL);

			//如果第一个时间到达预定时间
			if (absolute < current)
			{
				list<MSG_Head*> disposable;
				//存储可处理内容

				MSG_Head* temporary;

				err = pthread_mutex_lock(&psock->m_timeMutex);//设置互斥锁
				if (err != 0)
				{
					char* er = new char[100];
					sprintf(er, "Mutex lock set failed, the error code is %d.", err);
					Error_insert_File(LOG_FATAL, er);
					delete[]er;
				}

				//将所有超时节点找到并放入队列
				while ((temporary = psock->Overtime_Get(current)) != NULL)
				{
					disposable.push_back(temporary);
				}

				//解锁
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

	//队列为空
	if (m_heartSize == 0 || m_heartQueue.empty())
	{
		return NULL;
	}

	//再次判断时间是否超限
	time_t early = EarlyTime_Get();
	if (early <= current)
	{
		ansMsg = FirstTime_Remove();

		//对时间进行更新
		time_t newtime = current + m_heartbeatWait;

		//重新放入队列
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

	//取得队头对应消息指针，删除队头
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