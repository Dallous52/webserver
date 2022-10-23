#include "ProfileCtl.h"


vector<string> GetConfig_Name_Num(char* txt, vector<char> annotation)
{
	vector<string> ans;

	if (txt == NULL || !strlen(txt))
	{
		return ans;
	}

	string txts = txt;

	//删除行间空格
	for (int i = 0; i < txts.size(); i++)
	{
		if (txts[i] == ' ')
		{
			txts.erase(i--, 1);
		}
	}

	//注释等无用行处理
	for (int i = 0; i < annotation.size(); i++)
	{
		if (txts[0] == annotation[i])
		{
			return ans;
		}
	}

	ans.resize(2);

	int mid = txts.find('=');
	
	ans[0].append(txts, 0, mid);
	ans[1].append(txts, mid + 1, txts.size() - 1);

	return ans;
}


string findProVar(string name)
{
	for (int i = 0; i < GloVar.size(); i++)
	{
		if (name == GloVar[i].name)
		{
			return GloVar[i].var;
		}
	}

	cout << "no such variable named " << name << " in profile." << endl;
	return "";
}