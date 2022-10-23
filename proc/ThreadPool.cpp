#include "ThreadPool.h"
#include "Error.h"
#include "Manage.h"

//初始化全局变量
pthread_mutex_t MThreadPool::m_pMutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t MThreadPool::m_pCond = PTHREAD_COND_INITIALIZER;

bool MThreadPool::m_isExit = false;


MThreadPool::MThreadPool()
{
	m_threadRunning = 0;
	
	m_oldTime = 0;
}


//线程创建
bool MThreadPool::Create(int num)
{
	ThreadItem* newthread;

	int err;

	m_CreateNum = num;

	for (int i = 0; i < m_CreateNum; i++)
	{
		newthread = new	ThreadItem(this);
		m_threadpool.push_back(newthread);
		
		//调用系统函数创建线程		ThreadFunc为线程入口函数
		err = pthread_create(&newthread->Handle, NULL, ThreadFunc, newthread);
		if (err != 0)
		{
			char* er = new char[100];
			sprintf(er, "Thread create failed, the error code is %d.", err);
			Error_insert_File(LOG_FATAL, er);
			delete[]er;

			return false;
		}
	}//end for

	//取个别名
	vector<ThreadItem*>& auxtp = m_threadpool;
	
	for (int i = 0; i < auxtp.size(); i++)
	{
		if (!auxtp[i]->isRunning)
		{
			usleep(100 * 1000);
			//休息 100 * 1000 微秒
			i--;
		}
	}

	return true;
}


void MThreadPool::GetMassage_And_Proc(char* buffer)
{
	//启用互斥锁
	int err = pthread_mutex_lock(&m_pMutex);
	if (err != 0)
	{
		char* er = new char[100];
		sprintf(er, "Mutex lock set failed, the error code is %d.", err);
		Error_insert_File(LOG_FATAL, er);
		delete[]er;
	}


	m_MsgQueue.push_back(buffer);

	++m_MsgQueueCount;

	//取消互斥锁
	err = pthread_mutex_unlock(&m_pMutex);
	if (err != 0)
	{
		char* er = new char[100];
		sprintf(er, "Mutex unlock set failed, the error code is %d.", err);
		Error_insert_File(LOG_FATAL, er);
		delete[]er;
	}

	Call();

	return;
}


void* MThreadPool::ThreadFunc(void* threadData)
{
	ThreadItem* auxThread = static_cast<ThreadItem*>(threadData);
	MThreadPool* cpthreadpool = auxThread->pThis;
	//用于取得指向本线程池类的指针(因为是静态成员函数无法使用 this指针)

	int err;

	pthread_t tid = pthread_self();
	//获取自身pid

	while (true)
	{
		err = pthread_mutex_lock(&m_pMutex);//设置互斥锁
		if (err != 0)
		{
			char* er = new char[100];
			sprintf(er, "Mutex lock set failed, the error code is %d.", err);
			Error_insert_File(LOG_FATAL, er);
			delete[]er;
		}

		//用于控制线程在接收消息时启动，处理完成后根据消息队列是否为空自动判断是否执行
		while (cpthreadpool->m_MsgQueue.size() == 0 && m_isExit == false)
		{
			if (auxThread->isRunning == false)
			{
				auxThread->isRunning = true;
				//只有运行时才允许调用StopAll函数
			}

			pthread_cond_wait(&m_pCond, &m_pMutex);
			//若不满足条件，线程将卡在此函数中等待
			//两个参数为静态成员可直接引用
		}

		//如果进程正在运行时需要退出
		//m_isExit条件会为true
		if (m_isExit)
		{
			pthread_mutex_unlock(&m_pMutex);
			break;
		}

		//线程满足运行条件准备运行
		
		char* workbuf = cpthreadpool->m_MsgQueue.front();
		cpthreadpool->m_MsgQueue.pop_front();
		--cpthreadpool->m_MsgQueueCount;

		err = pthread_mutex_unlock(&m_pMutex);
		if (err != 0)
		{
			char* er = new char[100];
			sprintf(er, "Mutex unlock set failed, the error code is %d.", err);
			Error_insert_File(LOG_FATAL, er);
			delete[]er;
		}

		
		//运行到此说明线程可以进行数据处理
		//线程运行数原子量+1
		++cpthreadpool->m_threadRunning;

		//对消息进行处理
		msocket.ThreadReceive_Proc(workbuf);

		delete[]workbuf;
		--cpthreadpool->m_threadRunning;

	}//end while
	
	return (void*)0;
}


void MThreadPool::StopAll()
{
	//若程序已结束直接退出
	if (m_isExit)
	{
		return;
	}

	m_isExit = true;

			
	//唤醒由pthread_cond_wait(&m_pCond, &m_pMutex);卡着的所有线程
	int err = pthread_cond_broadcast(&m_pCond);
	if (err != 0)
	{
		char* er = new char[100];
		sprintf(er, "thread pool exit:> broadcast failed, the error code is %d.", err);
		Error_insert_File(LOG_FATAL, er);
		delete[]er;

		return;
	}

	vector<ThreadItem*>::iterator it;
	for (it = m_threadpool.begin(); it != m_threadpool.end(); it++)
	{
		pthread_join((*it)->Handle, NULL);
		//等待一个进程终止
	}

	//释放互斥锁等变量
	pthread_mutex_destroy(&m_pMutex);
	pthread_cond_destroy(&m_pCond);
	
	//释放线程所占空间
	for (it = m_threadpool.begin(); it != m_threadpool.end(); it++)
	{
		if (*it)
		{
			delete* it;
		}
	}
	m_threadpool.clear();

	Error_insert_File(LOG_NOTICE, "All threads exit normally.");

	return;
}


void MThreadPool::Call()
{
	int err = pthread_cond_signal(&m_pCond);
	//上述函数用于唤醒一个等待m_pCond的进程
	if (err != 0)
	{
		char* er = new char[100];
		sprintf(er, "thread call:> thread weak up failed, the error code is %d.", err);
		Error_insert_File(LOG_FATAL, er);
		delete[]er;
	}
	
	if (m_CreateNum == m_threadRunning)
	{
		//正在运行的线程数 = 线程总数
		//线程数量不足
		time_t auxtime = time(NULL);
		if (auxtime - m_oldTime > 10)
		{
			m_oldTime = auxtime;

			Error_insert_File(LOG_CRIT, 
				"thread call:> Insufficient threads, please expand the thread pool.");
		}
	}

	return;
}


void MThreadPool::ClearMsgQueue()
{
	char* auxPoint;

	while (!m_MsgQueue.empty())
	{
		auxPoint = m_MsgQueue.front();
		m_MsgQueue.pop_front();

		delete[]auxPoint;
	}
}


MThreadPool::~MThreadPool()
{
	//清理消息队列遗留
	ClearMsgQueue();
}