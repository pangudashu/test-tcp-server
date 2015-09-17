#include "msg.h"
#include "conn.h"
#include "protocol_binary.h"
#include <stdio.h>
#include <unistd.h>

/*线程阻塞*/
void msg_command_dispatch(conn* c)
{
	while(1){
		printf("=====================[%d]========================\n",c->sfd);
		printf("Header : Length %d Opcode %d Magic %d\n",
													c->binary_request.header.body_len,
													c->binary_request.header.opcode,
													c->binary_request.header.magic
													);
		//uint32_t* uid;
		//uint32_t* from_uid = (uint32_t*)malloc(sizeof(uint32_t));
		//uint32_t* to_uid = (uint32_t*)malloc(sizeof(uint32_t));
		//memcpy(from_uid,c->binary_request.body,sizeof(uint32_t));
		//memcpy(to_uid,c->binary_request.body + sizeof(uint32_t),sizeof(uint32_t));

		printf("Body :%s\n",c->binary_request.body);
		printf("=================================================\n");

		//free(from_uid);
		//free(to_uid);

		//c->binary_response.body = /*c->binary_request.body;*/"hello,这是要返回的处理数据^_^";
		c->binary_response.body = malloc((size_t)strlen(c->binary_request.body));
		strcpy(c->binary_response.body,c->binary_request.body);

		protocol_binary_response_header res_header = {
			.body_len = strlen(c->binary_response.body),
			.magic    = c->binary_request.header.magic,
		};

		c->binary_response.header = res_header;

/*
 *	OP_ADD_FRIEND     = 0x3E9,
	OP_CHAN_INVITE    = 0x3EA,
	OP_CHANGE_POS     = 0x3EB,
	OP_CHANGE_USER    = 0X3EC,

 *
 * */
		switch(c->binary_request.header.opcode){
			case OP_ADD_FRIEND:break;
			case OP_CHAN_INVITE:
				//sendmsg(c->sfd,&msg,0);
				//send(c->sfd,out,6 + strlen(str),0);
				break;
			case OP_CHANGE_POS:break;
			case OP_CHANGE_USER:break;
			default:
				printf("unknown command!\n");
			break;
		}


		//sleep(2);
		break;
	}
}
