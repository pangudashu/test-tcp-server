#include "conn.h"
#include "msg.h"
#include "thread.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>

int count = 0;
int fail = 0;
int come = 0;

/*
 * ================================
 * 连接链表
 * ================================
 */
void conn_list_push(conn_list_queue* cq,conn_list* cq_item)
{/*{{{*/
	cq_item->next = NULL;
	pthread_mutex_lock(&cq->lock);
	if (NULL == cq->tail){
		cq->head = cq_item;
		cq_item->prev = NULL;
	}else{
		cq->tail->next = cq_item;
		cq_item->prev = cq->tail;
	}
	cq->tail = cq_item;
	pthread_mutex_unlock(&cq->lock);
}/*}}}*/

conn_list* conn_list_pop(conn_list_queue* cq)
{/*{{{*/
	conn_list *item;

	pthread_mutex_lock(&cq->lock);
	item = cq->head;
	if (NULL != item) {
		if(NULL != item->next){
			item->next->prev = NULL;
		}
		cq->head = item->next;
		if (NULL == cq->head){
			cq->tail = NULL;
		}
	}
	pthread_mutex_unlock(&cq->lock);
	return item;
}/*}}}*/

void conn_list_del(conn_list_queue* cq,conn_list* item)
{/*{{{*/
	pthread_mutex_lock(&cq->lock);
	if(NULL != item->prev){
		item->prev->next = item->next;
	}else{
		cq->head = item->next;
	}
	if(NULL != item->next){
		item->next->prev = item->prev;
	}else{
		cq->tail = item->prev;
	}
	pthread_mutex_unlock(&cq->lock);
}/*}}}*/

void conn_list_loop(conn_list_queue* cq,void*(*callback)(conn*))
{/*{{{*/
	if(NULL == cq->head) return;
	pthread_mutex_lock(&cq->lock);
	conn_list* H = cq->head;

	while(H != NULL){
		if(H->c->sfd > 0){
			callback(H->c);
		}
		H = H->next;
	}
	pthread_mutex_unlock(&cq->lock);
}/*}}}*/
/*
 * ================================
 * END
 * ================================
 */

conn* conn_new(int sfd,enum conn_stat init_stat,size_t read_buf_size,const int event_flags,struct event_base* base)
{/*{{{*/
	conn* c;

	if(!(c = (conn*)calloc(1,sizeof(conn)))){
		perror("Failed to allocate conn object");
		return NULL;
	}

	c->sfd             = sfd;//(*c).
	c->stat            = init_stat;
	c->event_flags     = event_flags;
	c->read_buf_size   = read_buf_size;
	c->read_buf_ready  = 0;
	c->read_buf        = (char*)malloc((size_t)c->read_buf_size); //分配读缓冲区内存
	c->unparsed_buf    = c->read_buf;
	c->binary_request.body = NULL;

	c->write_buf_ready    = 0;
	c->write_buf_size     = MAX_WRITE_LINE;//1024;
	c->write_buf          = (char*)malloc((size_t)c->write_buf_size); //分配读缓冲区内存
	c->unwrite_buf        = c->write_buf;


	//c->event          = event_new(base,sfd,event_flags,event_handler,c); //事件注册
	event_set(&c->event, sfd, event_flags, event_handler,(void*)c);
	event_base_set(base, &c->event);

	
	/*
	if(!c->event){
		event_free(c->event);
		free(c);
		return NULL;
	}
	*/

	if(event_add(&c->event,0) == -1){
		free(c->read_buf);
		free(c);
		return NULL;
	}

	pthread_mutex_init(&c->lock, NULL);

	return c;
}/*}}}*/

static bool try_read_send_msg(conn* c)
{/*{{{*/
	int msg_len = sizeof(c->binary_response.header) + strlen(c->binary_response.body) + 1;

	printf("[write]msg len : %d\n",msg_len);

	if(c->write_buf != c->unwrite_buf){
		if(c->write_buf_ready > 0){
			memmove(c->write_buf,c->unwrite_buf,c->write_buf_ready);
		}
		c->unwrite_buf = c->write_buf;
	}
	
	int avail_len = c->write_buf_size - c->write_buf_ready;

	if(msg_len > avail_len){
		//扩容
		printf("[write]write_buf扩容\n");
		char* new_buf = realloc(c->write_buf,c->write_buf_size + (msg_len - avail_len));
		if(!new_buf){
			return false;
		}
		c->write_buf_size += (msg_len - avail_len);
		c->write_buf = c->unwrite_buf = new_buf;
	}
	printf("write_buf size : %d\n",c->write_buf_size);
	//copy header
	memcpy(c->write_buf + c->write_buf_ready,(char*)&c->binary_response.header,sizeof(c->binary_response.header));
	//copy body
	memcpy(c->write_buf + c->write_buf_ready + sizeof(c->binary_response.header),c->binary_response.body,strlen(c->binary_response.body));
	//copy \n
	char end = '\n';
	memcpy(c->write_buf + c->write_buf_ready + sizeof(c->binary_response.header) + strlen(c->binary_response.body),(char*)&end,sizeof(end));

	c->write_buf_ready += msg_len;

	return true;
}/*}}}*/

