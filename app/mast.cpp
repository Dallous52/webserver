#include "Manage.h"
#include "Error.h"
#include "ProfileCtl.h"
#include "Process.h"
#include "SigHandle.h"

//ָ���û�����ָ��
char* const* m_argv;

//�Ƿ����ػ����̷�ʽ����
bool isIn_daemon;

//�����Ƿ��˳�
bool IsExit = false;

//��������
uint8_t this_process = 0;

//��������¼�������
LogicSocket msocket; 

int main(int argc, char* const* argv)
{
	m_argv = argv;

	//�����ļ�����
	Provar_init();

	//�źŴ������ݳ�ʼ��
	if (Init_signals() == -1)
	{
		Error_insert_File(LOG_URGENT,
			"signal processing function initialization failed.");
		return 1;
	}
	else
	{
		Error_insert_File(LOG_INFO,
			"signal set initialization success.");
	}

	//manage.Show_ProFile_var();

	//�ж��Ƿ����ػ����̷�ʽ��
	if (stoi(findProVar("start_on_daemons")) == 1)
	{
		int is_start = Create_daemon();

		if (is_start == -1)
		{
			Error_insert_File(LOG_FATAL,
				"Deamons create failed.");
			return 1;
		}
		if (is_start == 1)
		{
			Error_insert_File(LOG_NOTICE,
				"parent process exited");

			return 0;
		}

		isIn_daemon = true;
	}

	MoveEnviron();

	Major_Create_WorkProc();

	return 0;
}

