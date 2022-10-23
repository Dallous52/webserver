#include "Process.h"

void SetProcName(string name)
{

#ifndef PROCNAME
#define PROCNAME

	int size = 0;
	for (int i = 0; m_argv[i]; i++)
	{
		size += strlen(m_argv[i]) + 1;
	}
	
	//保证更换完名字后之后所有的内存都为0
	if (name.size() >= size)
	{
		strcpy(m_argv[0], name.c_str());
	}
	else
	{
		strcpy(m_argv[0], name.c_str());
		char* set = m_argv[0] + name.size();

		memset(set, 0, size - name.size());
	}
		 
#endif // !1

}


void MoveEnviron()
{
	int size = 0;

	//获取environ的元素个数
	int i;
	for (i = 0; environ[i]; i++);

	char** newenv = new char* [i + 1];
	newenv[i] = NULL;

	//拷贝
	for (int j = 0; environ[j]; j++)
	{
		int lenth = strlen(environ[j]);
		size += lenth + 1;//计算environ占用字节数
		newenv[j] = new char[lenth + 1];
		memcpy(newenv[j], environ[j], lenth);
		newenv[j][lenth] = '\0';
	}

	memset(environ[0], 0, size);//将旧内存全部设置为0

	environ = newenv;
	newenv = NULL;

	return;
}