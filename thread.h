#ifndef _THREAD_HEAD_
#define _THREAD_HEAD_

#include "conn.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define THREAD_NUM 4
#define W_THREAD_NUM 4

/*连接队列 start*/
typedef struct conn_queue_item CQ_ITEM;
struct conn_queue_item{
	int            client_fd;
	int            event_flags;
	enum conn_stat stat;
	conn*          conn; //write thread use
	CQ_ITEM*       next;
};

typedef struct conn_queue CQ;
struct conn_queue{
	CQ_ITEM*        head;
	CQ_ITEM*        tail;
	int             size;
	pthread_mutex_t lock;
};
/*连接队列 end*/

enum thread_role{
	T_R = 1,
	T_W = 2,
	T_MSG = 3, 
};

typedef struct{
	pthread_t          thread_id;        //
	struct event_base* base;             //
	struct event       notify_event;     //
	int                id;               //useless
	char*              name;             //useless
	enum thread_role   type;
	int                notify_recv;      //读管道fd
	int                notify_send;      //写管道fd

	CQ*                conn_queue;       //连接队列指针
	CQ*                write_conn_queue; //写队列指针
}THREAD;



/**
 * ==========[ function prototype ]==========
 */
void thread_init(void);
void write_thread_init(void);
static void create_worker(void*(*func)(void*),void* arg);
static void* worker_libevent(void* arg);
static void* worker_msg_dispatcher(void* arg);
static void setup_thread(THREAD* thread);
static void setup_msg_thread(THREAD* msg_thread);
static void thread_event_handler(int fd, short which, void *arg);
void thread_dispatch_new_conn(int sfd,enum conn_stat init_stat,int event_flags);
static void conn_queue_push(CQ* cq,CQ_ITEM* cq_item);
static CQ_ITEM* conn_queue_pop(CQ* cq);
void* msg_monitor_handler(conn* c);


#endif
