#ifndef _CONN_HEAD_
#define _CONN_HEAD_

#include <sys/time.h>
//#include <event2/event.h>
#include <event.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <pthread.h>
#include "protocol_binary.h"

#define MAX_WRITE_LINE 1024
#define PROTOCOL_BINARY
#define PROTOCOL_BINARY_HEADER_LENGTH 24



enum conn_stat{
	conn_listening = 1,
	conn_new_cmd = 2,
	conn_read = 3,
	conn_waiting = 4,
	conn_closing = 5,
	conn_parse_cmd = 6,
	conn_write = 7,
	conn_writing = 8,
};

enum read_buf_res{
	read_data_received,
	read_error,
	read_no_data_received
};
enum data_unpack_res{
	DATA_UNPACK_WAIT,
	DATA_UNPACK_ERROR,
	DATA_UNPACK_HEADER_OK,
	DATA_UNPACK_BODY_OK,
};

enum send_result{
	send_error,
	send_done,
	send_no,
};


typedef struct conn_list conn_list;
typedef struct conn conn;
struct conn{
	int            sfd;
	enum conn_stat stat;
	//struct event*  event;
	struct event   event;
	short          event_flags;

	char*          read_buf;       //收到数据
	int            read_buf_size;  // read_buf size
	int            read_buf_ready; // read_buf size   read start from this position
	char*          unparsed_buf;   //未解析字符串

	//request
	struct{
		protocol_binary_request_header header;
		char*                          body;
	}binary_request;
	
	//response
	struct{
		protocol_binary_response_header header;
		char*                           body;
	}binary_response;

	char*          write_buf;
	int            write_buf_size;
	int            write_buf_ready;
	char*          unwrite_buf;

	short          which;          //触发事件

	void*          thread;         //deal thread

	conn_list*     conn_list_p;

	pthread_mutex_t lock;
};

/*client链表*/
struct conn_list{
	conn* c;
	conn_list* prev;
	conn_list* next;
};

typedef struct conn_list_queue conn_list_queue;
struct conn_list_queue{
	conn_list*        head;
	conn_list*        tail;
	pthread_mutex_t   lock;
};




conn* conn_new(int sfd,enum conn_stat init_stat, size_t read_buf_size,const int event_flags, struct event_base* base);
static void event_handler(const int fd, const short which, void *arg);
void conn_close(conn* c);
static void drive_machine(conn* c);
static void conn_set_stat(conn* c,enum conn_stat stat);
static enum read_buf_res try_to_read(conn* c);
static bool try_to_parse_cmd(conn* c);
static bool update_event(conn* c,const int new_event_flags);
static void command_handler(conn* c,char* command);

void conn_list_push(conn_list_queue* cq,conn_list* cq_item);
void conn_list_del(conn_list_queue* cq,conn_list* item);
void conn_list_loop(conn_list_queue* cq,void*(*callback)(conn*));
static bool try_read_send_msg(conn* c);
static enum send_result send_msg(conn* c);
static enum data_unpack_res read_request_header_binary(conn* c);


conn_list_queue* CONN_LIST;

#endif
