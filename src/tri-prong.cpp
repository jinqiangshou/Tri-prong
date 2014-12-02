#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>

#include "comm_const.h"
#include "comm_struct.h"
#include "auxiliary_func.h"
#include "build_request.h"
#include "output.h"
#include "version.h"


int fd[2]; //fd用于conn和send之间的通信
int fd2[2]; //fd2用于send和recv之间的通信

static char* tool_name = NULL; //The title of this program

//以下三个参数的关系为 press_rate * press_time = press_number
//命令行参数中只能指定两个，另一个计算得出
int press_rate = 0; //单位 request per second，每秒请求数
int press_time = 0; //单位 秒，压测的时间
int press_number = 0; //单位 request，压测的次数

double press_interval = 1000000.0; //该数值等于press_rate的倒数，单位为 微秒

char temp_host_ip[] = "127.0.0.1";
char temp_uri[]="/";
host_info host = {temp_host_ip, 80, temp_uri};//存储server的信息
static parameters_set parameters={0, 0, temp_host_ip, 80, temp_uri}; //存储所有的参数信息
request_data *test_data; //长度为测试总数的数组，存储每次测试的数据
static final_statistics *overall_stat; //最终将会被输出到屏幕上的数据

static const struct option long_options[]={
	{"rate", required_argument, NULL, 'r'},
	{"host", required_argument, NULL, 'h'},
	{"port", required_argument, NULL, 'p'},
	{"time", required_argument, NULL, 't'},
	{"uri", required_argument, NULL, 'u'},
	{NULL,0,NULL,0}
};

char http_request[MAX_REQUEST_SIZE]={0};

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

