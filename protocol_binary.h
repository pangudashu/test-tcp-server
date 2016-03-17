#ifndef _PROTOCOL_BINARY_H_
#define _PROTOCOL_BINARY_H_

#include <stdio.h>
#include <unistd.h>


typedef union{
    struct {
        uint8_t magic;
        uint8_t opcode;
        uint16_t keylen;
        uint8_t extlen;
        uint8_t datatype;
        uint16_t reserved;
        uint32_t bodylen;
        uint32_t opaque;
        uint64_t cas;
    } request;
    uint8_t bytes[24];
}protocol_binary_request_header;

typedef struct{
    uint32_t body_len;
		uint32_t magic;
}protocol_binary_response_header;


typedef enum {
    PROTOCOL_BINARY_CMD_GET = 0x00,
    PROTOCOL_BINARY_CMD_SET = 0x01,
    PROTOCOL_BINARY_CMD_ADD = 0x02,
    PROTOCOL_BINARY_CMD_REPLACE = 0x03,
    PROTOCOL_BINARY_CMD_DELETE = 0x04,
    PROTOCOL_BINARY_CMD_INCREMENT = 0x05,
    PROTOCOL_BINARY_CMD_DECREMENT = 0x06,
    PROTOCOL_BINARY_CMD_QUIT = 0x07
} protocol_binary_command;



#endif
