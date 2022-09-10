PRO_NAME = libgx265.a
PRO_ROOT = $(shell pwd)
CROSS_COMPILE = #arm-linux-
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar
INC := -I/usr/local/include/
#INC += -I../example
CFLAGS += $(INC)
LIB = -L/usr/local/lib -lx265
SRC_DIR := $(PRO_ROOT)
LIB_SRC = \
	$(SRC_DIR)/x265.c

obj = $(LIB_SRC:%.c=%.o)


all:$(obj)
	$(AR)  -rcs  $(PRO_NAME) $(obj)
clean:
	@rm $(obj)  $(PRO_NAME) -rf