//连接线程
void *thread_conn(void *para)
{
	int iRet = 0; //用于临时记录connect函数的返回值
	double timedif; //用于记录dbl_time_diff函数的返回值
	struct timeval start_this, start_backup;
	struct timeval current;
	struct timespec sleep_time;
	gettimeofday(&start_this, NULL);
	memcpy(&start_backup, &start_this, sizeof(start_this));
	for(int i=0; i<press_number; i++){	
		/* 新建socket，并connect，然后将socket描述符写入fd[1]管道 */
		req_pos_pair *rpp_conn = new req_pos_pair;
		rpp_conn->position = i;
		
		struct hostent *he;
		struct sockaddr_in server;
		if((he = gethostbyname(host.host_ip)) == NULL)
		{
			printf("gethostbyname() error.\n");
			exit(1);
		}
		
		if((rpp_conn->sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			printf("socket() error.\n");
			exit(1);
		}
		test_data[rpp_conn->position].sockfd = rpp_conn->sockfd;
		
		bzero(&server, sizeof(server));
		server.sin_family = AF_INET;
		server.sin_port = htons(host.port);
		server.sin_addr = *((struct in_addr *)he->h_addr);
		
		//设置socket为非阻塞
		int old_option = set_nonblock_sock(rpp_conn->sockfd);
		
		//设置test_data.init_connect字段
		gettimeofday(&(test_data[rpp_conn->position].init_connect), NULL);
		iRet = connect(rpp_conn->sockfd, (struct sockaddr *)&server, sizeof(server));
		if(iRet == 0)
		{//连接立即成功，需要处理这种情况
			printf("connection successes immediately.\n");
			fcntl(rpp_conn->sockfd, F_SETFL, old_option);
			close(rpp_conn->sockfd);
			exit(1);
		}else if(errno != EINPROGRESS)
		{//errno不是EINPROGRESS意味着未知错误出现
			printf("connect() error.\n");
			exit(1);
		}else //errno == EINPROGRESS and iRet < 0
		{//这种情况是正常现象，将rpp结构体写入管道	
			write(fd[1], (void *)&rpp_conn, sizeof(void *));
		}
		
		gettimeofday(&current, NULL);
		timedif = dbl_time_diff(&current, &start_this);//该任务距离理论上开始执行它的时间有多久，即理论上任务执行时间
		if(press_interval>timedif){ //如果还没到发送下一个的时间，则睡眠一会
			sleep_time.tv_sec = (int)(floor(press_interval-timedif)+0.1) / 1000000;
			sleep_time.tv_nsec = ((int)(floor(press_interval-timedif)+0.1) % 1000000) * 1000;
			nanosleep(&sleep_time, NULL);//线程休眠一会
		}
		
		next_send_time(&start_this, &start_backup, (i+1));		
	}
	
	close(fd[1]);//关闭管道的写端
	
	return NULL;
}

//发送线程
void *thread_send(void *para)
{
	int num, read_count;
	int time_out_pointer = 0;
	timeval temptime; //用于临时记录当前时间
	req_pos_pair *rpp_send;
	int epfd_send = epoll_create(MAX_WAIT_EVENT);
	addfd_to_epoll2(epfd_send, fd[0], EPOLLIN, (void *)&fd[0]);
	int epfd_send_count = 1; //记录epoll中目前监听的文件描述符个数
	struct epoll_event events_send[MAX_WAIT_EVENT];
	int iret = -1; //用于记录epoll_wait的返回值
	int tempfd = -1; //用于临时记录epoll返回的就绪的文件描述符的值
	int position = -1; //用于临时记录epoll返回值的就绪文件在test_data数组中的位置
	
	build_http_request(&parameters); //构造即将要发送的HTTP请求
	while(1){
		/* 这里的判断是说，epfd_send中是不是已经没有描述符了，如果没有了就退出 */
		if(epfd_send_count <= 0){
			close(fd2[1]);
			close(epfd_send);
			break;
		}
		iret = epoll_wait(epfd_send, events_send, MAX_WAIT_EVENT, MAX_EPOLL_WAIT_TIME);
		if(iret > 0){ //有东西读到，需要判断是管道事件还是connect事件
			for(int i=0; i<iret;i++)
			{
				if((void *)&fd[0] == events_send[i].data.ptr){
				 //处理管道可读事件
					read_count = read(fd[0], (void *)&rpp_send, sizeof(void *));
					if(read_count == 0) //管道被关闭,所有请求都已经发出了
					{
						delete_from_epoll(epfd_send, fd[0]);
						epfd_send_count --;
						close(fd[0]);
					}else if(read_count != sizeof(void *)) //读出来的数据不全
					{
						printf("the content read from fd[0] is not intact.\n");
						exit(1);
					}else{ //正常读取，将读到的socket加入监听队列
						addfd_to_epoll2(epfd_send, rpp_send->sockfd, EPOLLIN | EPOLLOUT, (void *)rpp_send);
						epfd_send_count ++;
					}
				}else{
				 //处理socket连接成功事件
					gettimeofday(&temptime,NULL);//获得连接成功的时间
					
					req_pos_pair *temprpp = (req_pos_pair *)events_send[i].data.ptr;
					tempfd = temprpp->sockfd;
					position = temprpp->position;
					if ((events_send[i].events & EPOLLIN) && (events_send[i].events & EPOLLOUT))
					{//既可读又可写，证明出错了或者已经连接并且有来自服务端的数据
						//修改test_data.is_connected，连接失败
						test_data[position].is_connected = 0;
						close(tempfd);
						//exit(1);
					}else if(events_send[i].events & EPOLLOUT)
					{//成功连接,发送请求，然后将描述符从epoll监听队列中删除，并将描述符写入fd2[1]
						//修改连接成功花费的时间，连接成功标志位，发出请求的时间
						test_data[position].connect_time = uint_time_diff(&temptime, &(test_data[position].init_connect));
						test_data[position].is_connected = 1;
						
						gettimeofday(&temptime, NULL);//开始发送请求的时间
						test_data[position].start_send_time = uint_time_diff(&temptime, &(test_data[position].init_connect));
						num = send(tempfd, http_request, strlen(http_request), 0);
						if(num < 0){
							printf("send() error\n");
							exit(1);
						}
						delete_from_epoll(epfd_send, tempfd);
						epfd_send_count --;
						write(fd2[1], (void *)&events_send[i].data.ptr, sizeof(void *));
					}
				}
			}
		}else if(iret == 0){
			//do nothing
		}else // iret<0
		{
			printf("epoll_send failed\n");
			return NULL;
		}
		
		/* 超时检测环节 */
		gettimeofday(&temptime, NULL);
		temptime.tv_sec -= REQUEST_TIME_OUT;
		while(time_out_pointer < press_number && test_data[time_out_pointer].is_connected == 1)
		{
			time_out_pointer++;
		}
		while(time_out_pointer < press_number && test_data[time_out_pointer].init_connect.tv_sec > 0 && 
			time_compare(&(test_data[time_out_pointer].init_connect), &temptime)>=0 && test_data[time_out_pointer].is_connected == 0)
		{
			close(test_data[time_out_pointer].sockfd);
			delete_from_epoll(epfd_send, test_data[time_out_pointer].sockfd);
			epfd_send_count --;
			time_out_pointer ++;
		}
	}

	return NULL;
}

//接收线程
void *thread_recv(void *para)
{
	int num, read_count;
	int time_out_pointer = 0;
	timeval temptime; //用于临时记录时间
	req_pos_pair *rpp_recv;
	int epfd_recv = epoll_create(MAX_WAIT_EVENT);
	addfd_to_epoll2(epfd_recv, fd2[0], EPOLLIN, (void *)&fd2[0]);
	int epfd_recv_count = 1; //记录epoll中目前监听的文件描述符个数
	struct epoll_event events_recv[MAX_WAIT_EVENT];
	char buf[MAX_RECV_SIZE_ONE_TIME]; //用于处理socket可读事件，读取数据
	int iret = -1; //用于记录epoll_wait的返回值
	int tempfd = -1; //用于临时记录epoll返回的就绪的文件描述符的值
	int position = -1; //用于临时记录epoll返回值的就绪文件在test_data数组中的位置
	while(1){
		if(epfd_recv_count <= 0){ //epoll监听队列中已经没有描述符了
			close(epfd_recv); //关闭监听队列，直接退出程序
			break;
		}
		iret = epoll_wait(epfd_recv, events_recv, MAX_WAIT_EVENT, MAX_EPOLL_WAIT_TIME);
		if(iret > 0){
			for(int i=0;i<iret;i++)
			{
				if(((void *)&fd2[0]) == events_recv[i].data.ptr){
				 //处理管道可读事件
					read_count = read(fd2[0], (void *)&rpp_recv, sizeof(void *));
					if(read_count == 0) //读取到0字节，fd2[1]被关闭
					{
						delete_from_epoll(epfd_recv, fd2[0]);
						epfd_recv_count --;
						close(fd2[0]);
					}else if(read_count != sizeof(void *))
					{ //读出来的数据不全
						printf("the content read from fd2[0] is not intact. read_count=%d and sizeof(void *)=%d\n", read_count, sizeof(void *));
						exit(1);
					}else{ //正常读取，将读到的socket加入监听队列
						addfd_to_epoll2(epfd_recv, rpp_recv->sockfd, EPOLLIN, (void *)rpp_recv);
						epfd_recv_count ++;
					}
				}else{
				 //处理socket可读事件
					gettimeofday(&temptime, NULL);//记录下当前时间
					
					req_pos_pair *temprpp = (req_pos_pair *)events_recv[i].data.ptr;
					tempfd = temprpp->sockfd;
					position = temprpp->position;
					
					while(1){
						num = recv(tempfd, buf, MAX_RECV_SIZE_ONE_TIME-1, 0);
						if( num > 0 )
						{//normally read some message
							test_data[position].byte_transfered += num; //记录读取的数据量
						}else if( num == 0)//对端关闭
						{
							close(tempfd);//被动关闭
							break;
						}else{//数据读完，主动关闭
							if (errno == EAGAIN || errno == EWOULDBLOCK){//数据读取完毕windows下EAGAIN为EWOULDBLOCK
								close(tempfd);
								break;
							}else{
								printf("unknown error when recv from server. error number is: %d\n", errno);
								exit(1);
							}
						}
					}
					//gettimeofday(&temptime, NULL);//记录下当前时间
					//记录数据读取完毕的时间
					test_data[position].recv_finish_time = uint_time_diff(&temptime, &(test_data[position].init_connect));
					if(test_data[position].byte_transfered > 0){//如果读到了数据，证明收到回复了
						test_data[position].is_replied = 1;
					}
					
					delete_from_epoll(epfd_recv, tempfd);
					delete temprpp;
					epfd_recv_count --;
				}
			}
		}else if(iret == 0){
			// do nothing
		}else // iret < 0
		{
			printf("epoll_recv failed\n");
			return NULL;
		}
		
		/* 超时检测环节 */
		gettimeofday(&temptime, NULL);
		temptime.tv_sec -= REQUEST_TIME_OUT;
		while(time_out_pointer < press_number && test_data[time_out_pointer].is_replied == 1)
		{
			time_out_pointer++;
		}
		while(time_out_pointer < press_number && test_data[time_out_pointer].init_connect.tv_sec > 0 && 
			time_compare(&(test_data[time_out_pointer].init_connect), &temptime)>=0 && test_data[time_out_pointer].is_replied == 0)
		{
			close(test_data[time_out_pointer].sockfd);
			delete_from_epoll(epfd_recv, test_data[time_out_pointer].sockfd);
			epfd_recv_count --;
			time_out_pointer ++;
		}
	}

	return NULL;
}

//初始化系统参数
void init_parameter(int argc, char *argv[])
{
	int ch = 0;
	opterr = 0;
	unsigned int record = 0;
	
	int options_index = 0;
	while ((ch=getopt_long(argc, argv, "r:h:p:t:u:", long_options, &options_index)) != -1)
	{
		switch( ch )
		{
			case 'r':
				parameters.__press_rate = atoi(optarg);
				record += ( 1 << 0 );
				break;
			case 'h':
				parameters.__host_ip = optarg;
				record += ( 1 << 1 );
				break;
			case 'p':
				parameters.__port = (short)(atoi(optarg));
				record += ( 1 << 2 );
				break;
			case 't':
				parameters.__press_time = atoi(optarg);
				record += ( 1 << 3 );
				break;
			case 'u':
				parameters.__uri = optarg;
				record += ( 1 << 4 );
				break;
			default :
				usage(tool_name);
		}
	}
	
	if ( (record & 0x9) != 0x9 ){
		usage(tool_name);
	}
	
	//将parameters中的内容复制到相应参数中
	memcpy(&press_rate, &(parameters.__press_rate), sizeof(press_rate));
	memcpy(&press_time, &(parameters.__press_time), sizeof(press_time));
	memcpy(&(host.port), &(parameters.__port), sizeof(host.port));

	host.host_ip = parameters.__host_ip;
	host.uri = parameters.__uri;
	
	check_setting();
	
	press_number = press_time * press_rate;
	press_interval = 1000000.0 / press_rate;
	
	return ;
}

//主函数
int main(int argc, char *argv[])
{
	tool_name = argv[0];
	init_parameter(argc, argv);
	
	print_info();
	
	//初始化统计所用的struct
	overall_stat = new final_statistics;
	memset(overall_stat, 0, sizeof(final_statistics));
	//初始化要发送的请求
	memset(http_request, 0 , MAX_REQUEST_SIZE);
	//初始化存储每个请求测试数据的空间
	test_data = (request_data *)calloc(press_number, sizeof(request_data));
	pipe(fd);
	pipe(fd2);
	
	pthread_t conn_id, send_id, recv_id;
	pthread_attr_t joinable_attr;
	
	if(pthread_attr_init(&joinable_attr) != 0)
	{
		fprintf(stderr, "Init thread attribute failed!\n");
		exit(1);
	}
	
	if(pthread_attr_setdetachstate(&joinable_attr, PTHREAD_CREATE_JOINABLE) != 0)
	{
		fprintf(stderr, "Set thread detach state joinable failed!\n");
		exit(1);
	}
	
	fprintf(stdout, "Now the host is under test. Please wait a moment ...\n\n");
	sleep(2);
	
	pthread_create(&conn_id, &joinable_attr, thread_conn, NULL);
	pthread_create(&send_id, &joinable_attr, thread_send, NULL);
	pthread_create(&recv_id, &joinable_attr, thread_recv, NULL);

	pthread_join(conn_id, NULL);
	pthread_join(send_id, NULL);
	pthread_join(recv_id, NULL);
	
	statistics_calculation(overall_stat, test_data, press_number);
	
	print_statistics(overall_stat);
	delete overall_stat;
	free(test_data);
	
	return 0;
}
