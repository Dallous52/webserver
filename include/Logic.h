#ifndef Logic
#define Logic

#include "Globar.h"
#include "Socket.h"
#include "Command.h"

//MSocket������:����ҵ���߼���������Ϣ�������¼�������
class LogicSocket : public MSocket
{
public:

	LogicSocket();
	virtual ~LogicSocket();

	//��ʼ������
	virtual bool Initialize();

public:
	//*********************���ִ�����**********************
	
	//���������ͺ���
	bool HandleTest_Ping(pConnection pConn, MSG_Head* pMsghdr,
		char* pkgBodyInfo, unsigned short bodyLen);
	
	//�����ô��������������������Ϣ
	bool HandleTest_Print(pConnection pConn, MSG_Head* pMsghdr,
		char* pkgBodyInfo, unsigned short bodyLen);

	//���ݷ��Ͳ���
	bool HandleTest_testSend(pConnection pConn, MSG_Head* pMsghdr,
		char* pkgBodyInfo, unsigned short bodyLen);

	//********************************************************
public:

	//�����߳̽����źŴ���
	virtual void ThreadReceive_Proc(char* msgInfo);

	//���ڷ���������
	void HeartbeatPKG_Send(MSG_Head* pMsghdr);

	//���ڼ�⴦����������
	virtual void PingTime_Check(MSG_Head* pmsghdr, time_t current);

private:

	//����������
	unsigned short m_commandQuantity;
};

#endif
