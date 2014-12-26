#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>

#include "common_constant.h"
#include "common_struct.h"
#include "auxiliary_function.h"
#include "build_request.h"
#include "output.h"
#include "version.h"
#include "parse_url.h"

int fd[2]; //fd用于conn和send之间的通信
int fd2[2]; //fd2用于send和recv之间的通信

static char* tool_name = NULL; //The title of this program

//以下三个参数的关系为 press_rate * press_time = press_number
//命令行参数中只能指定两个，另一个计算得出
int press_rate = 0; //单位 request per second，每秒请求数
int press_time = 0; //单位 秒，压测的时间
int press_number = 0; //单位 request，压测的次数

double press_interval = 1000000.0; //该数值等于press_rate的倒数，单位为 微秒

static full_url_list url_list = { {NULL}, 0 }; //从参数或文件中读取的完整url列表
const struct linger so_linger = { 1, 0 }; //设置描述符快速回收
static parameters_set parameters = { 0, 0, {NULL}, 0 }; //存储所有的参数信息
static test_connect_ontime connect_ontime = {{0,0}, {0,0}}; //用于计算所有connect的准时率
request_data *test_data; //长度为测试总数的数组，存储每次测试的数据
static final_statistics *overall_stat; //最终将会被输出到屏幕上的数据

static const struct option long_options[] = {
	{"rate", required_argument, NULL, 'r'},
	{"time", required_argument, NULL, 't'},
	{"url", required_argument, NULL, 'u'},
	{NULL, 0, NULL, 0}
};

char http_request[MAX_REQUEST_SIZE] = {0};

/* 计算下一个start_this的值(开始发送的值)，存储在第一个参数t中 */
void next_send_time( struct timeval *t, const timeval *start_time, int conn_count )
{
	int how_many_1s = conn_count / press_rate;
	int time_within_1s = (int)( (conn_count % press_rate) * press_interval );
	
	if( start_time->tv_usec + time_within_1s > 1000000 )
	{
		t->tv_sec = start_time->tv_sec + how_many_1s + 1;
		t->tv_usec = start_time->tv_usec + time_within_1s - 1000000;
	}else
	{
		t->tv_sec = start_time->tv_sec + how_many_1s;
		t->tv_usec = start_time->tv_usec + time_within_1s;
	}
}

//返回值int，如果为0，表示sockfd描述符被关闭；如果为1，表示sockfd描述符没有被关闭
int read_socket( int sockfd, char buf[], const int max_buf_len, request_data *reqdata )
{
	int num = 0;
	int iret = -1; //用于设置返回值
	while( 1 ){
		num = recv( sockfd, buf, max_buf_len - 1, 0 );
		if( num > 0 )
		{ //正常接收消息
			reqdata->byte_transfered += num;
		}else if( num == 0 )
		{ //对端关闭，则本端被动关闭
			close( sockfd );
			iret = 0;
			break;
		}else
		{ //数据读完，后面可能还有数据，暂时不关闭
			if ( errno == EAGAIN || errno == EWOULDBLOCK )
			{ //当前没有数据可读的话，linux出现EAGAIN错误，而EWOULDBLOCK是windows下出现的
				//close( sockfd );
				iret = 1;
				break;
			}else
			{
				fprintf( stderr, "ERROR message (from file %s, line %d): recv() error. errno = %d.\n", __FILE__, __LINE__, errno );
				exit( 1 );
			}
		}
	}
	return iret;
}

