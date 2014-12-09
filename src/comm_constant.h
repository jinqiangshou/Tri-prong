#ifndef __COMM_CONST_H__
#define __COMM_CONST_H__

#define MAX_WAIT_EVENT 20000
#define MAX_EPOLL_WAIT_TIME 2 //unit: ms
#define REQUEST_TIME_OUT 3 //unit: second. Request failed if it's not replied within 3 seconds.
#define MAX_URL_SIZE 1000
#define MAX_REQUEST_SIZE 2048
#define MAX_RECV_SIZE_ONE_TIME 8192 //unit: bytes
#define MAX_RATE 5000
#define MAX_TIME 100

#endif //__COMM_CONST_H__
