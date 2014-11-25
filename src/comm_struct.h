#ifndef __COMM_STRUCT_H__
#define __COMM_STRUCT_H__

#include <sys/time.h>

//存储所有命令行参数信息
typedef struct __parameters_set {
	int __press_rate;
	int __press_time;
	char *__host_ip;
	short __port;
	char *__uri;
} parameters_set;

//存储主机信息
typedef struct __host_info {
	char *host_ip;
	short port;
	char *uri;
} host_info;

//用于存储每次连接的数据
typedef struct __request_data {
	timeval init_connect;
	timeval connect_success;
	timeval start_send;
	timeval recv_finish;
	unsigned long byte_transfered;
} request_data;

//用于最终输出的数据统计
typedef struct __final_statistics {
	/* 连接相关的数据 */
	timeval avg_connect_time;
	timeval min_connect_time;
	timeval max_connect_time;
	int connect_timeout_count;
	
	/* 发请求到收回复的数据 */
	timeval avg_response_time;
	timeval min_response_time;
	timeval max_response_time;
	int response_timeout_count;
	
	/* 完整周期总用时（连接+收发请求）数据 */
	timeval avg_request_time;
	timeval min_request_time;
	timeval max_request_time;
	int total_fail_count; // equal to ( response_timeout_count + connect_timeout_count )
	
	int total_request;
	double fail_percent; // equal to ( total_fail_count / total_request ) * 100%
	unsigned long total_byte;
} final_statistics;
#endif //__COMM_STRUCT_H__
