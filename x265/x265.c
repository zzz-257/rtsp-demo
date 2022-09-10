#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "x265.h"

int g_x265(char *yuv, char *sfifo)
{
	static unsigned int cnt=0;
	int i,j;
	int y_size;
	int buff_size;
	char *buff=NULL;
	int ret;
	//TODO
//typedef struct x265_nal
//{
//    uint32_t type;        /* NalUnitType */
//    uint32_t sizeBytes;   /* size in bytes */
//    uint8_t* payload;
//} x265_nal;
	x265_nal *pNals=NULL;
	uint32_t iNal=0;
	unsigned int cur_size=0;

	x265_param* pParam=NULL;
	x265_encoder* pHandle=NULL;
	x265_picture *pPic_in=NULL;

	//Encode 50 frame
	//if set 0, encode all frame
	int frame_num=0;
	int csp=X265_CSP_I420;
	int width=640,height=480;

	pParam=x265_param_alloc();
	x265_param_default(pParam);
	pParam->bRepeatHeaders=1;//write sps,pps before keyframe
	pParam->internalCsp=csp;
	pParam->sourceWidth=width;
	pParam->sourceHeight=height;
	pParam->fpsNum=25;
	pParam->fpsDenom=1;
	{
		/*log close*/
		pParam->logLevel=0;
		/*sei close*/
		pParam->bEmitInfoSEI=0;
		/*gop set*/
		pParam->keyframeMax=8;
		/*b frame close*/
		pParam->bframes=0;
		/*first gop slice IDR set*/
		pParam->bOpenGOP=0;
#if 0
		/*sao*/
		pParam->bEnableSAO=1;
		/*qp*/
		pParam->bOptQpPPS=25;

		/*rc*/
		pParam->rc.qp=25;
		pParam->rc.qpMin=15;
		pParam->rc.qpMax=35;
#endif
	}
	//Init
	pHandle=x265_encoder_open(pParam);

	if(pHandle==NULL)
	{
		printf("x265_encoder_open err\n");
		return 0;
	}
	y_size = pParam->sourceWidth * pParam->sourceHeight;

	pPic_in = x265_picture_alloc();
	x265_picture_init(pParam,pPic_in);
	switch(csp)
	{
		case X265_CSP_I444:{
							   buff=(char *)malloc(y_size*3);
							   pPic_in->planes[0]=buff;
							   pPic_in->planes[1]=buff+y_size;
							   pPic_in->planes[2]=buff+y_size*2;
							   pPic_in->stride[0]=width;
							   pPic_in->stride[1]=width;
							   pPic_in->stride[2]=width;
							   break;
						   }
		case X265_CSP_I420:{
							   buff=(char *)malloc(y_size*3/2);
							   pPic_in->planes[0]=buff;
							   pPic_in->planes[1]=buff+y_size;
							   pPic_in->planes[2]=buff+y_size*5/4;
							   pPic_in->stride[0]=width;
							   pPic_in->stride[1]=width/2;
							   pPic_in->stride[2]=width/2;
							   break;
						   }
		default:{
					printf("Colorspace Not Support.\n");
					return -1;
				}
	}

	//detect frame number
	if(frame_num==0)
	{
		switch(csp)
		{
			case X265_CSP_I444:frame_num=strlen(yuv)/(y_size*3);break;
			case X265_CSP_I420:frame_num=strlen(yuv)/(y_size*3/2);break;
			default:printf("Colorspace Not Support.\n");return -1;
		}
	}

	//Loop to Encode
	for( i=0;i<frame_num;i++)
	{
		switch(csp)
		{
			case X265_CSP_I444:
				{
				//	memcpy(pPic_in->planes[0],&yuv[y_size*3/2*i+0],y_size);		//Y
				//	memcpy(pPic_in->planes[1],&yuv[y_size*3/2*i+y_size],y_size);	//U
				//	memcpy(pPic_in->planes[2],&yuv[y_size*3/2*i+y_size+y_size],y_size);	//V
					break;
				}
			case X265_CSP_I420:
				{
				//	memcpy(pPic_in->planes[0],&yuv[y_size*3/2*i+0],y_size);		//Y
				//	memcpy(pPic_in->planes[1],&yuv[y_size*3/2*i+y_size],y_size/4);	//U
				//	memcpy(pPic_in->planes[2],&yuv[y_size*3/2*i+y_size+y_size/4],y_size/4);	//V
					pPic_in->planes[0] = &yuv[y_size*3/2*i+0];		//Y
					pPic_in->planes[1] = &yuv[y_size*3/2*i+y_size];	//U
					pPic_in->planes[2] = &yuv[y_size*3/2*i+y_size+y_size/4];	//V
					break;
				}
			default:
				{
					printf("Colorspace Not Support.\n");
					return -1;
				}
		}

		ret=x265_encoder_encode(pHandle,&pNals,&iNal,pPic_in,NULL);	

		for(j=0;j<iNal;j++)
		{
			memcpy(&sfifo[cur_size], pNals[j].payload, pNals[j].sizeBytes);
			cur_size+=pNals[j].sizeBytes;
		}	
	}
	//Flush Decoder
	while(1)
	{
		ret=x265_encoder_encode(pHandle,&pNals,&iNal,NULL,NULL);
		if(ret==0){
			break;
		}
		//	printf("Flush 1 frame.\n");

		for(j=0;j<iNal;j++)
		{
			memcpy(&sfifo[cur_size], pNals[j].payload, pNals[j].sizeBytes);
			cur_size+=pNals[j].sizeBytes;
		}
	}

	x265_encoder_close(pHandle);
	x265_picture_free(pPic_in);
	x265_param_free(pParam);
	free(buff);

	printf("[%s] [%d] ########Succeed encode frame : %d cur_size : 0x%x ######## \n", __func__,__LINE__, cnt++, cur_size);


	return cur_size;
}
