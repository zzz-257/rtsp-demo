#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#include "fifo.h"

#define CONFIG_COND_FREE 1
#define CONFIG_COND_ACTIVE 1

struct sfifo_s* sfifo_get_free_buf(struct sfifo_des_s *sfifo_des_p)
{

	static long empty_count = 0;
	struct sfifo_s *sfifo = NULL;

	pthread_mutex_lock(&(sfifo_des_p->free_list.mutex));
#ifdef CONFIG_COND_FREE
	while (sfifo_des_p->free_list.head == NULL) {

		pthread_cond_wait(&(sfifo_des_p->free_list.cond), &(sfifo_des_p->free_list.mutex));
	}
#else
	if (sfifo_des_p->free_list.head == NULL) {

		if (empty_count++ % 120 == 0) {

			printf("free list empty\n");
		}
		goto EXIT;
	}
#endif
	sfifo = sfifo_des_p->free_list.head;
	sfifo_des_p->free_list.head = sfifo->next;

EXIT:
	pthread_mutex_unlock(&(sfifo_des_p->free_list.mutex));

	return sfifo;
}

/*入队*/
int sfifo_put_free_buf(struct sfifo_s *sfifo, struct sfifo_des_s *sfifo_des_p)
{

	int send_cond = 0;

	/*加锁*/
	pthread_mutex_lock(&(sfifo_des_p->free_list.mutex));
	if (sfifo_des_p->free_list.head == NULL)
	{
		sfifo_des_p->free_list.head = sfifo;
		sfifo_des_p->free_list.tail = sfifo;
		sfifo_des_p->free_list.tail->next = NULL;
		send_cond = 1;
	}
	else
	{
		sfifo_des_p->free_list.tail->next = sfifo;
		sfifo_des_p->free_list.tail = sfifo;
		sfifo_des_p->free_list.tail->next = NULL;
	}
	/*解锁*/
	pthread_mutex_unlock(&(sfifo_des_p->free_list.mutex));
#ifdef CONFIG_COND_FREE
	if (send_cond) {

		pthread_cond_signal(&(sfifo_des_p->free_list.cond));
	}
#endif
	return 0;
}

struct sfifo_s* sfifo_get_active_buf(struct sfifo_des_s *sfifo_des_p)
{

	struct sfifo_s *sfifo = NULL;

	pthread_mutex_lock(&(sfifo_des_p->active_list.mutex));
#ifdef CONFIG_COND_ACTIVE
	while (sfifo_des_p->active_list.head == NULL) {

		//pthread_cond_timedwait(&(sfifo_des_p->active_list.cond), &(sfifo_des_p->active_list.mutex), &outtime);
		pthread_cond_wait(&(sfifo_des_p->active_list.cond), &(sfifo_des_p->active_list.mutex));
	}
#else
	if (sfifo_des_p->active_list.head == NULL) {

		printf("active list empty\n");
		goto EXIT;
	}
#endif

	sfifo = sfifo_des_p->active_list.head;
	sfifo_des_p->active_list.head = sfifo->next;

EXIT:
	pthread_mutex_unlock(&(sfifo_des_p->active_list.mutex));

	return sfifo;
}

/*出队*/
int sfifo_put_active_buf(struct sfifo_s *sfifo, struct sfifo_des_s *sfifo_des_p)
{

	int send_cond = 0;

	pthread_mutex_lock(&(sfifo_des_p->active_list.mutex));
	if (sfifo_des_p->active_list.head == NULL) {

		sfifo_des_p->active_list.head = sfifo;
		sfifo_des_p->active_list.tail = sfifo;
		sfifo_des_p->active_list.tail->next = NULL;
		send_cond = 1;
	} else {

		sfifo_des_p->active_list.tail->next = sfifo;
		sfifo_des_p->active_list.tail = sfifo;
		sfifo_des_p->active_list.tail->next = NULL;
	}
	pthread_mutex_unlock(&(sfifo_des_p->active_list.mutex));
#ifdef CONFIG_COND_ACTIVE
	if (send_cond) {

		pthread_cond_signal(&(sfifo_des_p->active_list.cond));
	}
#endif
	return 0;
}

struct sfifo_des_s *sfifo_init(int sfifo_num, int sfifo_buffer_size)
{

	int i = 0;
	struct sfifo_s *sfifo;

	struct sfifo_des_s *sfifo_des_p = (struct sfifo_des_s *)malloc(sizeof(struct sfifo_des_s));

	/*存数据队列 头结点&尾节点*/
	sfifo_des_p->free_list.head = NULL;
	sfifo_des_p->free_list.tail = NULL;

	/*初始化互斥锁*/
	pthread_mutex_init(&sfifo_des_p->free_list.mutex, NULL);
	/*初始化条件变量*/
	pthread_cond_init(&sfifo_des_p->free_list.cond, NULL);

	/*取数据队列 头结点&尾节点*/
	sfifo_des_p->active_list.head = NULL;
	sfifo_des_p->active_list.tail = NULL;

	/*初始化互斥锁*/
	pthread_mutex_init(&sfifo_des_p->active_list.mutex, NULL);
	/*初始化条件变量*/
	pthread_cond_init(&sfifo_des_p->active_list.cond, NULL);

	/*初始化 n个 数据*/
	for (i = 0; i < sfifo_num; i++)
	{
		sfifo = (struct sfifo_s *)malloc(sizeof(struct sfifo_s));
		sfifo->buffer = (unsigned char *)malloc(sfifo_buffer_size);

	//	printf("sfifo_init: %x\n", (unsigned int)sfifo->buffer);

		memset(sfifo->buffer, i, sfifo_buffer_size);
		sfifo->size = sfifo_buffer_size;
		sfifo->next = NULL;
		sfifo_put_free_buf(sfifo, sfifo_des_p);
	}

	return sfifo_des_p;
}
