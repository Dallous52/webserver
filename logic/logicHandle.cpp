#include "Logic.h"
#include "MLock.h"
#include "Crc32.h"


bool LogicSocket::HandleTest_Ping(pConnection pConn, MSG_Head* pMsghdr,
	char* pkgBodyInfo, unsigned short bodyLen)
{
	if (bodyLen != 0)
	{
		return false;
	}

	Mlock lock(&pConn->logicMutex);

	pConn->lastPingTime = time(NULL);

	HeartbeatPKG_Send(pMsghdr);

	//cerr << "Debug:> Heartbeat package send success." << endl;

	return true;
}


bool LogicSocket::HandleTest_Print(pConnection pConn, MSG_Head* pMsghdr,
	char* pkgBodyInfo, unsigned short bodyLen)
{
	if (pkgBodyInfo == NULL)
	{
		return false;
	}

	int judgeLen = sizeof(mMessage);
	if (bodyLen != judgeLen)
	{
		return false;
	}
	//����Ϊ������ж�

	Mlock lock(&pConn->logicMutex);

	mMessage* body = (mMessage*)pkgBodyInfo;
	body->message[99] = '\0';

	cerr << endl << "/*---------Here is the package information---------*/" << endl;
	cerr << "message :> " << body->message << endl;
	cerr << "num = " << ntohl(body->test) << endl;

	return true;
}


bool LogicSocket::HandleTest_testSend(pConnection pConn, MSG_Head* pMsghdr,
	char* pkgBodyInfo, unsigned short bodyLen)
{
	if (pkgBodyInfo == NULL)
	{
		return false;
	}

	int judgeLen = sizeof(mMessage);
	if (bodyLen != judgeLen)
	{
		return false;
	}
	//����Ϊ������ж�
	
	//������Ϣ����

	Mlock lock(&pConn->logicMutex);
	//�߳�����ʵ�ֻ���

	const char* msg = "This is a test message from server.";

	size_t sendsize = strlen(msg);

	char* sendBuf = new char[m_PKG_Hlen + m_MSG_Hlen + sendsize];

	//������Ϣͷ
	memcpy(sendBuf, pMsghdr, m_MSG_Hlen);

	PKG_Head* pkghdr = (PKG_Head*)(sendBuf + m_MSG_Hlen);

	//����ͷ
	pkghdr->mesgCode = htons(3);
	pkghdr->pkgLen = htons(m_PKG_Hlen + sendsize);

	//������
	char* sendInfo = sendBuf + m_MSG_Hlen + m_PKG_Hlen;
	
	strcpy(sendInfo, msg);

	//����crcֵ
	uint32_t crc = crc32((unsigned char*)sendInfo, sendsize);
	pkghdr->crc32 = htonl(crc);

	//��������
	MsgSend(sendBuf);

	return true;
}