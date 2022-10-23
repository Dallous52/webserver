#定义项目编译根目录，通过exprot把某个变量声明为全局的
export BUILD_ROOT = $(shell pwd)

#定义头文件的路径变量
export INCLUDE_PATH = $(BUILD_ROOT)/include

#定义要编译的目录
BUILD_DIR = $(BUILD_ROOT)/proc/\
	    $(BUILD_ROOT)/signal/\
            $(BUILD_ROOT)/net/\
            $(BUILD_ROOT)/logic/\
	    $(BUILD_ROOT)/app/ 


#编译时是否产生调试信息。GUN调试器可以利用该信息
export DEBUG = true;
