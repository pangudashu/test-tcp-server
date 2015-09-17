index:agent.c conn.c msg.c thread.c
	gcc -o test-mem agent.c conn.c msg.c thread.c -levent
clean:
	rm -rf test-mem
