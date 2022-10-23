#ifndef Command
#define Command

#pragma pack(1)

//客户端发送消息结构体定义
typedef struct mMessage
{
	char message[100];
	int test;

}mMessage;

#pragma pack()

#endif // !Command

