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

static const struct option long_options[]={
	{"rate", required_argument, NULL, 'r'},
	{"host", required_argument, NULL, 'h'},
	{"port", required_argument, NULL, 'p'},
	{"time", required_argument, NULL, 't'},
	{"uri", required_argument, NULL, 'u'},
	{NULL,0,NULL,0}
};

char http_request[MAX_REQUEST_SIZE]={0};

static final_statistics *overall_stat;

//连接线程
void *thread_conn(void *para)
{
	struct timeval start_this, start_backup;
	struct timeval current;
	struct timespec sleep_time;
	gettimeofday(&start_this, NULL);
	memcpy(&start_backup, &start_this, sizeof(start_this));
	for(int i=0; i<press_number; i++){	
		/* 新建socket，并connect，然后将socket描述符写入fd[1]管道 */
		int sockfd, num;
		//char buf[MAX_REQUEST_SIZE];
		struct hostent *he;
		struct sockaddr_in server;
		if((he = gethostbyname(host.host_ip)) == NULL)
		{
			printf("gethostbyname() error.\n");
			exit(1);
		}
		
		if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			printf("socket() error.\n");
			exit(1);
		}
		bzero(&server, sizeof(server));
		server.sin_family = AF_INET;
		server.sin_port = htons(host.port);
		server.sin_addr = *((struct in_addr *)he->h_addr);
		
		//设置socket为非阻塞
		int old_option = set_nonblock_sock(sockfd);
		int iRet = 0;
		iRet = connect(sockfd, (struct sockaddr *)&server, sizeof(server));
		if(iRet == 0)
		{//连接立即成功，需要处理这种情况
			printf("connection successes immediately.\n");
			fcntl(sockfd, F_SETFL, old_option);
			close(sockfd);
		}else if(errno != EINPROGRESS)
		{//errno不是EINPROGRESS意味着未知错误出现
			printf("connect() error.\n");
			exit(1);
		}else //errno == EINPROGRESS and iRet < 0
		{//这种情况是正常现象，将描述符写入管道
			write(fd[1], (void *)&sockfd, sizeof(int *));
		}
		
		gettimeofday(&current, NULL);
		double timedif = diff(&current, &start_this);//该任务距离理论上开始执行它的时间有多久，即理论上任务执行时间
		if(press_interval>timedif){ //如果还没到发送下一个的时间，则睡眠一会
			sleep_time.tv_sec = (int)(floor(press_interval-timedif)+0.1) / 1000000;
			sleep_time.tv_nsec = ((int)(floor(press_interval-timedif)+0.1) % 1000000) * 1000;
			//printf("sleep time tv_sec=%ld, tv_nsec = %ld\n", sleep_time.tv_sec, sleep_time.tv_nsec);
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
	/*FILE *t1=NULL;*/
	int sockfd_send, num;
	int epfd_send = epoll_create(4);
	addfd_to_epoll(epfd_send, fd[0], EPOLLIN);
	int epfd_send_count = 1;
	struct epoll_event events_send[MAX_WAIT_EVENT];
	
	build_http_request(&parameters);
	while(1){
		/* 这里需要加一个判断，epfd_send中是不是已经没有描述符了，如果没有了就退出 */
		if(epfd_send_count <= 0){
			close(fd2[1]);
			close(epfd_send);
			break;
		}
		int iret = epoll_wait(epfd_send, events_send, MAX_WAIT_EVENT, -1);
		if(iret > 0){ //有东西读到，需要判断是管道事件还是connect事件
			for(int i=0; i<iret;i++)
			{
				if(fd[0] == events_send[i].data.fd){
				 //处理管道可读事件
					int read_count = read(fd[0], (void *)&sockfd_send, sizeof(int *));
					if(read_count == 0) //管道被关闭,所有请求都已经发出了
					{
						printf("pipe fd[1] has been closed.\n");
						delete_from_epoll(epfd_send, fd[0]);
						epfd_send_count --;
						close(fd[0]);
					}else if(read_count != sizeof(int *)) //读出来的数据不全
					{
						printf("the content read from fd[0] is not intact.\n");
						exit(1);
					}else{//正常读取，将读到的socket加入监听队列
						addfd_to_epoll(epfd_send, sockfd_send, EPOLLIN | EPOLLOUT);
						epfd_send_count ++;
					}
				}else{
				 //处理socket连接成功事件
					int tempfd = events_send[i].data.fd;
					if ((events_send[i].events & EPOLLIN) && (events_send[i].events & EPOLLOUT))
					{//既可读又可写，证明出错了或者已经连接并且有来自服务端的数据
						printf("error occurs when connect. The errno number is %d.\n", errno);
						close(tempfd);
						exit(1);
					}else if(events_send[i].events & EPOLLOUT)
					{//成功连接,发送请求，然后将描述符从epoll监听队列中删除，并将描述符写入fd2[1]
						num = send(tempfd, http_request, strlen(http_request), 0);
						if(num < 0){
							printf("send() error\n");
							exit(1);
						}
						delete_from_epoll(epfd_send, tempfd);
						epfd_send_count --;
						write(fd2[1], (void *)&tempfd, sizeof(int *));
					}
				}
			}
		}else // iret<=0
		{
			printf("epoll_send failed\n");
			return NULL;
		}
	}
	return NULL;
}

//接收线程
void *thread_recv(void *para)
{
	/*FILE *k=NULL;*/
	int sockfd_recv, num;
	int epfd_recv = epoll_create(4);
	addfd_to_epoll(epfd_recv, fd2[0], EPOLLIN);
	int epfd_recv_count = 1;
	struct epoll_event events_recv[MAX_WAIT_EVENT];
	while(1){
		if(epfd_recv_count <= 0){ //epoll监听队列中已经没有描述符了
			close(epfd_recv);//关闭监听队列，直接退出程序
			break;
		}
		int iret = epoll_wait(epfd_recv, events_recv, MAX_WAIT_EVENT, -1);
		if(iret >0){
			for(int i=0;i<iret;i++)
			{
				if(fd2[0] == events_recv[i].data.fd){
				 //处理管道可读事件
					int read_count = read(fd2[0], (void *)&sockfd_recv, sizeof(int *));
					if(read_count == 0) //读取到0字节，fd2[1]被关闭
					{
						printf("pipe fd2[1] has been closed.\n");
						delete_from_epoll(epfd_recv, fd2[0]);
						epfd_recv_count --;
						close(fd2[0]);
					}else if(read_count != sizeof(int *))
					{//读出来的数据不全
						printf("the content read from fd2[0] is not intact.\n");
						exit(1);
					}else{//正常读取，将读到的socket加入监听队列
						addfd_to_epoll(epfd_recv, (int)sockfd_recv, EPOLLIN);
						epfd_recv_count ++;
					}
				}else{
				 //处理socket可读事件
					int tempfd = events_recv[i].data.fd;
					char buf[MAX_RECV_SIZE_ONE_TIME];
					while(1){
						num = recv(tempfd, buf, MAX_RECV_SIZE_ONE_TIME-1, 0);
						//printf("The number of received message is %d\n", num);
						if( num > 0 )
						{//normally read some message
							overall_stat->total_byte += num;
							//printf("server message: %s\n", buf);
						}else if( num == 0)
						{
							close(tempfd);
							break;
						}else{
							if (errno == EAGAIN || errno == EWOULDBLOCK){//数据读取完毕windows下EAGAIN为EWOULDBLOCK
								close(tempfd);
								break;
							}else{
								printf("unknow error when recv from server. error number is: %d\n", errno);
								exit(1);
							}
						}
					}
					
					delete_from_epoll(epfd_recv, tempfd);
					epfd_recv_count --;
				}
			}
		}else // iret<=1
		{
			printf("epoll_recv failed\n");
			return NULL;
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
	//memcpy(&(host.host_ip),&(parameters.__host_ip), sizeof(host.host_ip));
	//memcpy(&(host.uri), &(parameters.__uri), sizeof(host.uri));
	host.host_ip = parameters.__host_ip;
	host.uri = parameters.__uri;
	
	check_setting();
	
	press_number = press_time * press_rate;
	press_interval = 1000000.0 / press_rate;
	
	printf("press_number = %d\n", press_number);
	printf("press_interval = %f\n", press_interval);
	
	print_info();
	return ;
}

//主函数
int main(int argc, char *argv[])
{
	tool_name = argv[0];
	init_parameter(argc, argv);
	
	//初始化统计所用的struct
	overall_stat = new final_statistics;
	memset(overall_stat, 0, sizeof(final_statistics));
	//初始化要发送的请求
	memset(http_request, 0 , MAX_REQUEST_SIZE);
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
	
	fprintf(stdout, "Now the host is under test. Please wait a moment ...\n");
	sleep(2);
	
	pthread_create(&conn_id, &joinable_attr, thread_conn, NULL);
	pthread_create(&send_id, &joinable_attr, thread_send, NULL);
	pthread_create(&recv_id, &joinable_attr, thread_recv, NULL);

	pthread_join(conn_id, NULL);
	pthread_join(send_id, NULL);
	pthread_join(recv_id, NULL);
	
	print_result(overall_stat);
	delete overall_stat;
	
	//printf("sizeof(FILE *)=%d\n", sizeof(FILE *));
	//printf("From main thread, the result is %d\n", ret);
	return 0;
}
