#include "Error.h"
#include "ProfileCtl.h"

void Error_insert_File(int errLev, const string& error)
{
	int fail_lev = stoi(findProVar("out_error_level"));
	//�������ļ���ȡ�������ȼ�

	string errfile = findProVar("log_addr");
	//�������ļ��л�ȡ��־�ļ���

	int log = open(errfile.c_str(), O_WRONLY | O_APPEND);

	if (log == -1)//��־��ʧ�ܴ���
	{
		log = open(errfile.c_str(), O_WRONLY | O_CREAT, 0600);

		if (log == -1)
		{
			cerr << "log file open failed : " << strerror(errno) << endl;
			return;
		}
	}

	string currTime = GetTime();

	for (int i = 0; i < ERR_NUM; i++)
	{
		if (errLev == i)//������Ϣ������־�ļ�
		{
			if (i < fail_lev)//�ж��Ƿ��������Ļ
			{
				cerr << currTime << " [" << ErrorInfo[i] << "] " << error << endl;
			}

			string err = currTime + " [" + ErrorInfo[i] + "] " + error + "\n";
			//********�ǵü�ʱ��
			write(log, err.c_str(), err.size());
		}
	}
}


string GetTime()
{
	string date;
	time_t times;
	struct tm* timed;
	char ansTime[50];

	time(&times); //��ȡ��1900������˶����룬����time_t���͵�timep
	timed = localtime(&times);//��localtime������ת��Ϊstruct tm�ṹ��

	sprintf(ansTime, "%d-%02d-%02d %02d:%02d:%02d", 1900 + timed->tm_year, 1 + timed->tm_mon,
		timed->tm_mday, timed->tm_hour, timed->tm_min, timed->tm_sec);

	date = ansTime;

	return date;
}