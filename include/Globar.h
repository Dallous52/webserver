#define _CRT_SECURE_NO_WARNINGS

#ifndef Globar
#define Globar

#include <fstream>
#include <cstring>
#include <errno.h>
#include <string>
#include <vector>
#include <atomic>
#include <list>
#include <map>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/sem.h>
#include <pthread.h>
#include <stdarg.h>
using namespace std;

#define File_Profile_Ctl "misc/profile"

#define PROCESS_MASTER 0

#define PROCESS_CHILD  1

extern vector<char> note;	

//配置文件参数结构体
struct Profile_Var
{
	string name;
	string var;
};

extern vector<Profile_Var> GloVar;

extern char* const* m_argv;

extern pid_t this_pid;

extern pid_t this_ppid;

extern bool isIn_daemon;

extern uint8_t this_process;

extern bool IsExit;

extern sig_atomic_t this_reap;

#endif // !Globar

