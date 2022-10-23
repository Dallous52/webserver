#ifndef Sockets
#define Sockets

#include "Globar.h"

//服务器可以积压未处理完的连接请求个数
#define ListenBackLog 511							  

//事件最大处理数
#define MAX_EVENTS	  512	

//*****************通讯状态相关定义*******************

#define _PKG_MAX_LENGTH 30000						  //每个包最大长度

//通讯收包状态定义

#define _PKG_HD_INIT    0							  //包头接收准备状态
#define _PKG_HD_RECEIVE 1							  //包头接收工作状态
#define _PKG_BD_INIT    3							  //包体接收准备状态
#define _PKG_BD_RECEIVE 4						      //包体接收工作状态

#define HEAD_BUFFER		20							  //包头接收缓冲区

#pragma pack(1)//设置系统默认对齐字节序为 1

//包头结构定义
typedef struct PKG_Head
{
	unsigned short pkgLen;					//报文总长
	unsigned short mesgCode;				//用于区分报文内容

	int	crc32;								//crc32错误校验【循环冗余校验】
}PKG_Head;

#pragma pack()//还原系统默认对其字符序

//****************************************************

typedef struct Connection  vConnection, *pConnection;	//指向连接信息结构指针

typedef struct Listening   Listening_t, *Listening_p;	//指向监听端口结构指针

typedef class MSocket MSocket;

//事件处理函数指针
typedef void (MSocket::* Event_handle)(pConnection head);


//连接池内连接结构
struct Connection
{
	//******初始化与释放******
	Connection();
	~Connection();
	void GetOne();					//当获取或创建一个连接时执行此函数初始化成员
	void FreeOne();					//当要释放一个函数时调用此函数
	//************************


	//******连接基本信息******
	int fd;							//套接字句柄
	uint32_t events;				//本连接的事件处理类型
	Listening_p listening;			//监听结构体指针
	uint8_t serialNum;				//连接序号，每次分配时加一
	struct sockaddr	s_addr;			//用于保存对方地址信息
	
	Event_handle rHandle;			//读事件处理相关函数
	Event_handle wHandle;			//写事件处理相关函数

	//uint8_t w_ready;				//（unsigned char）写动作准备完成标记
	//uint8_t r_ready;				//读动作准备完毕
	//unsigned instance : 1;		//效用标志位，大小1bit ,0：有效，1：失效
	//*************************


	//********收包有关*********
	unsigned char recvStat;			//当前收包状态
	bool isNewRecv;					//判断包头是否完成接收，以分配空间接收包体
	
	char	headInfo[HEAD_BUFFER];	//用于储存包头的缓冲区
	char*	bufptr;					//初始指向包头缓冲区首元素，用于辅助接收
	unsigned int recvlen;			//用于指定当此接收的数据长度
	char* MsgInfo;					//指向完整的信息包
	//*************************


	//*********发包相关*********
	char* sendptr;					//发送数据头指针
	char* delptr;					//用于发送数据后释放内存的指针
	size_t sendLen;					//要发送数据的长度

	atomic<int> requestSent;		//客户端请求发送的信息数
	atomic<int> throwSendCount;		//是否通过系统发送，> 0 表示通过系统通知发送
	//**************************


	//*********安全相关*********
	time_t lastPingTime;			//最后一次收到心跳包时间
	uint64_t floodCheckLast;		//最后一次泛洪检测时间
	size_t floodCount;				//泛洪嫌疑包数量
	//**************************


	//**********其他***********
	time_t recyTime;				//连接开始回收时间
	pthread_mutex_t logicMutex;		//业务逻辑处理函数互斥锁
	pConnection	next;				//后继指针
};


//监听端口相关信息
struct Listening
{
	int port; //监听端口号
	int fd;	  //套接字句柄
	
	pConnection connection; //连接池中元素指针
};


//消息头
typedef struct MSG_Head
{
	pConnection	conn;			//指针指向当前连接
	uint64_t serialNum;			//连接序号，用于比较连接是否作废
}MSG_Head;


//网络信息处理类
class MSocket
{
public:

	MSocket();
	
	//加virtual可在其子类中实现对目标函数的覆盖
	virtual ~MSocket();

public:
	
	//初始化函数
	virtual bool Initialize();
	
	//epoll初始化
	int epoll_init();

	//工作进程相关量初始化
	bool WrokProcRelevent_Init();

	//epoll事件处理函数
	int epoll_process_events(int time);

	//用于线程接收信号处理
	virtual void ThreadReceive_Proc(char* msgInfo);

	//用于检测心跳包到期时间
	virtual void PingTime_Check(MSG_Head* pmsghdr, time_t current);

	//工作进程相关关闭回收函数
	void  WrokProcRelevent_Off();

	//增加epoll事件
	int Epoll_Add_event(int sock, uint32_t enventTp,
		uint32_t otherflag, int addAction, pConnection cptr);

	//将一个待发送的数据放入消息队列等待发送
	void MsgSend(char* sendBuf);

	//主动关闭一个连接
	void ActiveShutdown(pConnection cptr);

private:

