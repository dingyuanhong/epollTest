CC = gcc
CXX = g++
AR = ar
RM = rm -rf

#原始目录
SRC_PATH :=.
MODULES :=
#目标名
TARGET :=
#源文件
MODULES += $(wildcard $(SRC_PATH)/util/*.cpp)
MODULES += $(wildcard $(SRC_PATH)/module/*.cpp)
SRCS += $(MODULES)
SRCS += $(wildcard $(SRC_PATH)/*.cpp)
#中间文件
OBJS := $(SRCS:.cpp=.o)

MODULE_OBJS := $(MODULES:.cpp=.o)
SEND_OBJS=$(MODULE_OBJS) config.o connect_core.o process_core.o epoll_core.o Sender.o
SEND_TARGET=sender

RECIEVE_OBJS=$(MODULE_OBJS) config.o connect_core.o process_core.o epoll_core.o Recieve.o
RECIEVE_TARGET=recieve

#动态库
LIBS := stdc++ pthread

#模块库文件
MOULE_LIBRARY_PATH = /usr/lib/ /usr/local/lib/

#头文件路径
INCLUDE_PATH :=.
#动态库路径
LIBRARY_PATH :=

INCLUDE_PATH += /usr/include
LIBRARY_PATH += $(MOULE_LIBRARY_PATH)


RELEASE = 1
BITS =

#ifeq ( 1 , ${DBG_ENABLE} )
#	CFLAGS += -D_DEBUG -O0 -g -DDEBUG=1
#endif

CFLAGS = -Wall -std=c++11
LFLAGS =

#头文件
CFLAGS += $(foreach dir,$(INCLUDE_PATH),-I$(dir))

#库路径
LDFLAGS += $(foreach lib,$(LIBRARY_PATH),-L$(lib))

#库名
LDFLAGS += $(foreach lib,$(LIBS),-l$(lib))

#检查版本
ifeq ($(RELEASE),0)
	#debug
	CFLAGS += -g
else
	#release
	CFLAGS += -O3 -DNDEBUG
endif

#检查位宽
ifeq ($(BITS),32)
	CFLAGS += -m32
	LFLAGS += -m32
else
	ifeq ($(BITS),64)
		CFLAGS += -m64
		LFLAGS += -m64
	else
	endif
endif

$(warning $(OBJS))

#操作命令
all:clean source build

$(OBJS):%.o:%.cpp
	$(CXX) $(CFLAGS) -c $^ -o $@

source:$(OBJS)

build_send:$(SEND_OBJS)
	$(CXX) $(LFLAGS) -o $(SEND_TARGET) $(SEND_OBJS) $(LDFLAGS)

build_recieve:$(RECIEVE_OBJS)
	$(CXX) $(LFLAGS) -o $(RECIEVE_TARGET) $(RECIEVE_OBJS) $(LDFLAGS)

build:build_recieve build_send
	$(RM) $(OBJS)

clean:
	echo $(SRCS)
	$(RM) $(OBJS)
