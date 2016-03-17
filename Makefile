index:agent.c conn.c msg.c thread.c
	gcc -o memcached-test agent.c conn.c msg.c thread.c util.c -levent
clean:
	rm -rf test-mem
