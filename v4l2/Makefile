LIB_NAME = gv4l2
STATIC_NAME = lib$(LIB_NAME).a
SHARE_NAME = lib$(LIB_NAME).so

CROSS_COMPILE = #arm-linux-
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

all:$(STATIC_NAME) $(SHARE_NAME)

SOURCE := $(wildcard *.c src/*.c)

OBJ := $(patsubst %.c, %.o, $(SOURCE))

#链接其他依赖库
LIBS := #-lx265

#依赖库库路径
LIB_PATH := #-L./lib

#头文件路径
INCLUDES := -I./include -I../fifo

CFLAGS := -Wall -c -lpthread -std=gnu99 -lm -lstdc++ $(LIB_PATH) $(LIBS)

%.o: %.c
	@$(CC) $(INCLUDES) $(CFLAGS) $< -o $@ -DDEBUG

#编译为静态库
$(STATIC_NAME): $(OBJ)
	@$(AR) -crv $(STATIC_NAME) $(OBJ)

#编译为动态库
$(SHARE_NAME): $(OBJ)
	@$(CC) -shared -fPIC -o $(SHARE_NAME) $(SOURCE) $(INCLUDES) -DDEBUG

.PHONY: clean

clean:  
	@rm -rf $(OBJ) *.a *.so
