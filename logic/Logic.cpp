#include "Logic.h"
#include "Crc32.h"
#include "MLock.h"
#include "Error.h"


//定义成员函数指针（处理函数）
typedef bool (LogicSocket::* Handler)(pConnection pConn, MSG_Head* pMsghdr,
	char* pkgBodyInfo, unsigned short bodyLen);

//处理函数指针数值
static const Handler ProcFunc[] = {
	&LogicSocket::HandleTest_Ping,
	&LogicSocket::HandleTest_Print,
	&LogicSocket::HandleTest_testSend
};


LogicSocket::LogicSocket()
{
	m_commandQuantity = (unsigned short)(sizeof(ProcFunc) / sizeof(Handler));
}


//子类初始化函数调用父类初始化函数
bool LogicSocket::Initialize()
{
	return MSocket::Initialize();
}


void LogicSocket::ThreadReceive_Proc(char* msgInfo)
{
	MSG_Head* pMsghdr = (MSG_Head*)msgInfo;
	//指向消息头指针

	PKG_Head* pPkghdr = (PKG_Head*)(msgInfo + m_MSG_Hlen);
	//指向包头指针

	void* pkgBodyInfo =	NULL;
	//指向包体指针

	unsigned short pkglen = ntohs(pPkghdr->pkgLen);
	uint32_t pkgCrc32 = ntohl(pPkghdr->crc32);

	//int* test = (int*)(msgInfo + m_MSG_Hlen + m_PKG_Hlen);
	//cerr << "test:>" << (unsigned int)*test << endl;

	if (m_PKG_Hlen == pkglen)
	{
		//只有包头没有包体
		if (pkgCrc32 != 0)
		{
			//无包体的包校验码为0
			//校验码错误直接丢弃
			return;
		}
	}
	else
	{
		pkgBodyInfo = (void*)(msgInfo + m_MSG_Hlen + m_PKG_Hlen);

		int mcrc32 = crc32((unsigned char*)pkgBodyInfo, pkglen - m_PKG_Hlen);
		//计算crc校验值

		if (pkgCrc32 != mcrc32)
		{
			//cerr << "The crc32 check code is differnet, package discard." << endl;
			return;
		}
	}

	unsigned short msgCode = ntohs(pPkghdr->mesgCode);
	//获取处理函数指令码

	//cerr << "Debug: msgCode:> " << msgCode << endl;

	//uint32_t age = ntohl(test->age);
	//cerr << "Debug: age:> " << age << endl;

	pConnection pConn = pMsghdr->conn;

	//判断连接是否过期
	if (pConn->serialNum != pMsghdr->serialNum)
	{
		//客户端异常断开，丢弃无用包
		return;
	}

	//判断用户发送的消息码是否正确
	if (msgCode >= m_commandQuantity)
	{
		//cerr << "Message code is out of bounds, package discard." << endl;
		return;
	}

	//执行消息码对应的处理函数
	(this->*ProcFunc[msgCode])(pConn, pMsghdr, (char*)pkgBodyInfo, pkglen - m_PKG_Hlen);

	return;
}


void LogicSocket::HeartbeatPKG_Send(MSG_Head* pMsghdr)
{
	char* buffer = new char[m_MSG_Hlen + m_PKG_Hlen];
	char* auxbuf = buffer;

	//填入消息头
	memcpy(auxbuf, pMsghdr, m_MSG_Hlen);
	auxbuf = auxbuf + m_MSG_Hlen;

	//填入包头
	PKG_Head* pkghdr = (PKG_Head*)auxbuf;
	pkghdr->mesgCode = htons(0);
	pkghdr->pkgLen = htons(m_PKG_Hlen);
	pkghdr->crc32 = 0;

	//返送消息
	MsgSend(buffer);

	return;
}


void LogicSocket::PingTime_Check(MSG_Head* pmsghdr, time_t current)
{
	if (pmsghdr->serialNum == pmsghdr->conn->serialNum)
	{
		pConnection auxptr = pmsghdr->conn;

		//心跳包超时时间过长 将连接断开
		if ((current - auxptr->lastPingTime) > (m_heartbeatWait * 3))
		{
			Error_insert_File(LOG_NOTICE, "Connection overtime, disconnect.");
		
			ActiveShutdown(auxptr);
		}

		delete pmsghdr;
	}
	else
	{
		delete pmsghdr;
	}

	return;
}


LogicSocket::~LogicSocket()
{

}