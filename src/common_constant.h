#ifndef __COMMON_CONSTANT_H__
#define __COMMON_CONSTANT_H__

#define MAX_WAIT_EVENT 20000
#define MAX_EPOLL_WAIT_TIME 1 //unit: ms
#define REQUEST_TIME_OUT 3 //unit: second. Request failed if it's not replied within 3 seconds.
#define REQUEST_EXPIRE_TIME 500000 //unit: us ( NOTE: 1s = 1000000us ). If recv generates EAGAIN, we still listen the socket in epoll until it expires. 
#define MAX_URL_SIZE 1000
#define MAX_REQUEST_SIZE 2048
#define MAX_RECV_SIZE_ONE_TIME 8192 //unit: bytes
#define MAX_RATE 10000
#define MAX_TIME 100
#define MAX_URL_NUMBER 2
#define MAX_HOST_IP_LENGTH 50
#define MAX_URI_LENGTH 500

#endif //__COMMON_CONSTANT_H__