static enum send_result send_msg(conn* c)
{/*{{{*/
	if(c->sfd <= 0){
		return send_error;
	}

	if(c->write_buf_ready <= 0){
		return send_done;
	}

	ssize_t result = send(c->sfd,c->unwrite_buf,c->write_buf_ready, 0);
	if (result < 0) {
		if (errno == EAGAIN){
			return send_no;
		}
		return send_error;
	}
	c->unwrite_buf += result;
	
	if(result == c->write_buf_ready){
		c->write_buf_ready -= result;
		return send_done;
	}else{
		c->write_buf_ready -= result;
		return send_no;
	}
}/*}}}*/

static void event_handler(const int fd, const short which, void *arg)
{/*{{{*/
	conn* c = (conn*)arg;
	assert(NULL != c);

	c->which = which;

	if(fd != c->sfd){
		fprintf(stderr,"event fd doesn't match conn fd!\n");
		conn_close(c);
		return;
	}

	pthread_mutex_lock(&c->lock);
	drive_machine(c);
	pthread_mutex_unlock(&c->lock);

	return ;
}/*}}}*/

//状态机
static void drive_machine(conn* c)
{/*{{{*/
	bool stop = false;
	enum read_buf_res read_res;
	enum data_unpack_res unpack_res;//解包结果

	struct sockaddr_in addr;
	socklen_t addrlen;
	int client_fd;

	int nreqs = 20;

	printf("flag : %d\n",c->stat);


	while(!stop)
	{
		switch(c->stat){
			/*new conn dispatch to thread deal*/
			case conn_listening:
				addrlen = sizeof(addr);

				client_fd = accept(c->sfd,(struct sockaddr*)&addr,&addrlen);
				printf("%d\n",client_fd);


				if(client_fd == -1){
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						stop = true;
					}else if(errno == EMFILE){
						stop = true;
					}else{
						stop = true;
					}
					break;
				}

				//设置为非阻塞	
				if (fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL) | O_NONBLOCK) < 0) {
					perror("setting O_NONBLOCK");
					close(client_fd);
					break;
				}

				thread_dispatch_new_conn(client_fd,conn_new_cmd,EV_READ | EV_PERSIST);
				stop = true;
			break;

			case conn_new_cmd:
				--nreqs;

				if(nreqs >= 0){
					//reset conn_stat
					conn_set_stat(c,conn_waiting);
				}else{
					//to avoid the event occupy all the time
					stop = true;
					printf("over max loop time!!!!!!!!!!!!!\n");
				}
			break;

			case conn_read:
				read_res = try_to_read(c);

				switch(read_res){
					case read_error:
						conn_set_stat(c,conn_closing);
					break;
					case read_data_received:
						conn_set_stat(c,conn_parse_cmd);
					break;
					case read_no_data_received:
						conn_set_stat(c,conn_waiting);
					break;
				}
			break;

			case conn_waiting:
				if(!update_event(c,EV_READ | EV_PERSIST)){
					conn_set_stat(c, conn_closing);
					break;
				}
				stop=true;
				conn_set_stat(c,conn_read);
			break;

			case conn_parse_cmd:
                printf("解析:conn_parse_cmd\n");
#ifdef PROTOCOL_BINARY
				unpack_res = read_request_header_binary(c);


				switch(unpack_res){
					case DATA_UNPACK_ERROR:
						conn_set_stat(c,conn_closing);
					break;
					case DATA_UNPACK_BODY_OK:
						//deal request
						//阻塞
						msg_command_dispatch(c);

						c->read_buf_ready = c->read_buf_ready - (PROTOCOL_BINARY_HEADER_LENGTH + c->binary_request.header.request.bodylen);
						c->unparsed_buf += PROTOCOL_BINARY_HEADER_LENGTH + c->binary_request.header.request.bodylen; 
						//conn_set_stat(c,conn_write);
						conn_set_stat(c,conn_waiting);
					break;
					case DATA_UNPACK_HEADER_OK:
						printf("header ok !!!\n");
						printf("===============\nheader:Length:%d Opcode:%d\n==============\n",c->binary_request.header.request.bodylen,c->binary_request.header.request.opcode);
						c->read_buf_ready = c->read_buf_ready - PROTOCOL_BINARY_HEADER_LENGTH;
						c->unparsed_buf += PROTOCOL_BINARY_HEADER_LENGTH; 
					case DATA_UNPACK_WAIT:
						printf("wait!!!\n");
						conn_set_stat(c,conn_waiting);
					break;
				}
