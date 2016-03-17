#include "thread.h"
#include "conn.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
//#include <event2/event.h>
#include <event.h>
#include <signal.h>

THREAD* threads;        //read worder
THREAD* write_threads;  //write worker
THREAD* msg_thread;     //monitor message worker

/*
 + ========================== +
 | 线程初始化
 | 主线程Accept
 | 4个worker线程 read
 | 1个线程监控消息
 | 4个worker线程 write
 + ========================== +
 */
void thread_init(void)
{/*{{{*/
	int i;
	threads       = calloc(THREAD_NUM,sizeof(THREAD));
	write_threads = calloc(W_THREAD_NUM,sizeof(THREAD));
	//msg_thread    = (THREAD*)malloc(sizeof(THREAD));

	char* name[] = {"r_a","r_b","r_c","r_d","r_e"};
	char* w_name[] = {"w_a","w_b","w_c","w_d","w_e"};
	
	for(i = 0;i < THREAD_NUM;++i){
		int fds[2];
		//create pipe
		if(pipe(fds)){
			printf("pipe create error!\n");
			exit(1);
		}

		threads[i].id = i + 1;
		threads[i].name = name[i];
		threads[i].type = T_R;

		threads[i].notify_recv = fds[0];
		threads[i].notify_send = fds[1];
		threads[i].conn_queue  = NULL;

		setup_thread(&threads[i]);
	}

	/*
	for(i = 0;i < W_THREAD_NUM;++i){
		int fds[2];
		//create pipe
		if(pipe(fds)){
			printf("pipe create error!\n");
			exit(1);
		}

		write_threads[i].id = i + 1;
		write_threads[i].name = w_name[i];
		write_threads[i].type = T_W;

		write_threads[i].notify_recv = fds[0];
		write_threads[i].notify_send = fds[1];
		write_threads[i].conn_queue  = NULL;

		setup_thread(&write_threads[i]);
	}
	*/

	//setup_msg_thread(msg_thread);
	
	//create thread
	for(i = 0;i < THREAD_NUM;++i){
		create_worker(worker_libevent,&threads[i]);
	}

	/*
	for(i = 0;i < W_THREAD_NUM;++i){
		create_worker(worker_libevent,&write_threads[i]);
	}
	*/
	//create_worker(worker_msg_dispatcher,msg_thread);
}/*}}}*/

static void setup_msg_thread(THREAD* msg_thread)
{/*{{{*/
	msg_thread->id = 1;
	msg_thread->name = "msg monitor";
	msg_thread->type = T_MSG;
	msg_thread->conn_queue  = NULL;

	//注册线程连接队列
	CONN_LIST = (conn_list_queue*)malloc(sizeof(conn_list_queue));
	if(NULL == CONN_LIST){
		perror("Failed to allocate memory for connection queue");
		exit(-1);
	}

	//init queue
	pthread_mutex_init(&CONN_LIST->lock, NULL);
	CONN_LIST->head = NULL;
	CONN_LIST->tail = NULL;
}/*}}}*/

/*create thread*/
static void create_worker(void*(*func)(void*),void* arg)
{/*{{{*/
	pthread_t       thread;
	pthread_attr_t  attr;
	int             ret;

#ifndef WIN32
sigset_t signal_mask;
sigemptyset (&signal_mask);
sigaddset (&signal_mask, SIGPIPE);
int rc = pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);
if (rc != 0) {
	printf("block sigpipe error/n");
} 
#endif

	pthread_attr_init(&attr);
	if ((ret = pthread_create(&thread, &attr, func, arg)) != 0) {
		fprintf(stderr, "Can't create thread: %s\n",strerror(ret));
		exit(1);
	}
}/*}}}*/

void* msg_monitor_handler(conn* c)
{/*{{{*/
	//分配thread
	THREAD* thread = threads + (c->sfd%THREAD_NUM);
	printf("thread name :%d %s\n",c->sfd,thread->name);

	
	CQ_ITEM* cq_item = malloc(sizeof(CQ_ITEM));
	if(cq_item == NULL){
		fprintf(stderr, "Can't allocate conn_queue_item!\n");
		exit(1);
	}
	cq_item->conn = c;

	//c->write_stat = conn_new_cmd;

	conn_queue_push(thread->write_conn_queue,cq_item);
	
	char buf[1];
	buf[0] = 'm';

	if (write(thread->notify_send, buf, 1) != 1) {
		perror("Writing to write thread notify pipe");
	}else{
	//pthread_mutex_lock(&c->lock);
		//c->write_notice = 1;
	//pthread_mutex_unlock(&c->lock);
	}
}/*}}}*/

/*消息监控线程处理函数*/
static void* worker_msg_dispatcher(void* arg)
{/*{{{*/
	THREAD* thread = arg;

	printf("msg dispatcher thread :%s\n",thread->name);


	int n = 0;
	while(1)
	{
		conn_list_loop(CONN_LIST,msg_monitor_handler);
		sleep(1);
		++n;
	}

	free(thread);
}/*}}}*/

