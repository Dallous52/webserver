#include "Logic.h"
#include "Crc32.h"
#include "MLock.h"
#include "Error.h"


//�����Ա����ָ�루��������
typedef bool (LogicSocket::* Handler)(pConnection pConn, MSG_Head* pMsghdr,
	char* pkgBodyInfo, unsigned short bodyLen);

//������ָ����ֵ
static const Handler ProcFunc[] = {
	&LogicSocket::HandleTest_Ping,
	&LogicSocket::HandleTest_Print,
	&LogicSocket::HandleTest_testSend
};


LogicSocket::LogicSocket()
{
	m_commandQuantity = (unsigned short)(sizeof(ProcFunc) / sizeof(Handler));
}


//�����ʼ���������ø����ʼ������
bool LogicSocket::Initialize()
{
	return MSocket::Initialize();
}


void LogicSocket::ThreadReceive_Proc(char* msgInfo)
{
	MSG_Head* pMsghdr = (MSG_Head*)msgInfo;
	//ָ����Ϣͷָ��

	PKG_Head* pPkghdr = (PKG_Head*)(msgInfo + m_MSG_Hlen);
	//ָ���ͷָ��

	void* pkgBodyInfo =	NULL;
	//ָ�����ָ��

	unsigned short pkglen = ntohs(pPkghdr->pkgLen);
	uint32_t pkgCrc32 = ntohl(pPkghdr->crc32);

	//int* test = (int*)(msgInfo + m_MSG_Hlen + m_PKG_Hlen);
	//cerr << "test:>" << (unsigned int)*test << endl;

	if (m_PKG_Hlen == pkglen)
	{
		//ֻ�а�ͷû�а���
		if (pkgCrc32 != 0)
		{
			//�ް���İ�У����Ϊ0
			//У�������ֱ�Ӷ���
			return;
		}
	}
	else
	{
		pkgBodyInfo = (void*)(msgInfo + m_MSG_Hlen + m_PKG_Hlen);

		int mcrc32 = crc32((unsigned char*)pkgBodyInfo, pkglen - m_PKG_Hlen);
		//����crcУ��ֵ

		if (pkgCrc32 != mcrc32)
		{
			//cerr << "The crc32 check code is differnet, package discard." << endl;
			return;
		}
	}

	unsigned short msgCode = ntohs(pPkghdr->mesgCode);
	//��ȡ������ָ����

	//cerr << "Debug: msgCode:> " << msgCode << endl;

	//uint32_t age = ntohl(test->age);
	//cerr << "Debug: age:> " << age << endl;

	pConnection pConn = pMsghdr->conn;

	//�ж������Ƿ����
	if (pConn->serialNum != pMsghdr->serialNum)
	{
		//�ͻ����쳣�Ͽ����������ð�
		return;
	}

	//�ж��û����͵���Ϣ���Ƿ���ȷ
	if (msgCode >= m_commandQuantity)
	{
		//cerr << "Message code is out of bounds, package discard." << endl;
		return;
	}

	//ִ����Ϣ���Ӧ�Ĵ�����
	(this->*ProcFunc[msgCode])(pConn, pMsghdr, (char*)pkgBodyInfo, pkglen - m_PKG_Hlen);

	return;
}


void LogicSocket::HeartbeatPKG_Send(MSG_Head* pMsghdr)
{
	char* buffer = new char[m_MSG_Hlen + m_PKG_Hlen];
	char* auxbuf = buffer;

	//������Ϣͷ
	memcpy(auxbuf, pMsghdr, m_MSG_Hlen);
	auxbuf = auxbuf + m_MSG_Hlen;

	//�����ͷ
	PKG_Head* pkghdr = (PKG_Head*)auxbuf;
	pkghdr->mesgCode = htons(0);
	pkghdr->pkgLen = htons(m_PKG_Hlen);
	pkghdr->crc32 = 0;

	//������Ϣ
	MsgSend(buffer);

	return;
}


void LogicSocket::PingTime_Check(MSG_Head* pmsghdr, time_t current)
{
	if (pmsghdr->serialNum == pmsghdr->conn->serialNum)
	{
		pConnection auxptr = pmsghdr->conn;

		//��������ʱʱ����� �����ӶϿ�
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