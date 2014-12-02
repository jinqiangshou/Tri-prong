#include <stdio.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdlib.h>

#include "auxiliary_func.h"

//将描述符fd的event事件注册到epollfd事件表中
void addfd_to_epoll(int epollfd, int fd, __uint32_t event)
{
	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = event;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

void addfd_to_epoll2(int epollfd, int fd, __uint32_t event, void *ptr)
{
	struct epoll_event ev;
	ev.data.ptr = ptr;
	ev.events = event;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

//将描述符fd从epollfd事件表中删除
void delete_from_epoll(int epollfd, int fd)
{
	//ev is of no use in EPOLL_CTL_DEL, but it must be passed with kernel less than 2.6.9
	struct epoll_event ev = {0};  
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
}

//将timeval结构体转化成timespec结构体，并存储在ts中
void timeval_to_timespec(const struct timeval* tv, struct timespec* ts)
{
	ts->tv_sec = tv->tv_sec;
	ts->tv_nsec = 1000* tv->tv_usec;
}

//返回tv1-tv2的值，单位是微秒
double dbl_time_diff(const struct timeval* tv1, const struct timeval* tv2)
{
	return (tv1->tv_sec - tv2->tv_sec)*1000000.0+(tv1->tv_usec - tv2->tv_usec);
}

//返回tv1-tv2的值，单位是微秒
unsigned int uint_time_diff(const struct timeval* tv1, const struct timeval* tv2)
{
	return (tv1->tv_sec - tv2->tv_sec)*1000000+(tv1->tv_usec - tv2->tv_usec);
}

//将sockfd描述符设置为非阻塞，并返回旧的设置
int set_nonblock_sock(int sockfd)
{
	int old_option = fcntl(sockfd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(sockfd, F_SETFL, new_option);
	return old_option;
}

//时间比较，如果tv1更晚，返回-1；如果tv2更晚，返回1；如果相等，返回0
int time_compare(const struct timeval* tv1, const struct timeval* tv2)
{
	if(tv1->tv_sec > tv2->tv_sec){
		return -1;
	}else if(tv1->tv_sec < tv2->tv_sec){
		return 1;
	}
	
	if(tv1->tv_usec > tv2->tv_usec){
		return -1;
	}else if(tv1->tv_usec < tv2->tv_usec){
		return 1;
	}
	
	return 0;
}
