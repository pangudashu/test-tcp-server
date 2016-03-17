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

#define MAX_LINE 1024
#define PORT 11311

void do_read(evutil_socket_t fd, short events, void *arg);
void do_write(evutil_socket_t fd, short events, void *arg);

struct fd_state {
	int fd;
    char buffer[MAX_LINE];
    size_t buffer_used;

    size_t n_written;
    size_t write_upto;

    struct event *read_event;
    struct event *write_event;
};

struct fd_state *alloc_fd_state(struct event_base *base, evutil_socket_t fd)
{
    struct fd_state *state = (struct fd_state *)malloc(sizeof(struct fd_state));
    if (!state) {
		close(fd);
        return NULL;
    }
	bzero(state->buffer,sizeof(state->buffer));

    state->read_event = event_new(base, fd, EV_READ | EV_TIMEOUT | EV_PERSIST, do_read, state);
    if (!state->read_event) {
		close(fd);
        free(state);
        return NULL;
    }

    state->write_event = event_new(base, fd, EV_WRITE | EV_PERSIST, do_write, state);
    if (!state->write_event) {
		close(fd);
        event_free(state->read_event);
        free(state);
        return NULL;
    }

    assert(state->write_event);

	state->fd = fd;
    return state;
}

void free_fd_state(struct fd_state *state)
{
    event_free(state->read_event);
    event_free(state->write_event);
	close(state->fd);
    free(state);
}

void do_read(evutil_socket_t fd, short events, void *arg)
{
	printf("begin read !\n");
    struct fd_state *state = arg;
    char buf[1024];
    int i = 0;
    ssize_t result;
	bzero(buf,sizeof(buf));

    while (1) {
        // assert(state->write_event);
        result = read(fd, buf + i,1);
        if (result <= 0) break;
		i += result;
        printf("[%s][%d]buf=[%s]len=[%d]\n", __FILE__, __LINE__, buf,result);
    }

	char* el = memchr(buf, '\n',i);
	printf("end : %s\n",el);

    memcpy(state->buffer,buf,sizeof(buf));
    assert(state->write_event);
    event_add(state->write_event, NULL);
    state->write_upto = state->buffer_used;

	printf("result : %d buf total : %s\n",result,state->buffer);

    if (result == 0) {
        perror("client closed");
        free_fd_state(state);
    } else if (result < 0) {
        if (errno == EAGAIN)    // XXXX use evutil macro
            return;
        perror("recv");
        free_fd_state(state);
    }
}

void do_write(evutil_socket_t fd, short events, void *arg)
{
    struct fd_state *state = arg;

    //while (state->n_written < state->write_upto)
    {
        //ssize_t result = send(fd, state->buffer + state->n_written,
        //state->write_upto - state->n_written, 0);
		char res[] = {'a','b','\0','d'};
        ssize_t result = send(fd, res/*state->buffer*/,sizeof(res) /*strlen(state->buffer)*/, 0);
        if (result < 0) {
            if (errno == EAGAIN)    // XXX use evutil macro
                return;
            free_fd_state(state);
            return;
        }
        assert(result != 0);
        state->n_written += result;
    }

    //if (state->n_written == state->buffer_used)
    {
        state->n_written = state->write_upto = state->buffer_used = 1;
    }

    event_del(state->write_event);
}

void do_accept(evutil_socket_t listener, short event, void *arg)
{
    struct event_base *base = arg;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);

    int fd = accept(listener, (struct sockaddr *)&ss, &slen);

	printf("accept client : %d max : %d\n",fd,FD_SETSIZE);
    if (fd < 0) {        // XXXX eagain??
        perror("accept");
    //} else if (fd > FD_SETSIZE) {
    //    close(fd);    // XXX replace all closes with EVUTIL_CLOSESOCKET */
    } else {
        struct fd_state *state;
        evutil_make_socket_nonblocking(fd);
        state = alloc_fd_state(base, fd);

        assert(state);    /*XXX err */
        assert(state->write_event);
		struct timeval t = {5,0};
        event_add(state->read_event,&t);
    }
}

int main(int argc, char **argv)
{
    evutil_socket_t listener;
    struct sockaddr_in sin;
    struct event_base *base;
    struct event *listener_event;

    setvbuf(stdout, NULL, _IONBF, 0);
    
    base = event_base_new();
    if (!base)
        return -1;        /*XXXerr */

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(PORT);

    listener = socket(AF_INET, SOCK_STREAM, 0);
    evutil_make_socket_nonblocking(listener);

#ifndef WIN32
    {
        int one = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one,
             sizeof(one));
    }
#endif

    if (bind(listener, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("bind");
        return -1;
    }

    if (listen(listener, 16) < 0) {
        perror("listen");
        return -1;
    }

    listener_event = event_new(base, listener, EV_READ | EV_PERSIST, do_accept,(void *)base);
    /*XXX check it */
    event_add(listener_event,NULL);
    event_base_dispatch(base);
    event_base_free(base);

    return 0;
}
