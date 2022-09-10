#!/bin/sh

clear

rm -rfv lib/*
echo "###############################################################################"

cd fifo;make clean;make
if [ $? -ne 0 ];then
	echo "make fifo err"
	exit
fi
cd -
cp -vf fifo/libgfifo.a lib/
echo "###############################################################################"


cd rtsp;make clean;make
if [ $? -ne 0 ];then
	echo "make rtsp err"
	exit
fi
cd -
cp -vf rtsp/librtsp_hisi.a lib/
echo "###############################################################################"



cd v4l2;make clean;make
if [ $? -ne 0 ];then
	echo "make v4l2 err"
	exit
fi
cd -
cp -vf v4l2/libgv4l2.a lib/
echo "###############################################################################"



cd x265;make clean;make
if [ $? -ne 0 ];then
	echo "make x265 err"
	exit
fi
cd -
cp -vf x265/libgx265.a lib/
echo "###############################################################################"


ctags -R
gcc  main_online.c \
	-L lib -lgx265 -lrtsp_hisi -lgfifo -lgv4l2  \
	-L /usr/local/lib -lx265 \
	-I /usr/local/include \
	-I fifo \
	-I rtsp  \
	-lpthread \
	-o online

gcc  main_native.c \
	-L lib -lgx265 -lrtsp_hisi -lgfifo -lgv4l2  \
	-L /usr/local/lib -lx265 \
	-I /usr/local/include \
	-I fifo \
	-I rtsp  \
	-lpthread \
	-o native
