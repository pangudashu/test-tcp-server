#ifndef _PROTOCOL_BINARY_H_
#define _PROTOCOL_BINARY_H_

#include <stdio.h>
#include <unistd.h>

typedef struct{
		uint32_t opcode;
		uint32_t magic;
		uint32_t body_len;
}protocol_binary_request_header;

typedef struct{
		uint32_t body_len;
		uint32_t magic;
}protocol_binary_response_header;


enum opcode_list{
	OP_ADD_FRIEND     = 0x3E9,
	OP_CHAN_INVITE    = 0x3EA,
	OP_CHANGE_POS     = 0x3EB,
	OP_CHANGE_USER    = 0X3EC,
};



#endif
