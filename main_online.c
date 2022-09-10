#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>

#include "comm.h"
#include "rtsp_demo.h"

static int flag_run = 1;

static void sig_proc(int signo)
{
	flag_run = 0;
	sleep(3);
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

#include "fifo.h"

pthread_t v4l2;
pthread_t x265;
pthread_t rtsp;

struct sfifo_des_s *gbuf_p2;
struct sfifo_des_s *gbuf_p1;

extern int g_x265(char *yuv, char *sfifo);
extern void yuyv_to_yuv420P(unsigned char *in, unsigned char *out, int width, int height);

//#define dbg_w_bs
void *x265_thread(void *args)
{
	struct sfifo_s *yuv;
	struct sfifo_s *bs;
	static int i=0;

	char *xbuf = (char *)malloc(640*480*3/2);
	assert(xbuf);
	while(flag_run) {

re_enc:
		yuv = sfifo_get_active_buf(gbuf_p1);
		assert(yuv);
		bs = sfifo_get_free_buf(gbuf_p2);
		if (yuv != NULL && bs != NULL) {

			yuyv_to_yuv420P(yuv->buffer, xbuf, 640, 480);

			/*调用编码器*/
			bs->cur_size = g_x265(xbuf, bs->buffer);
			if(bs->cur_size > 640*480*2)
			{
				printf("%s[%d] bs over size, re enc\n",__func__,__LINE__);
				goto re_enc;
			}

#ifdef dbg_w_bs
			FILE *fw = fopen("xxx.h265", "ab+");
			assert(fw);
#endif
#ifdef dbg_w_bs
			fwrite(bs->buffer, 1, bs->cur_size, fw);
			if(i++>40)
			{
				fclose(fw);
			}
#endif
			sfifo_put_free_buf(yuv, gbuf_p1);
			sfifo_put_active_buf(bs, gbuf_p2);

		}
	}
	free(xbuf);
}

extern void v4l2_init(void);
extern void v4l2_exit(void);
extern void v4l2_get_yuv(struct sfifo_des_s *gbuf_p0);
void *v4l2_thread(void *args)
{
	v4l2_init();
	while(flag_run){

		v4l2_get_yuv(gbuf_p1);
	}
	v4l2_exit();
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
void *rtsp_thread(void *args)
{
	rtsp_demo_handle demo;
	rtsp_session_handle session = NULL;
	uint8_t *vbuf = NULL;
	uint8_t *abuf = NULL;
	uint64_t ts = 0;
	int ret;

	demo = rtsp_new_demo(8554);//rtsp sever socket
	if (NULL == demo) {
		printf("rtsp_new_demo failed\n");
		return 0;
	}

	session = rtsp_new_session(demo, "/live");//对应rtsp session 
	if (NULL == session) {
		printf("rtsp_new_session failed\n");
		return 0;
	}

	int codec_id = RTSP_CODEC_ID_VIDEO_H265;
	rtsp_set_video(session, codec_id, NULL, 0);
	rtsp_sync_video_ts(session, rtsp_get_reltime(), rtsp_get_ntptime());

	printf("rtsp://192.168.2.10:8554/live\n");

	while (flag_run) {
		ts = rtsp_get_reltime();
		uint8_t type = 0;

		struct sfifo_s *bs;
		char *vbuf;
read_video_again:
		/*读取nalu*/
		bs = sfifo_get_active_buf(gbuf_p2);
		vbuf = bs->buffer;
		if (vbuf != NULL) {

			/*传送nalu*/
			if (session)//1源session 存存
			{
				rtsp_tx_video(session, vbuf, bs->cur_size, ts);//2rtsp_client_connect存在
			}

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
				{
					printf("%s[%d]\n",__func__,__LINE__);
					goto read_video_again;
				}
			}

			if(codec_id==RTSP_CODEC_ID_VIDEO_H264)
			{
				if (!(type>0 && type<22))
				{
					printf("%s[%d]\n",__func__,__LINE__);
					goto read_video_again;
				}
			}
			sfifo_put_free_buf(bs, gbuf_p2);
		}
		else
		{
					printf("%s[%d] ####################################\n",__func__,__LINE__);
			goto read_video_again;
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
	}

	free(vbuf);
	free(abuf);

	if (session)
		rtsp_del_session(session);

	rtsp_del_demo(demo);
}

int main(int argc, char *argv[])
{

	int sfifo_num = 4;
	int sfifo_buffer_size = 640*480*2;

	signal(SIGINT, sig_proc);

	/*fifo gbuf_p2 init*/
	gbuf_p2 = sfifo_init(sfifo_num, sfifo_buffer_size);
	/*fifo gbuf_p1 init*/
	gbuf_p1 = sfifo_init(sfifo_num, sfifo_buffer_size);

	/*pthread v4l2 init*/
	pthread_create(&v4l2,NULL,v4l2_thread,NULL);
	/*pthread x265 init*/
	pthread_create(&x265,NULL,x265_thread,NULL);
	/*pthread rtsp init*/
	pthread_create(&rtsp,NULL,rtsp_thread,NULL);

	pthread_join(v4l2,NULL);printf("%s[%d] v4l2 exit\n",__func__,__LINE__);
	pthread_join(x265,NULL);printf("%s[%d] x265 exit\n",__func__,__LINE__);
	pthread_join(rtsp,NULL);printf("%s[%d] rtsp exit\n",__func__,__LINE__);

	/*fifo gbuf_p2 exit*/
	free(gbuf_p2);
	/*fifo gbuf_p1 exit*/
	free(gbuf_p1);

	return 0;
}
