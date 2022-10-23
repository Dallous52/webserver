#ifndef Logic
#define Logic

#include "Globar.h"
#include "Socket.h"
#include "Command.h"

//MSocket的子类:用于业务逻辑（各种消息触发的事件）处理
class LogicSocket : public MSocket
{
public:

	LogicSocket();
	virtual ~LogicSocket();

	//初始化函数
	virtual bool Initialize();

public:
	//*********************各种处理函数**********************
	
	//心跳包发送函数
	bool HandleTest_Ping(pConnection pConn, MSG_Head* pMsghdr,
		char* pkgBodyInfo, unsigned short bodyLen);
	
	//测试用处理函数，用于输出包体信息
	bool HandleTest_Print(pConnection pConn, MSG_Head* pMsghdr,
		char* pkgBodyInfo, unsigned short bodyLen);

	//数据发送测试
	bool HandleTest_testSend(pConnection pConn, MSG_Head* pMsghdr,
		char* pkgBodyInfo, unsigned short bodyLen);

	//********************************************************
public:

	//用于线程接收信号处理
	virtual void ThreadReceive_Proc(char* msgInfo);

	//用于发送心跳包
	void HeartbeatPKG_Send(MSG_Head* pMsghdr);

	//用于检测处理到期心跳包
	virtual void PingTime_Check(MSG_Head* pmsghdr, time_t current);

private:

	//处理函数总数
	unsigned short m_commandQuantity;
};

#endif
