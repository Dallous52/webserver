#ifndef ThreadPool
#define ThreadPool

#include "Globar.h"

class MThreadPool
{
public:

	MThreadPool();

	~MThreadPool();

public:

	//创建出num个线程
	bool Create(int num);

	//使线程池中所有的线程退出
	void StopAll();

	//从线程池掉线程执行任务
	void Call();

	//获取消息队列中的消息，并给到处理函数	
	void GetMassage_And_Proc(char* buffer);

private:

	//新线程的线程入口函数
	static void* ThreadFunc(void *threadData);

	//清空消息队列
	void ClearMsgQueue();

private:

	//线程结构定义
	struct ThreadItem
	{
		pthread_t Handle;			//线程句柄
		MThreadPool* pThis;			//线程池指针

		bool isRunning;				//标记是否开始运行，只有开始运行才可通过StopAll关闭

		//构造函数通过默认参数初始化成员
		ThreadItem(MThreadPool* pthis) :pThis(pthis), isRunning(false) {}

		~ThreadItem() {}
	};

private:

	int m_CreateNum;					//要创建的线程数量
	vector<ThreadItem*> m_threadpool;	//线程池内存载体

	static pthread_mutex_t m_pMutex;	//互斥锁
	static pthread_cond_t m_pCond;		//线程同步条件变量
	static bool m_isExit;				//线程退出标志，true为退出
	atomic<int> m_threadRunning;		//运行中线程数
	time_t m_oldTime;					//上次发生线程不够用事件的时间

	list<char*> m_MsgQueue;				//消息队列
	int m_MsgQueueCount;				//消息队列大小

};
#endif // !ThreadPool