#else
				//文本协议
				if(!try_to_parse_cmd(c)){
					conn_set_stat(c,conn_waiting);
				}
#endif
			break;

			case conn_write:
			
				//将body内容复制到write_buf
				if(false == try_read_send_msg(c)){
					conn_set_stat(c,conn_closing);
				}

				//释放header
				free(c->binary_request.body);
				bzero(&c->binary_request.header,sizeof(c->binary_request.header));
				conn_set_stat(c,conn_writing);
			break;

			case conn_writing:
				//write from conn.wbuf
				printf("=========[response]==========\n");
				switch(send_msg(c)){
					case send_error:
						conn_set_stat(c,conn_closing);
					break;
					case send_done:
						free(c->binary_response.body);
						bzero(&c->binary_response.header,sizeof(c->binary_response.header));
						printf("write done!! %d\n",c->read_buf_ready);

						if(c->read_buf_ready > 0){
							conn_set_stat(c,conn_parse_cmd);
						}else{
							conn_set_stat(c,conn_new_cmd);
						}
					break;
					case send_no:break;
				}
				printf("=========[response end]==========\n");
			break;

			case conn_closing:
				printf("closing!!!!!!!\n");
				conn_close(c);
				stop = true;
			break;
		}
	}
}/*}}}*/

/* 分包 : 二进制协议 */
static enum data_unpack_res read_request_header_binary(conn* c)
{/*{{{*/
	//读body
	//不再预解析header!!!!!!!!!!!!!!!!!!!!!!!
	/*
	if(c->binary_request.header.body_len > 0){
		if(c->read_buf_ready <= c->binary_request.header.body_len){
			return DATA_UNPACK_WAIT;
		}

		if(*(c->unparsed_buf + c->binary_request.header.body_len) != '\n'){
			return DATA_UNPACK_ERROR;
		}

				
		char* request_body = malloc(c->binary_request.header.body_len);
		int i;
		for(i = 0;i < c->binary_request.header.body_len;++i){
			*(request_body + i) = *(c->unparsed_buf + PROTOCOL_BINARY_HEADER_LENGTH + i);
		}
		
		c->binary_request.body   = request_body;
		return DATA_UNPACK_BODY_OK;
	}
	*/
	//判断包头是否已读完
	if(c->read_buf_ready < PROTOCOL_BINARY_HEADER_LENGTH){
		return DATA_UNPACK_WAIT;
	}

	protocol_binary_request_header* req;
	req = (protocol_binary_request_header*)c->unparsed_buf;

    req->request.keylen = ntohs(req->request.keylen);
    req->request.bodylen = ntohl(req->request.bodylen);
    req->request.cas = ntohll(req->request.cas);

	int body_len = req->request.bodylen;
	int opcode   = req->request.opcode;

    printf("body len:%d opcode:%d\n",body_len,opcode);

	if(opcode >= 9999 || body_len > 1024*1024){
		//未知命令
		return DATA_UNPACK_ERROR;
	}

	if(c->read_buf_ready >= (body_len + PROTOCOL_BINARY_HEADER_LENGTH)){
		/*
		if(*(c->unparsed_buf + PROTOCOL_BINARY_HEADER_LENGTH + body_len) != '\n'){
			return DATA_UNPACK_ERROR;
		}
		*/


		char* request_body = (char*)malloc((size_t)body_len);
		bzero(request_body,body_len);

		int i;
		for(i = 0;i < body_len;i++){
			*(request_body + i) = *(c->unparsed_buf + PROTOCOL_BINARY_HEADER_LENGTH + i);
		}

		c->binary_request.header = *req;
		c->binary_request.body   = request_body;
		return DATA_UNPACK_BODY_OK;
	}else{
		//不再预解析header!!!!!!!!!!!!!!!!!!!!!!!
		//
		//
		//c->binary_request.header = *req;
		//return DATA_UNPACK_HEADER_OK;
		return DATA_UNPACK_WAIT;
	}
}/*}}}*/