//连接线程
void *thread_conn( void *para )
{
	int iRet = 0; //用于临时记录connect函数的返回值
	int old_option;
	double timedif; //用于记录dbl_time_diff函数的返回值
	struct timeval start_this, start_backup;
	struct timeval current;
	struct timespec sleep_time;
	gettimeofday( &start_this, NULL );
	memcpy( &start_backup, &start_this, sizeof(start_this) );
	memcpy( &(connect_ontime.test_start), &start_this, sizeof(start_this) ); //用于connect准时率统计
	
	for( int i = 0; i < press_number; i++ )
	{
		/* 新建socket，并connect，然后将socket描述符写入fd[1]管道 */
		struct hostent *he;
		struct sockaddr_in server;
		if( (he = gethostbyname(parameters.host[0]->host_ip)) == NULL )
		{
			fprintf( stderr, "ERROR message (from file %s, line %d): gethostbyname() error.\n", __FILE__, __LINE__ );
			exit( 1 );
		}
		
		if( (test_data[i].sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
		{
			fprintf( stderr, "ERROR message (from file %s, line %d): socket() error. errno = %d.\n", __FILE__, __LINE__, errno );
			exit( 1 );
		}
		
		bzero( &server, sizeof(server) );
		server.sin_family = AF_INET;
		server.sin_port = htons( parameters.host[0]->port );
		server.sin_addr = *( (struct in_addr *)he->h_addr );
		
		//设置socket为非阻塞
		old_option = set_nonblock_sock( test_data[i].sockfd );
		
		//设置描述符快速回收
		if( setsockopt(test_data[i].sockfd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger)) != 0 )
		{
			fprintf( stderr, "ERROR message (from file %s, line %d): setsockopt() error.\n", __FILE__, __LINE__ );
			exit( 1 );
		}
		
		//设置test_data.init_connect字段
		gettimeofday( &(test_data[i].init_connect), NULL );
		iRet = connect( test_data[i].sockfd, (struct sockaddr *)&server, sizeof(server) );
		if( iRet == 0 )
		{ //连接立即成功，需要处理这种情况
			fprintf( stderr, "ERROR message (from file %s, line %d): connect() successes immediately.\n", __FILE__, __LINE__ );
			fcntl( test_data[i].sockfd, F_SETFL, old_option );
			close( test_data[i].sockfd );
			exit( 1 );
		}else if( errno != EINPROGRESS )
		{ //errno不是EINPROGRESS意味着未知错误出现
			fprintf( stderr, "ERROR message (from file %s, line %d): connect() error. errno = %d.\n", __FILE__, __LINE__, errno );
			exit( 1 );
		}else //errno == EINPROGRESS and iRet < 0
		{ //这种情况是正常现象，将rpp结构体写入管道
			request_data *test_data_addr = &( test_data[i] );
			write( fd[1], (void *)&test_data_addr, sizeof(void *) );
		}
		
		gettimeofday( &current, NULL );
		timedif = dbl_time_diff( &current, &start_this );//该任务距离理论上开始执行它的时间有多久，即理论上任务执行时间
		if( press_interval > timedif )
		{ //如果还没到发送下一个的时间，则睡眠一会
			sleep_time.tv_sec = (int)( floor(press_interval - timedif) + 0.1 ) / 1000000;
			sleep_time.tv_nsec = ( (int)(floor(press_interval - timedif) + 0.1) % 1000000 ) * 1000;
			pselect( 0, NULL, NULL, NULL, &sleep_time, NULL); //线程休眠一会
		}
		
		next_send_time( &start_this, &start_backup, (i+1) );
	}
	
	gettimeofday( &(connect_ontime.test_end), NULL ); //得到结束时间
	close( fd[1] );//关闭管道的写端
	
	return NULL;
}

//发送线程
void *thread_send( void *para )
{
	int num, read_count;
	int time_out_pointer = 0;
	timeval temptime; //用于临时记录当前时间
	request_data *temp_req_data;
	int epfd_send = epoll_create( MAX_WAIT_EVENT );
	addfd_to_epoll2( epfd_send, fd[0], EPOLLIN, (void *)&fd[0] );
	int epfd_send_count = 1; //记录epoll中目前监听的文件描述符个数
	struct epoll_event events_send[MAX_WAIT_EVENT];
	int iret = -1; //用于记录epoll_wait的返回值
	int tempfd = -1; //用于临时记录epoll返回的就绪的文件描述符的值
	int position = -1; //用于临时记录epoll返回值的就绪文件在test_data数组中的位置
	
	build_http_request( &parameters ); //构造即将要发送的HTTP请求
	while( 1 ){
		/* 这里的判断是说，epfd_send中是不是已经没有描述符了，如果没有了就退出 */
		if( epfd_send_count <= 0 )
		{
			close( fd2[1] );
			close( epfd_send );
			break;
		}
		iret = epoll_wait( epfd_send, events_send, MAX_WAIT_EVENT, MAX_EPOLL_WAIT_TIME );
		if( iret > 0 )
		{ //有东西读到，需要判断是管道事件还是connect事件
			for( int i = 0; i < iret; i++ )
			{
				if( (void *)&fd[0] == events_send[i].data.ptr )
				{ //处理管道可读事件
					read_count = read( fd[0], (void *)&temp_req_data, sizeof(void *) );
					if( read_count == 0 ) //管道被关闭,所有请求都已经发出了
					{
						delete_from_epoll( epfd_send, fd[0] );
						epfd_send_count--;
						close( fd[0] );
					}else if( read_count != sizeof(void *) )
					{ //读出来的数据不全
						fprintf( stderr, "ERROR message (from file %s, line %d): the bytes read from fd[0] is wrong.\n", __FILE__, __LINE__ );
						exit( 1 );
					}else
					{ //正常读取，将读到的socket加入监听队列
						addfd_to_epoll2( epfd_send, temp_req_data->sockfd, EPOLLIN | EPOLLOUT, (void *)temp_req_data );
						epfd_send_count++;
					}
				}else
				{ //处理socket连接成功事件
					gettimeofday( &temptime, NULL );//获得连接成功的时间
					
					temp_req_data = (request_data *)events_send[i].data.ptr;
					tempfd = temp_req_data->sockfd;
					if( (events_send[i].events & EPOLLERR) || (events_send[i].events & EPOLLHUP) )
					{ //epoll出现错误了
						temp_req_data->is_connected = -1; //-1表示连接立即出错
						delete_from_epoll( epfd_send, tempfd );
						epfd_send_count--;
						close( tempfd );
						temp_req_data->sockfd = -1; //表示socket已经关闭
					}else if ( (events_send[i].events & EPOLLIN) && (events_send[i].events & EPOLLOUT) )
					{ //既可读又可写，证明出错了或者已经连接并且有来自服务端的数据，以上情况都认为连接失败
						temp_req_data->is_connected = -1; //-1表示连接立即出错
						delete_from_epoll( epfd_send, tempfd );
						epfd_send_count--;
						close( tempfd );
						temp_req_data->sockfd = -1; //表示socket已经关闭
					}else if( events_send[i].events & EPOLLOUT )
					{ //成功连接,发送请求，然后将描述符从epoll监听队列中删除，并将描述符写入fd2[1]
						//修改连接成功花费的时间，连接成功标志位，发出请求的时间
						temp_req_data->connect_time = uint_time_diff( &temptime, &(temp_req_data->init_connect) );
						temp_req_data->is_connected = 1;
						
						gettimeofday( &temptime, NULL );//开始发送请求的时间
						temp_req_data->start_send_time = uint_time_diff( &temptime, &(temp_req_data->init_connect) );
						num = send( tempfd, http_request, strlen(http_request), 0 );
						if( num < 0 )
						{
							fprintf( stderr, "ERROR message (from file %s, line %d): send() error. errno = %d.\n", __FILE__, __LINE__, errno );
							exit( 1 );
						}
						delete_from_epoll( epfd_send, tempfd );
						epfd_send_count--;
						write( fd2[1], (void *)&temp_req_data, sizeof(void *) );
					}
				}
			}
		}else if( iret == 0 )
		{
			//do nothing
		}else // iret<0
		{
			fprintf( stderr, "ERROR message (from file %s, line %d): epoll_wait() returns negative value. errno = %d.\n", __FILE__, __LINE__, errno );
			exit( 1 );
		}
		
		/* 超时检测环节 */
		gettimeofday( &temptime, NULL );
		temptime.tv_sec -= REQUEST_TIME_OUT;
		while( time_out_pointer < press_number && (test_data[time_out_pointer].is_connected == 1 || test_data[time_out_pointer].is_connected == -1) )
		{
			time_out_pointer++;
		}
		while( time_out_pointer < press_number && test_data[time_out_pointer].init_connect.tv_sec > 0 && 
			time_compare(&(test_data[time_out_pointer].init_connect), &temptime)>=0 && test_data[time_out_pointer].is_connected == 0 )
		{
			delete_from_epoll( epfd_send, test_data[time_out_pointer].sockfd );
			epfd_send_count--;
			close( test_data[time_out_pointer].sockfd );
			test_data[time_out_pointer].sockfd = -1; //表示socket已经关闭
			time_out_pointer++;
		}
	}

	return NULL;
}

//接收线程
void *thread_recv( void *para )
{
	int read_count;
	int time_out_pointer = 0;
	timeval temptime; //用于临时记录时间
	unsigned int timedif, timedif2;
	request_data *temp_req_data;
	int epfd_recv = epoll_create( MAX_WAIT_EVENT );
	addfd_to_epoll2( epfd_recv, fd2[0], EPOLLIN, (void *)&fd2[0] );
	int epfd_recv_count = 1; //记录epoll中目前监听的文件描述符个数
	struct epoll_event events_recv[MAX_WAIT_EVENT];
	char buf[MAX_RECV_SIZE_ONE_TIME]; //用于处理socket可读事件，读取数据
	int iret = -1; //用于记录epoll_wait的返回值
	int sock_open = -1; //用于记录read_socket的返回值
	int tempfd = -1; //用于临时记录epoll返回的就绪的文件描述符的值
	int position = -1; //用于临时记录epoll返回值的就绪文件在test_data数组中的位置
	while( 1 ){
		if( epfd_recv_count <= 0 )
		{ //epoll监听队列中已经没有描述符了
			close( epfd_recv ); //关闭监听队列，直接退出程序
			break;
		}
		iret = epoll_wait( epfd_recv, events_recv, MAX_WAIT_EVENT, MAX_EPOLL_WAIT_TIME );
		if( iret > 0 )
		{
			gettimeofday( &temptime, NULL );//记录下当前时间
			for( int i = 0; i < iret; i++ )
			{
				if( ((void *)&fd2[0]) == events_recv[i].data.ptr )
				{ //处理管道可读事件
					read_count = read( fd2[0], (void *)&temp_req_data, sizeof(void *) );
					if( read_count == 0 ) //读取到0字节，fd2[1]被关闭
					{
						delete_from_epoll( epfd_recv, fd2[0] );
						epfd_recv_count--;
						close( fd2[0] );
					}else if( read_count != sizeof(void *) )
					{ //读出来的数据不全
						fprintf( stderr, "ERROR message (from file %s, line %d): the bytes read from fd2[0] is wrong.\n", __FILE__, __LINE__ );
						exit( 1 );
					}else
					{ //正常读取，将读到的socket加入监听队列
						addfd_to_epoll2( epfd_recv, temp_req_data->sockfd, EPOLLIN, (void *)temp_req_data );
						epfd_recv_count++;
					}
				}else{
				 //处理socket可读事件
				 	temp_req_data = (request_data *)events_recv[i].data.ptr;
					tempfd = temp_req_data->sockfd;
					
					if( (events_recv[i].events & EPOLLERR) || (events_recv[i].events & EPOLLHUP) )
					{
						temp_req_data->is_replied = -1; //-1表示连接立即出错
						delete_from_epoll( epfd_recv, tempfd );
						epfd_recv_count--;
						close( tempfd );
						temp_req_data->sockfd = -1;
					}else if( events_recv[i].events & EPOLLIN )
					{
						sock_open = read_socket( tempfd, buf, MAX_RECV_SIZE_ONE_TIME, temp_req_data ); //读取socket
						
						//记录数据读取完毕的时间，如果已经记录过，不需要重复记录
						if(temp_req_data->recv_finish_time == 0)
						{
							temp_req_data->recv_finish_time = uint_time_diff( &temptime, &(temp_req_data->init_connect) );
						}
						if( temp_req_data->byte_transfered > 0 )
						{ //如果读到了数据，证明收到回复了
							temp_req_data->is_replied = 1;
						}
						if( sock_open == 0 )
						{ //tempfd已经被关闭
							delete_from_epoll( epfd_recv, tempfd );
							temp_req_data->sockfd = -1; //-1表示该socket已经关闭
							epfd_recv_count--;
						}
					}
				}
			}
		}else if( iret == 0 )
		{
			// do nothing
		}else // iret < 0
		{
			fprintf( stderr, "ERROR message (from file %s, line %d): epoll_wait() returns negative value. errno = %d.\n", __FILE__, __LINE__, errno );
			exit( 1 );
		}
		
		/* 超时检测环节 */
		gettimeofday( &temptime, NULL );
		
		while( time_out_pointer < press_number )
		{
			if( test_data[time_out_pointer].init_connect.tv_sec != 0 )
			{ //该链接已经调用过connect
				timedif = uint_time_diff( &temptime, &(test_data[time_out_pointer].init_connect) );
				timedif2 = uint_time_diff( &temptime, &(test_data[time_out_pointer].init_connect) ) - test_data[time_out_pointer].recv_finish_time;
				
				if( test_data[time_out_pointer].is_replied == -1 || test_data[time_out_pointer].sockfd == -1 )
				{ //已经出错了或者sockfd已经用完被关闭了
					time_out_pointer++;
				}else if( test_data[time_out_pointer].is_replied == 0 && timedif < REQUEST_TIME_OUT * 1000000 )
				{ //没收到过回复，还没超时
					break;
				}else if( test_data[time_out_pointer].is_replied == 1 && timedif < REQUEST_TIME_OUT * 1000000 &&  timedif2 < REQUEST_EXPIRE_TIME )
				{ //收到过回复，还没超时
					break;
				}else if( test_data[time_out_pointer].is_replied == 0 && timedif >= REQUEST_TIME_OUT * 1000000 )
				{ //没收到过回复，超时间了
					delete_from_epoll( epfd_recv, test_data[time_out_pointer].sockfd );
					epfd_recv_count--;
					close( test_data[time_out_pointer].sockfd );
					test_data[time_out_pointer].sockfd = -1; //socket已经关闭
					time_out_pointer++;
				}else if( test_data[time_out_pointer].is_replied == 1 && ( timedif >= REQUEST_TIME_OUT * 1000000 || timedif2 >= REQUEST_EXPIRE_TIME ) )
				{ //收到过回复，超时间了
					delete_from_epoll( epfd_recv, test_data[time_out_pointer].sockfd );
					epfd_recv_count--;
					close( test_data[time_out_pointer].sockfd );
					test_data[time_out_pointer].sockfd = -1; //socket已经关闭
					time_out_pointer++;
				}
			}else
			{
				break;
			}
				
		}
	}

	return NULL;
}

//初始化系统参数
void init_parameter( int argc, char *argv[] )
{
	int ch = 0;
	opterr = 0;
	unsigned int record = 0;
	
	int options_index = 0;
	while ( (ch = getopt_long(argc, argv, "r:h:p:t:u:", long_options, &options_index)) != -1 )
	{
		switch( ch )
		{
			case 'r':
				parameters.__press_rate = atoi( optarg );
				record += ( 1 << 0 );
				break;
			case 'u':
				url_list.full_url[0] = optarg;
				url_list.url_number = 1;
				record += ( 1 << 1 );
				break;
			case 't':
				parameters.__press_time = atoi( optarg );
				record += ( 1 << 2 );
				break;
			default :
				usage( tool_name );
		}
	}
	
	if ( (record & 0x7) != 0x7 ){
		usage( tool_name );
	}
	
	//将parameters中的内容复制到相应参数中
	memcpy( &press_rate, &(parameters.__press_rate), sizeof(press_rate) );
	memcpy( &press_time, &(parameters.__press_time), sizeof(press_time) );
	parameters.url_number = url_list.url_number;
	for( int i=0; i<parameters.url_number; i++ )
	{
		parameters.host[i] = begin_parse( url_list.full_url[i] );
		if( !parameters.host[i] )
		{
			usage( tool_name );
		}
	}
	
	check_setting();
	
	press_number = press_time * press_rate;
	press_interval = 1000000.0 / press_rate;
	
	return ;
}

//主函数
int main( int argc, char *argv[] )
{
	tool_name = argv[0];
	init_parameter( argc, argv );
	
	print_info(&parameters);

	//初始化统计所用的struct
	overall_stat = new final_statistics;
	memset( overall_stat, 0, sizeof(final_statistics) );
	//初始化要发送的请求
	memset( http_request, 0 , MAX_REQUEST_SIZE );
	//初始化存储每个请求测试数据的空间
	test_data = (request_data *)calloc( press_number, sizeof(request_data) );
	pipe( fd );
	pipe( fd2 );
	
	pthread_t conn_id, send_id, recv_id;
	pthread_attr_t joinable_attr;
	
	if( pthread_attr_init(&joinable_attr) != 0 )
	{
		fprintf( stderr, "ERROR message (from file %s, line %d): pthread_attr_init() error. errno = %d.\n", __FILE__, __LINE__, errno );
		exit( 1 );
	}
	
	if( pthread_attr_setdetachstate(&joinable_attr, PTHREAD_CREATE_JOINABLE) != 0 )
	{
		fprintf( stderr, "ERROR message (from file %s, line %d): pthread_attr_setdetachstate() error. errno = %d.\n", __FILE__, __LINE__, errno );
		exit( 1 );
	}
	
	fprintf( stdout, "Now the host is under test. Please wait a moment ...\n\n" );
	sleep( 2 ); //睡眠两秒钟，让用户看清楚上面这句提示
	
	pthread_create( &conn_id, &joinable_attr, thread_conn, NULL );
	pthread_create( &send_id, &joinable_attr, thread_send, NULL );
	pthread_create( &recv_id, &joinable_attr, thread_recv, NULL );

	pthread_join( conn_id, NULL );
	pthread_join( send_id, NULL );
	pthread_join( recv_id, NULL );
	
	check_connect_ontime( press_number, &connect_ontime );
	
	statistics_calculation( overall_stat, test_data, press_number );
	
	print_statistics( overall_stat );
	for( int i=0; i<parameters.url_number; i++ )
	{
		end_parse( parameters.host[i] );
	}
	delete overall_stat;
	free( test_data );
	
	return 0;
}
