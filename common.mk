
#利用if语句判断gcc执行模式

ifeq ($(DEBUG),true)
CC = g++ -g -std=c++11
VERSION = debug
else
CC = g++ -std=c++11
VERSION = release
endif


#创建变量SRCS包含当前目录下所有.c文件名
#wildcard表示扫描当前目录下所有的文件
SRCS = $(wildcard *.cpp)

#创建变量OBJS将SRCS中所有的 .c 变成 .o
OBJS = $(SRCS:.cpp=.o)


DEPS = $(SRCS:.cpp=.d)

#指定bin文件位置
#BIN = /mnt/hgfs/linux/nginx
#addprefix 为BIN增加前缀
BIN := $(addprefix $(BUILD_ROOT)/,$(BIN))

#定义obj文件存放的目录，将链接目录的位置统一
LINK_OBJ_DIR = $(BUILD_ROOT)/app/link_obj
DEP_DIR = $(BUILD_ROOT)/app/dep

#创建obj目录和dep目录，-p表示如果已经存在就不创建
$(shell mkdir -p $(LINK_OBJ_DIR))
$(shell mkdir -p $(DEP_DIR))

#:=与=区别
#:=在解析阶段直接赋值常量字符串(像define)
#=在运行阶段使用变量赋值

OBJS := $(addprefix $(LINK_OBJ_DIR)/,$(OBJS))
DEPS := $(addprefix $(DEP_DIR)/,$(DEPS))

#找到目录中所有的.o目录(编译得出的)
LINK_OBJ = $(wildcard $(LINK_OBJ_DIR)/*.o)

#构建依赖关系时app目录下缺少OBJS相应文件
LINK_OBJ += $(OBJS)

#脚本开始运行入口
all:$(DEPS) $(OBJS) $(BIN)

#包含所有.d文件
ifneq ("$(wildcard $(DEPS))","")
include $(DEPS)
endif

#对所有.o文件进行连接输出
$(BIN):$(LINK_OBJ)
	@echo "-------------------------------build $(VERSION) mode--------------------------------------------"

	$(CC) -o $@ $^ -lpthread

#将所有.c文件编译为.o
$(LINK_OBJ_DIR)/%.o:%.cpp
	$(CC) -I$(INCLUDE_PATH) -o $@ -c $(filter %.cpp,$^)

#记录编译信息
$(DEP_DIR)/%.d:%.cpp
	echo -n $(LINK_OBJ_DIR)/ > $@
	$(CC) -I$(INCLUDE_PATH) -MM $^ >> $@


