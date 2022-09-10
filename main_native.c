#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "comm.h"
#include "rtsp_demo.h"

#include <signal.h>

static int flag_run = 1;

static void sig_proc(int signo)
{
	flag_run = 0;
}

static int get_next_video_frame (FILE *fp, uint8_t **buff, int *size)
{
	uint8_t szbuf[1024];
	int szlen = 0;
	int ret;
	if (!(*buff)) {
		*buff = (uint8_t*)malloc(2*1024*1024);
		if (!(*buff))
			return -1;
	}

	*size = 0;

	while ((ret = fread(szbuf + szlen, 1, sizeof(szbuf) - szlen, fp)) > 0) {
		int i = 3;
		szlen += ret;
		while (i < szlen - 3 && !(szbuf[i] == 0 &&  szbuf[i+1] == 0 && (szbuf[i+2] == 1 || (szbuf[i+2] == 0 && szbuf[i+3] == 1)))) i++;
		memcpy(*buff + *size, szbuf, i);
		*size += i;
		memmove(szbuf, szbuf + i, szlen - i);
		szlen -= i;
		if (szlen > 3) {
			//printf("szlen %d\n", szlen);
			fseek(fp, -szlen, SEEK_CUR);
			break;
		}
	}
	if (ret > 0)
		return *size;
	return 0;
}

static int get_next_audio_frame (FILE *fp, uint8_t **buff, int *size)
{
	int ret;
#define AUDIO_FRAME_SIZE 320
	if (!(*buff)) {
		*buff = (uint8_t*)malloc(AUDIO_FRAME_SIZE);
		if (!(*buff))
			return -1;
	}

	ret = fread(*buff, 1, AUDIO_FRAME_SIZE, fp);
	if (ret > 0) {
		*size = ret;
		return ret;
	}
	return 0;
}

/*
   rtsp_demo usage:
   rtsp_new_demo  
   rtsp_new_session 新建几个?
   rtsp_set_video 
   rtsp_set_audio 
   while(1){
   rtsp_tx_video
   rtsp_tx_audio
   rtsp_do_event
   }
   rtsp_del_session
   rtsp_del_demo
*/
int main(int argc, char *argv[])
{
	FILE *fp = NULL;
	rtsp_demo_handle demo;
	rtsp_session_handle session = NULL;
	uint8_t *vbuf = NULL;
	uint8_t *abuf = NULL;
	uint64_t ts = 0;
	int vsize = 0, asize = 0;
	int ret;

	demo = rtsp_new_demo(8554);//rtsp sever socket
	if (NULL == demo) {
		printf("rtsp_new_demo failed\n");
		return 0;
	}

	fp = fopen("./test.h265", "rb");//打开视频文件
	if (!fp) {
		fprintf(stderr, "open video failed\n");
		return 0;
	}

	session = rtsp_new_session(demo, "/live");//对应rtsp session 
	if (NULL == session) {
		printf("rtsp_new_session failed\n");
		return 0;
	}

	int codec_id = RTSP_CODEC_ID_VIDEO_H265;
	if (fp) {//当前请求路径存视频数据源
		rtsp_set_video(session, codec_id, NULL, 0);
		rtsp_sync_video_ts(session, rtsp_get_reltime(), rtsp_get_ntptime());
	}

	printf("==========> rtsp://192.168.2.12:8554/live <===========\n");

	ts = rtsp_get_reltime();
	signal(SIGINT, sig_proc);

	while (flag_run) {
		uint8_t type = 0;

		if (fp) {
		read_video_again:
			ret = get_next_video_frame(fp, &vbuf, &vsize);
			if (ret < 0) {
				fprintf(stderr, "get_next_video_frame failed\n");
				flag_run = 0;
				break;
			}
			if (ret == 0) {
				fseek(fp, 0, SEEK_SET);
				goto read_video_again;
			}

			if (session)//1源session 存存
				rtsp_tx_video(session, vbuf, vsize, ts);//2rtsp_client_connect存在

			type = 0;
			if (vbuf[0] == 0 && vbuf[1] == 0 && vbuf[2] == 1) {

				if(codec_id==RTSP_CODEC_ID_VIDEO_H264)
					type = vbuf[3] & 0x1f;
				else if(codec_id==RTSP_CODEC_ID_VIDEO_H265)
					type = vbuf[3]>>1 & 0x3f;
			}

			if (vbuf[0] == 0 && vbuf[1] == 0 && vbuf[2] == 0 && vbuf[3] == 1) {

				if(codec_id==RTSP_CODEC_ID_VIDEO_H264)
					type = vbuf[4] & 0x1f;
				else if(codec_id==RTSP_CODEC_ID_VIDEO_H265)
					type = vbuf[4]>>1 & 0x3f;
			}

			if(codec_id==RTSP_CODEC_ID_VIDEO_H264)
			{
				if (type != 5 && type != 1)
					goto read_video_again;
			}

			if(codec_id==RTSP_CODEC_ID_VIDEO_H264)
			{
				if (!(type>0 && type<22))
					goto read_video_again;
			}
		}

		do {
			ret = rtsp_do_event(demo);//
			if (ret > 0)
				continue;
			if (ret < 0)
				break;
			usleep(20000);
		} while (rtsp_get_reltime() - ts < 1000000 / 25);
		if (ret < 0)
			break;
		ts += 1000000 / 25;
		//	printf(".");fflush(stdout);//立即将printf的数据输出显示
	}

	free(vbuf);
	free(abuf);

	if (fp)
		fclose(fp);
	if (session)
		rtsp_del_session(session);

	rtsp_del_demo(demo);
	printf("Exit.\n");
//	getchar();
	return 0;
}
