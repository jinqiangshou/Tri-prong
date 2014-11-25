#ifndef __AUXILIARY_FUNC_H__
#define __AUXILIARY_FUNC_H__

#include <sys/epoll.h>
#include "comm_struct.h"
#include "comm_const.h"

//将描述符fd的event事件注册到epollfd事件表中
void addfd_to_epoll(int epollfd, int fd, __uint32_t event);

//将描述符fd从epollfd事件表中删除
void delete_from_epoll(int epollfd, int fd);

//将timeval结构体转化成timespec结构体，并存储在ts中
void timeval_to_timespec(const struct timeval* tv, struct timespec* ts);

//返回tv1-tv2的值，单位是微秒
double diff(const struct timeval* tv1, const struct timeval* tv2);

//将sockfd描述符设置为非阻塞，并返回旧的设置
int set_nonblock_sock(int sockfd);

/* 计算下一个start_this的值(开始发送的值)，存储在第一个参数t中 */
void next_send_time(struct timeval *t, const timeval *start_time, int conn_count);

//本工具的用法
void usage(const char* tool_name) __attribute__ ((noreturn));

//打印读取的参数信息
void print_info(void);

//检查参数设置是否合法
void check_setting(void);

//打印测试结果
void print_result(final_statistics *result);
#endif //__AUXILIARY_FUNC_H__
