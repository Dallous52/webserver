#ifndef PROFILE_CTL
#define PROFILE_CTL

#include "Globar.h"

//配置参数名与参数值提取
vector<string> GetConfig_Name_Num(char* txt, vector<char> annotation);

//获取配置参数
string findProVar(string name);

#endif
