#ifndef __FIFO_H__
#define __FIFO_H__

/*队列结构*/
struct sfifo_list_des_s {

	struct sfifo_s *head;
	struct sfifo_s *tail;

	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

struct sfifo_des_s {

	struct sfifo_list_des_s free_list;
	struct sfifo_list_des_s active_list;
};

struct sfifo_s {

	unsigned char *buffer;
	unsigned int size;
	unsigned int cur_size;
	struct sfifo_s *next;
};

struct sfifo_des_s *sfifo_init(int sfifo_num, int sfifo_buffer_size);

/* productor */
struct sfifo_s* sfifo_get_free_buf(struct sfifo_des_s *sfifo_des_p);
int sfifo_put_free_buf(struct sfifo_s *sfifo, struct sfifo_des_s *sfifo_des_p);

/* consumer */
struct sfifo_s* sfifo_get_active_buf(struct sfifo_des_s *sfifo_des_p);
int sfifo_put_active_buf(struct sfifo_s *sfifo, struct sfifo_des_s *sfifo_des_p);

#define dbg_v4l2

#endif