/*线程libevent循环*/
static void* worker_libevent(void* arg)
{/*{{{*/
	THREAD* thread = arg;
	printf("worker :%d %s \n",thread->type,thread->name);

	event_base_dispatch(thread->base);
	event_base_free(thread->base);
	free(thread);
}/*}}}*/

static void setup_thread(THREAD* thread)
{/*{{{*/
	thread->base = event_base_new();
	if(!thread->base){
		fprintf(stderr, "Can't allocate thread event base\n");
		exit(1);
	}
	//注册读管道事件
	//thread->notify_event = event_new(thread->base,thread->notify_recv,EV_READ | EV_PERSIST,thread_event_handler,thread);
	event_set(&thread->notify_event,thread->notify_recv,EV_READ | EV_PERSIST, thread_event_handler, thread);
	event_base_set(thread->base,&thread->notify_event);

	if((event_add(&thread->notify_event,0)) == -1){
		fprintf(stderr, "Can't monitor libevent notify pipe\n");
		exit(1);
	}

	//注册线程连接队列
	thread->conn_queue = (CQ*)malloc(sizeof(CQ));
	if(NULL == thread->conn_queue){
		perror("Failed to allocate memory for connection queue");
		exit(-1);
	}
	thread->write_conn_queue = (CQ*)malloc(sizeof(CQ));
	if(NULL == thread->write_conn_queue){
		free(thread->conn_queue);
		perror("Failed to allocate memory for connection queue");
		exit(-1);
	}

	//init queue
	pthread_mutex_init(&thread->conn_queue->lock, NULL);
	thread->conn_queue->head = NULL;
	thread->conn_queue->tail = NULL;
	thread->conn_queue->size = 0;
}/*}}}*/

/*pipe可读事件回调*/
static void thread_event_handler(int fd, short which, void *arg)
{/*{{{*/
	THREAD* thread = arg;
	CQ_ITEM* cq_item;
	conn_list* conn_list_item;

	char buf[1];

	if (read(fd, buf, 1) != 1){
		fprintf(stderr, "Can't read from libevent pipe\n");
	}

	switch(buf[0]){
		//新连接client
		case 'c':
			cq_item = conn_queue_pop(thread->conn_queue);
			if(NULL == cq_item) break;

			conn* c = conn_new(cq_item->client_fd,cq_item->stat,1024,cq_item->event_flags,thread->base);
			if(NULL == c){
				fprintf(stderr, "Can't listen for events on fd %d\n",cq_item->client_fd);
				close(cq_item->client_fd);
			}
			c->thread = thread;

			/*add to conn_list -start- */
			conn_list_item = (conn_list*)malloc(sizeof(conn_list));
			if(NULL == conn_list_item){
				fprintf(stderr, "conn_list内存分配失败\n");
				conn_close(c);
			}
			conn_list_item->c = c;
			//c->conn_list_p = conn_list_item;

			//conn_list_push(CONN_LIST,conn_list_item);
			/* -end- */

			free(cq_item);
		break;

		case 'm':
			printf("[%s %d] write event !!\n",__FILE__,__LINE__);
			cq_item = conn_queue_pop(thread->write_conn_queue);
			if(NULL == cq_item) break;
			
			pthread_mutex_lock(&cq_item->conn->lock);

			if(cq_item->conn->sfd > 0){
				//cq_item->conn->write_notice = 1;
			}
			pthread_mutex_unlock(&cq_item->conn->lock);

			free(cq_item);
		break;
	}
}/*}}}*/

//分发新连接
void thread_dispatch_new_conn(int sfd,enum conn_stat init_stat,int event_flags)
{/*{{{*/
	//分配thread
	THREAD* thread = threads + (sfd%THREAD_NUM);

	char buf[1];
	buf[0] = 'c';

	CQ_ITEM* cq_item = malloc(sizeof(CQ_ITEM));
	if(cq_item == NULL){
		fprintf(stderr, "Can't allocate conn_queue_item!\n");
		exit(1);
	}
	cq_item->client_fd   = sfd;
	cq_item->event_flags = event_flags;
	cq_item->stat        = init_stat;

	conn_queue_push(thread->conn_queue,cq_item);

	if (write(thread->notify_send, buf, 1) != 1) {
		perror("Writing to thread notify pipe");
	}
}/*}}}*/

static void conn_queue_push(CQ* cq,CQ_ITEM* cq_item)
{/*{{{*/
	cq_item->next = NULL;
	pthread_mutex_lock(&cq->lock);
	if (NULL == cq->tail){
		cq->head = cq_item;
	}else{
		cq->tail->next = cq_item;
	}
	cq->tail = cq_item;
	cq->size++;
	pthread_mutex_unlock(&cq->lock);
}/*}}}*/

static CQ_ITEM* conn_queue_pop(CQ* cq)
{/*{{{*/
	CQ_ITEM *item;

	pthread_mutex_lock(&cq->lock);
	item = cq->head;
	if (NULL != item) {
		cq->head = item->next;
		if (NULL == cq->head){
			cq->tail = NULL;
		}
		cq->size--;
	}
	pthread_mutex_unlock(&cq->lock);
	return item;
}/*}}}*/
