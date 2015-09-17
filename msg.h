#ifndef _MSG_H
#define _MSG_H

#include "conn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum command_deal_result{
	CMD_RES_SUCCESS,
	CMD_RES_ERROR,
	CMD_DATA_UNVALID,
};


void msg_command_dispatch(conn* c);



#endif
