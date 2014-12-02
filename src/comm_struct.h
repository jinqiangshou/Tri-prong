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

//存储socket描述符以及该请求在request_data数组中对应的位置
//该结构体被用于在管道中传输
typedef struct __req_pos_pair {
	int sockfd;
	int position;
} req_pos_pair;

//用于存储每次连接的数据
typedef struct __request_data {
	short is_connected; //TCP请求是否连接成功,压测执行时只能被thread_send更改，只能被thread_send读
	short is_replied; //请求是否收到回复,压测执行时只能被thread_recv更改，只能被thread_recv读
	timeval init_connect; //只能被thread_conn更改，会被thread_send和thread_recv读
	unsigned int connect_time; //只能被thread_send更改
	unsigned int start_send_time; //只能被thread_send更改
	unsigned int recv_finish_time; //只能被thread_recv更改
	unsigned long byte_transfered; //只能被thread_recv更改，单位为字节
	int sockfd; //本data属于哪个socket，只能被thread_conn更改，被thread_send和thread_recv读
} request_data;

//用于最终输出的数据统计，以下结构体所有元素只能被main函数更改
typedef struct __final_statistics {
	/* 连接相关的数据 */
	unsigned int avg_connect_time;
	unsigned int min_connect_time;
	unsigned int max_connect_time;
	int connect_fail_count;
	
	/* 发请求到收回复的数据，单位都是微秒 us */
	unsigned int avg_response_time;
	unsigned int min_response_time;
	unsigned int max_response_time;
	int response_fail_count;
	
	/* 完整周期总用时（连接+收发请求）数据 */
	unsigned int avg_request_time;
	unsigned int min_request_time;
	unsigned int max_request_time;
	int total_fail_count; // equal to ( response_timeout_count + connect_timeout_count )
	
	int total_request;
	double fail_percent; // equal to ( total_fail_count / total_request ) * 100%
	unsigned long total_byte;
} final_statistics;
#endif //__COMM_STRUCT_H__
