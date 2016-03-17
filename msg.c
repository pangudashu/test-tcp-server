#include "msg.h"
#include "conn.h"
#include "protocol_binary.h"
#include <stdio.h>
#include <unistd.h>

/*线程阻塞*/
void msg_command_dispatch(conn* c)
{
	while(1){
		printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		printf("Header : \n\tMagic %d Opcode %d Keylen %d Extlen %d Datatype %d Status %d Bodylen %d Opaque %d Cas %d\n",
													c->binary_request.header.request.magic,
													c->binary_request.header.request.opcode,
													c->binary_request.header.request.keylen,
													c->binary_request.header.request.extlen,
													c->binary_request.header.request.datatype,
													c->binary_request.header.request.reserved,
													c->binary_request.header.request.bodylen,
													c->binary_request.header.request.opaque,
													c->binary_request.header.request.cas
													);

		printf("Body : \n\tKey %s Value %s\n",
                c->binary_request.body + c->binary_request.header.request.extlen,
                c->binary_request.body + c->binary_request.header.request.extlen + c->binary_request.header.request.keylen
                );
		printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");


		switch(c->binary_request.header.request.opcode){
			default:
			break;
		}


		sleep(20);
		break;
	}
}
