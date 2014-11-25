#include <stdio.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdlib.h>

#include "auxiliary_func.h"
#include "comm_struct.h"
#include "comm_const.h"

extern int press_rate;
extern double press_interval;
extern host_info host;
extern int press_time;

void print_result(final_statistics *result)
{
	printf("Total bytes transfered: %ld\n", result->total_byte);
	return ;
}


//如何使用本工具
void usage(const char* tool_name)
{
	fprintf( stderr, "usage:  %s\n", tool_name );
	fprintf( stderr, "        -r [or --rate] N                 [REQUIRED] specify the press rate (unit: query per second).\n" ); 
	fprintf( stderr, "        -t [or --time] N                 [REQUIRED] test time (unit: second). \n" );
	fprintf( stderr, "        -h [or --host] XXX.XXX.XXX.XXX   target host ip. The default host ip is 127.0.0.1 .\n" );
	fprintf( stderr, "        -p [or --port] N                 port number. The default port number is 80.\n" );
	fprintf( stderr, "        -u [or --uri ] /index.html       uri of the request.\n");
	exit(1);
}

void check_setting(void)
{
	if(press_rate <= 0 || press_rate >= MAXRATE){
		fprintf(stderr, "The press rate is illegal. The request rate should be between 1 and %d QPS.\n", MAXRATE);
		exit(1);
	}
	
	if(press_time <= 0 || press_time > MAXTIME){
		fprintf(stderr, "The press time is too long! The test time should be between 1 and %d seconds.\n", MAXTIME);
		exit(1);
	}
	
	return ;
}
	
//打印读取的参数信息
void print_info(void)
{
	fprintf( stdout, "Host       : %s\n", host.host_ip );
	fprintf( stdout, "Port       : %d\n", host.port );
	fprintf( stdout, "Uri        : %s\n", host.uri);
	fprintf( stdout, "Target QPS : %d\n", press_rate );
	fprintf( stdout, "Test Time  : %d\n", press_time);
	return;
}

//将描述符fd的event事件注册到epollfd事件表中
void addfd_to_epoll(int epollfd, int fd, __uint32_t event)
{
	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = event;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

//将描述符fd从epollfd事件表中删除
void delete_from_epoll(int epollfd, int fd)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
}

//将timeval结构体转化成timespec结构体，并存储在ts中
void timeval_to_timespec(const struct timeval* tv, struct timespec* ts)
{
	ts->tv_sec = tv->tv_sec;
	ts->tv_nsec = 1000* tv->tv_usec;
}

//返回tv1-tv2的值，单位是微秒
double diff(const struct timeval* tv1, const struct timeval* tv2)
{
	return (tv1->tv_sec - tv2->tv_sec)*1000000.0+(tv1->tv_usec - tv2->tv_usec);
}

//将sockfd描述符设置为非阻塞，并返回旧的设置
int set_nonblock_sock(int sockfd)
{
	int old_option = fcntl(sockfd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(sockfd, F_SETFL, new_option);
	return old_option;
}

/* 计算下一个start_this的值(开始发送的值)，存储在第一个参数t中 */
void next_send_time(struct timeval *t, const timeval *start_time, int conn_count)
{
	int how_many_1s = conn_count / press_rate;
	int time_within_1s = (int)((conn_count % press_rate) * press_interval);
	
	if(start_time->tv_usec + time_within_1s > 1000000){
		t->tv_sec = start_time->tv_sec + how_many_1s + 1;
		t->tv_usec = start_time->tv_usec + time_within_1s - 1000000;
	}else{
		t->tv_sec = start_time->tv_sec + how_many_1s;
		t->tv_usec = start_time->tv_usec + time_within_1s;
	}
}