/* 分包 : 文本协议 */
static bool try_to_parse_cmd(conn* c)
{/*{{{*/
	assert(c != NULL);
	char* end = memchr(c->unparsed_buf,'\n',c->read_buf_ready);

	if(!end){
		return false;
	}

	printf("have end!\n");

	char* new = end + 1;

	int cmd_size = end - c->unparsed_buf;

	if((end - c->unparsed_buf) > 1 && *(end-1) == '\r'){
		--end;
	}
	*end = '\0';

	//命令处理
	command_handler(c,c->unparsed_buf);

	/**
	 * [warn]在这里偏移read_buf指针将导致每次解析一个cmd都需要一次内存拷贝
	 * 所以正确的处理方式是在conn中保存未解析buf指针，try_to_read中再移除已解析buf
	 */	
	c->read_buf_ready -= new - c->unparsed_buf;
	c->unparsed_buf = new;

	return true;
}/*}}}*/

static void command_handler(conn* c,char* command)
{/*{{{*/
	uint8_t len[4] = {0};
	uint8_t cmd[4] = {0};

	int i;
	/*包头[0-4B]*/
	for(i = 0;i < 4;++i){
		len[i] = *(command + i);
	}
	/*命令[4-14B]*/
	for(i = 0;i < 4;++i){
		cmd[i] = *(command + i + 4);
	}

	printf("%d  %d\n",sizeof(command),len);

	return;

	/*
	int msg_len = atoi(len);
	char msg[msg_len];
	char decode_msg[msg_len];
	bzero(msg,msg_len);
	bzero(decode_msg,msg_len);
	*/

	/*消息[14-msg_len]*/
	/*
	for(i = 0;i < msg_len;++i){
		msg[i] = *(command + i + 4 + 10);
	}

	decode(msg,decode_msg);

	printf("%s\n",decode_msg);
	*/

	//deal command
}/*}}}*/

static bool update_event(conn* c,const int new_event_flags)
{/*{{{*/
	assert(c != NULL);
	struct event_base *base = c->event.ev_base;
	if(new_event_flags == c->event_flags) return true;
	if(event_del(&c->event) == -1) return false;

	//c->event = event_new(base,c->sfd,new_event_flags,event_handler,c);

	event_set(&c->event, c->sfd, new_event_flags, event_handler,(void*)c);
	event_base_set(base, &c->event);

	/*
	if(!c->event){
		event_free(c->event);
		return false;
	}
	*/
	if(event_add(&c->event,0) == -1) return false;
	return true;
}/*}}}*/

static enum read_buf_res try_to_read(conn* c)
{/*{{{*/
	assert(c != NULL);
	int read_len;
	int num_allocs = 0;//最大扩容次数
	enum read_buf_res read_res = read_no_data_received;
	//printf("will wait to read! %d\n",c->read_buf_ready);

	if(c->read_buf != c->unparsed_buf){
		if(c->read_buf_ready != 0){
			//有已读入buf未处理字符串
			memmove(c->read_buf,c->unparsed_buf,c->read_buf_ready);
		}
		//错误：c->read_buf = c->unparsed_buf;
		c->unparsed_buf = c->read_buf;
	}

	while(1)
	{
		//扩容
		if(c->read_buf_size <= c->read_buf_ready){
			if(num_allocs >= 20){
				//buf too big!! need to deal
				return read_res;
			}
			++num_allocs;
			/**================[warn]==================
			 * 不能直接将realloc返回结果指向c->read_buf,
			 * 因为出错的情况下将造成c->read_buf内存泄露
			 */
			char* new_buf = realloc(c->read_buf,c->read_buf_size*2);
			if(!new_buf){
				c->read_buf_size = 0;
				return read_error;
			}
			c->read_buf = c->unparsed_buf = new_buf;//read_buf 指向新地址
			c->read_buf_size *= 2;
		}

		int buf_avail_size = c->read_buf_size - c->read_buf_ready;//剩余buf长度

		read_len = read(c->sfd,c->read_buf + c->read_buf_ready,buf_avail_size);


		if(read_len > 0){
			c->read_buf_ready += read_len;

			read_res = read_data_received;
			printf("[----------current----------]%d\n",c->read_buf_ready);

			if(read_len == buf_avail_size){
				//未读完，需要扩大buf内存
				continue;
			}else{
				break;
			}
		}

		if(read_len == 0){
			return read_error;
		}else if(read_len == -1){
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				break;
			}
			return read_error;
		}
	}

	return read_res;
}/*}}}*/

static void conn_set_stat(conn* c,enum conn_stat stat)
{/*{{{*/
	if(c->stat == stat) return ;
	c->stat = stat;
}/*}}}*/

void conn_close(conn* c)
{/*{{{*/
	assert(c != NULL);
	event_del(&c->event);
	close(c->sfd);

	if(c->conn_list_p != NULL){
		conn_list_del(CONN_LIST,c->conn_list_p);
		free(c->conn_list_p);
		c->conn_list_p = NULL;
	}

	free(c->read_buf);
	c->read_buf = NULL;
	free(c->write_buf);
	c->write_buf = NULL;

	free(c);
}/*}}}*/