	//辅助线程结构定义
	struct ThreadItem
	{
		pthread_t Handle;		//线程句柄
		MSocket* pThis;			//线程池指针

		bool isRunning;			//标记是否开始运行，只有开始运行才可通过StopAll关闭
		
		//构造函数通过默认参数初始化成员
		ThreadItem(MSocket* pthis) :pThis(pthis), isRunning(false) {}
		
		~ThreadItem() {}
	};

private:
	//建立监听端口
	bool Establish_listenSocket();
	
	//关闭监听端口
	void Close_listenSocket();
	
	//设置监听非阻塞
	bool Set_NoBlock(int sock);
	
	//初始化连接池
	void InitConnections();
	
	//从连接池中获取一个空闲连接对象
	pConnection Get_connection(int sock);
	
	//用户连接事件处理函数
	void Event_accept(pConnection old);
	
	//关闭用户接入连接
	void Close_Connection(pConnection cptr);
	
	//将从连接池内申请的连接释放
	void Free_connection(pConnection cptr);
	
	//链接回收处理线程
	static void* ReConnection_Thread(void* threadData);
	
	//数据发送线程
	static void* MessageSend_Thread(void* threadData);
	
	//心跳包检测线程
	static void* HeartbeatTime_Thread(void* theadData);
	
	//将需要特别回收的链接放入回收队列
	void IntoRecovery(pConnection cptr);

	//清空回收链接池
	void ClearConnections();

	//对数据的读处理函数
	void Read_requestHandle(pConnection cptr);

	//对数据的发送处理函数
	void Write_requestHandle(pConnection cptr);

	//接收数据专用函数，返回接收到的数据大小
	ssize_t ReceiveProc(pConnection cptr, char* buffer, ssize_t buflen);

	//发送数据专用函数，返回接收到的数据大小
	ssize_t SendProc(pConnection cptr, char* buffer, ssize_t buflen);

	//用于包头接收完毕后的处理
	void Request_Handle_A(pConnection cptr, bool& isFlood);
	
	//用于整个包接收完毕后的处理
	void Request_Handle_L(pConnection cptr, bool& isFlood);

	//往心跳包队列中增加连接
	void HeartQueue_ADD(pConnection cptr);

	//在心跳包队列中删除连接
	void HeardQueue_Delete(pConnection cptr);
	
	//清空心跳包队列
	void ClearHeartQueue();

	//清空发送队列
	void ClearSendQueue();

	//找到并返回当前已超时的一个节点
	MSG_Head* Overtime_Get(time_t current);

	//删除头节点,并返回其消息头指针
	MSG_Head* FirstTime_Remove();

	//获取计算器队列头部时间
	time_t EarlyTime_Get();

	//检测连接是否产生了泛洪攻击
	bool TestFlood(pConnection cptr);

private:
	
	int m_listen_port_count;					//监听端口数
	vector<Listening_p> m_listenSocketList;		//套接字队列

	int m_work_connections;						//epoll最大连接数量
	int m_epollHandle;							//epoll_create返回句柄	
	struct epoll_event m_events[MAX_EVENTS];	//用于储存epoll行为

	list<pConnection>	m_connection;			//连接池链表
	list<pConnection> m_reConnection;			//连接回收链表
	list<pConnection> m_fconnection;			//连接池空闲链表
	pthread_mutex_t m_connectMutex;				//连接相关互斥量
	atomic<int> m_connection_all;				//正在使用连接数
	atomic<int> m_connection_free;				//可用空闲连接数
	atomic<int> m_connection_reco;				//回收链接数
	atomic<int> m_connection_work;				//正在工作连接数

	list<char*>	m_sendQueue;					//发送消息队列
	atomic<int> m_sendMsgCount;					//消息发送队列消息数
	sem_t m_semSendMsg;							//发送队列信号量
	pthread_mutex_t m_sendMsgMutex;				//发包相关互斥量
	size_t m_MAX_Send;							//发送消息滞留的最大数量
	size_t m_MAX_rqSend;						//单个连接发送消息滞留的最大数量
	atomic<int> m_discardPkg;					//发送队列丢弃包的数量

	pthread_mutex_t m_recoveryMutex;			//回收相关互斥量
	int  m_reWaitTime;							//回收等待时间

	bool m_floodCheckStart;						//是否开启泛洪检查
	uint32_t m_floodInterval;					//泛洪检测允许间隔时间
	size_t m_floodPackages;						//泛洪检测所规定时间段内允许通过包个数

	vector<ThreadItem*> m_auxthread;			//用于储存辅助进程

public:

	size_t m_PKG_Hlen;							//包头长度
	size_t m_MSG_Hlen;							//消息头长度

	multimap<time_t, MSG_Head*>  m_heartQueue;	//存放连接心跳时间队列
	pthread_mutex_t m_timeMutex;				//心跳包处理互斥量
	size_t m_heartSize;							//心跳队列大小
	time_t m_firstHtime;						//当前计算器队列头部时间值
	bool m_heartbeatStart;						//是否开启心跳包
	int m_heartbeatWait;						//心跳包等待时间
};

#endif // !Sockets

