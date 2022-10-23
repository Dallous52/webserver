#include "Manage.h"
#include "Error.h"
#include "ProfileCtl.h"

//存储配置文件参数
vector<Profile_Var> GloVar;

//配置文件注释字符集
vector<char> note = { '#','[','\n' };

void Provar_init()
{
	ifstream file;

	file.open(File_Profile_Ctl, ios::in);

	if (!file.is_open())
	{
		string err = "open profile failed:> ";
		err += strerror(errno);
		Error_insert_File(LOG_FATAL, err);
		exit(0);
	}

	//配置文件变量初始化
	char* test = new char[401];
	while (file.getline(test, 400))
	{
		vector<string> ansed = GetConfig_Name_Num(test, note);

		if (ansed.size())
		{
			Profile_Var aux;
			aux.name = ansed[0];
			aux.var = ansed[1];

			GloVar.push_back(aux);
		}
	}
}


void Show_ProFile_var()
{
	for (int i = 0; i < GloVar.size(); i++)
	{
		cout << GloVar[i].name << " = " << GloVar[i].var << endl;
	}
}