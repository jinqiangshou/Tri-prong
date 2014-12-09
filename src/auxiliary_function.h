#ifndef __AUXILIARY_FUNCTION_H__
#define __AUXILIARY_FUNCTION_H__

#include <sys/epoll.h>

//将描述符fd的event事件注册到epollfd事件表中，事件被触发时，返回文件描述符fd的值
void addfd_to_epoll( int epollfd, int fd, __uint32_t event );

//将描述符fd的event事件注册到epollfd事件表中，事件被触发时，返回指针ptr的值
void addfd_to_epoll2( int epollfd, int fd, __uint32_t event, void *ptr );

//将描述符fd从epollfd事件表中删除
void delete_from_epoll( int epollfd, int fd );

//将timeval结构体转化成timespec结构体，并存储在ts中
void timeval_to_timespec( const struct timeval* tv, struct timespec* ts );

//返回tv1-tv2的值，单位是微秒，返回值为double类型
double dbl_time_diff( const struct timeval* tv1, const struct timeval* tv2 );

//返回tv1-tv2的值，单位是微秒，返回值为unsigned int类型，要求tv1必须大于tv2
unsigned int uint_time_diff( const struct timeval* tv1, const struct timeval* tv2 );

//时间比较，如果tv1更晚，返回-1；如果tv2更晚，返回1；如果相等，返回0
int time_compare( const struct timeval* tv1, const struct timeval* tv2 );

//将sockfd描述符设置为非阻塞，并返回旧的设置
int set_nonblock_sock( int sockfd );

#endif //__AUXILIARY_FUNCTION_H__
