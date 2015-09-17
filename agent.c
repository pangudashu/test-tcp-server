#include "conn.h"
#include "thread.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <event2/event.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <linux/tcp.h>
#include <signal.h>


#define PORT 11333
#define MAX_READ_LINE 1024


static int server_socket(void)
{/*{{{*/
	int sfd;
	struct sockaddr_in sin;
	int error;

	int flags =1;

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(PORT);

	sfd = socket(AF_INET, SOCK_STREAM, 0);

	if ((flags = fcntl(sfd, F_GETFL, 0)) < 0 ||
		fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0) {
		perror("setting O_NONBLOCK");
		close(sfd);
		return -1;
	}

	setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));

	error = setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags));
	if (error != 0){
		perror("setsockopt");
	}
	error = setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (void *)&flags, sizeof(flags));
	if (error != 0){
		perror("setsockopt");
	}

	if (bind(sfd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("bind");
		close(sfd);
		return -1;
	}

	/*listen waiting buffer*/
	if (listen(sfd, 1024) < 0) {
		perror("listen");
		close(sfd);
		return -1;
	}

	return sfd;
}/*}}}*/

int main(void)
{
	int sfd;
	struct event_base *base;

	//thread init
	thread_init();

	//socket
	assert((sfd = server_socket()) > 0);

	//libevent init
	if(!(base = event_base_new())){
		perror("event_base_new");
		close(sfd);
		return -1;
	}

	conn* c = conn_new(sfd,conn_listening,1,EV_READ | EV_PERSIST,base);
	if(NULL == c){
		perror("Failed to event_new");
		//close(sfd);
		conn_close(c);
		return -1;
	}

	//事件循环
	//event_base_dispatch(base);
	//event_base_free(base);
	if(event_base_loop(base,0) != 0){
		printf("base loop error\n");
	}

	return 0;
}
