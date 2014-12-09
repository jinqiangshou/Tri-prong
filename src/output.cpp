#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "common_struct.h"
#include "common_constant.h"
#include "version.h"

extern int press_rate;
extern double press_interval;
extern host_info host;
extern int press_time;

//如何使用本工具
void usage( const char* tool_name )
{
	fprintf( stderr, "usage:  %s\n", tool_name );
	fprintf( stderr, "        -r [or --rate] N                 [REQUIRED] specify the press rate (unit: query per second).\n" ); 
	fprintf( stderr, "        -t [or --time] N                 [REQUIRED] test time (unit: second). \n" );
	fprintf( stderr, "        -h [or --host] XXX.XXX.XXX.XXX   target host ip. The default host ip is 127.0.0.1 .\n" );
	fprintf( stderr, "        -p [or --port] N                 port number. The default port number is 80.\n" );
	fprintf( stderr, "        -u [or --uri ] /index.html       uri of the request.\n" );
	exit( 1 );
}

//打印读取的参数信息
void print_info( void )
{
	fprintf( stdout, "\n" );
	fprintf( stdout, "This is %s (last updated on %s), an HTTP benchmarking tool.\n", VERSION, LAST_UPDATE );
	fprintf( stdout, "Copyright © 2014 %s. Email: %s\n\n", AUTHOR, CONTACT );
	fprintf( stdout, "Test Information\n" );
	fprintf( stdout, "    Host       : %s\n", host.host_ip );
	fprintf( stdout, "    Port       : %d\n", host.port );
	fprintf( stdout, "    URI        : %s\n", host.uri );
	fprintf( stdout, "    Target QPS : %-6d%-11s\n", press_rate, "[#/second]" );
	fprintf( stdout, "    Test Time  : %-6d%-11s\n", press_time, "[second]" );
	fprintf( stdout, "\n" );
	return;
}

void check_setting( void )
{
	if( press_rate <= 0 || press_rate > MAX_RATE )
	{
		fprintf( stderr, "The press rate is illegal. The request rate should be between 1 and %d RPS.\n", MAX_RATE );
		exit( 1 );
	}
	
	if( press_time <= 0 || press_time > MAX_TIME )
	{
		fprintf( stderr, "The press time is too long! The test time should be between 1 and %d seconds.\n", MAX_TIME );
		exit( 1 );
	}
	
	return ;
}

void statistics_calculation( final_statistics *overall_stat, const request_data test_data[], const int press_number )
{
	unsigned int min_connect_time = UINT_MAX;
	unsigned int max_connect_time = 0;
	double avg_connect_time = 0.0;
	
	unsigned int min_response_time = UINT_MAX;
	unsigned int max_response_time = 0;
	double avg_response_time = 0.0;
	
	unsigned int min_request_time = UINT_MAX;
	unsigned int max_request_time = 0;
	double avg_request_time = 0;
	
	int connect_count = 0; //实时统计请求连接的成功数
	int response_count = 0; //实时统计成功收到回复的连接数
	for( int i = 0; i < press_number; i++ )
	{
		if( test_data[i].is_connected == 1 )
		{ //如果该请求连接成功
			connect_count++;
			if( test_data[i].connect_time > max_connect_time )
			{
				max_connect_time = test_data[i].connect_time;
			}
			if ( test_data[i].connect_time < min_connect_time )
			{
				min_connect_time = test_data[i].connect_time;
			}
			avg_connect_time = 1.0 * ( connect_count - 1 ) / connect_count * avg_connect_time + 1.0 / connect_count * test_data[i].connect_time;
		}
		
		if( test_data[i].is_replied == 1 && test_data[i].is_connected == 1 )
		{ //如果该请求连接成功，发送出去后也成功收到回复
			response_count++;
			overall_stat->total_byte += test_data[i].byte_transfered;
			
			int response_time_elapsed = test_data[i].recv_finish_time - test_data[i].start_send_time;
			if( response_time_elapsed > max_response_time )
			{
				max_response_time = response_time_elapsed;
			}
			if( response_time_elapsed < min_response_time )
			{
				min_response_time = response_time_elapsed;
			}
			avg_response_time = 1.0 * ( response_count - 1 ) / response_count * avg_response_time + 1.0 / response_count * response_time_elapsed;
			
			if( test_data[i].recv_finish_time > max_request_time )
			{
				max_request_time = test_data[i].recv_finish_time;
			}
			if( test_data[i].recv_finish_time < min_request_time )
			{
				min_request_time = test_data[i].recv_finish_time;
			}
			avg_request_time = 1.0 * ( response_count - 1 ) / response_count * avg_request_time + 1.0 / response_count * test_data[i].recv_finish_time;
		}
		
		if( test_data[i].is_replied != 1 && test_data[i].is_connected == 1 )
		{
			overall_stat->response_fail_count++;
		}
	}
	
	overall_stat->avg_connect_time = (unsigned int)avg_connect_time;
	overall_stat->min_connect_time = min_connect_time;
	overall_stat->max_connect_time = max_connect_time;
	
	overall_stat->avg_response_time = (unsigned int)avg_response_time;
	overall_stat->min_response_time = min_response_time;
	overall_stat->max_response_time = max_response_time;
	
	overall_stat->avg_request_time = (unsigned int)avg_request_time;
	overall_stat->min_request_time = min_request_time;
	overall_stat->max_request_time = max_request_time;
	
	overall_stat->connect_fail_count = press_number - connect_count;
	
	overall_stat->total_fail_count = overall_stat->response_fail_count + overall_stat->connect_fail_count;
	overall_stat->total_request = press_number;
	overall_stat->fail_percent = 1.0 * overall_stat->total_fail_count / overall_stat->total_request;
}

void print_statistics( final_statistics *overall_stat )
{
	fprintf( stdout, "    TCP Connect Fail Count:   %-9d%-8s\n", overall_stat->connect_fail_count, "[#]" );
	fprintf( stdout, "    HTTP Response Fail Count: %-9d%-8s\n", overall_stat->response_fail_count, "[#]" );
	fprintf( stdout, "    Overall Fail Count:       %-9d%-8s\n", overall_stat->total_fail_count, "[#]" );
	fprintf( stdout, "    Total Request Number:     %-9d%-8s\n", overall_stat->total_request, "[#]" );
	fprintf( stdout, "    Fail Percent:             %.3f%%\n", 100*(overall_stat->fail_percent) );
	fprintf( stdout, "    Total Bytes Transferred:  %-9lu%-8s\n", overall_stat->total_byte, "[Bytes]" );
	fprintf( stdout, "\n" );
	fprintf( stdout, "Request Time [unit: us]\n" );
	fprintf( stdout, "                             min         max         avg\n" );
	fprintf( stdout, "    TCP Connect Time:  %9u%12u%12u\n", overall_stat->min_connect_time, 
		overall_stat->max_connect_time, overall_stat->avg_connect_time );

	fprintf( stdout, "    HTTP Response Time:%9u%12u%12u\n", overall_stat->min_response_time, 
		overall_stat->max_response_time, overall_stat->avg_response_time );

	fprintf( stdout, "    Total Request Time:%9u%12u%12u\n", overall_stat->min_request_time, 
		overall_stat->max_request_time, overall_stat->avg_request_time );
	fprintf( stdout, "\n" );
	
	return ;
}
