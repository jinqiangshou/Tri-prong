#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse_url.h"
#include "common_struct.h"
#include "common_constant.h"

//function: parse url_string and restore information in target_url
//return value: if successful, return url struct pointer; if failed, return NULL
host_info* begin_parse( char *url_string)
{
	if( !url_string || !url_string[0] || strlen(url_string) <= 0 )
	{
		return NULL;
	}
	char *temppointer1 = url_string;
	long int templen;
	
	host_info *target_url = (host_info *)calloc(1, sizeof(host_info));

	/* Protocol: http, https, or ftp */
	if( strncasecmp(url_string, "https://", 8) == 0 )
	{
		target_url->protocol_type = HTTPS;
		temppointer1 += 8;
	}else if( strncasecmp(url_string, "http://", 7 ) == 0 )
	{
		target_url->protocol_type = HTTP;
		temppointer1 += 7;
	}else if( strncasecmp(url_string, "ftp://", 6) == 0 )
	{
		target_url->protocol_type = FTP;
		temppointer1 += 6;
	}else
	{
		free( target_url );
		return NULL;
	}

	/* host ip and port */
	char *temppointer2 = strchr( temppointer1, '/' );
	if ( temppointer2 )
	{
		temppointer2[0] = '\0';
		char *temppointer3 = strchr( temppointer1, ':' );
		if ( temppointer3 == NULL )
		{ //default port is 80
			target_url->port = 80;
			temppointer3 = temppointer2;
		}else
		{
			target_url->port = (unsigned short)atoi( (temppointer3 + 1) );
			if( target_url->port <= 0 || target_url->port >= 65536 )
			{ //legal port range is from 1 to 65535
				free( target_url );
				return NULL;
			}
			temppointer3[0]='\0';
		}

		templen = temppointer3 - temppointer1;
		
		if( templen >= MAX_HOST_IP_LENGTH )
		{
			free( target_url );
			return NULL;
		}
		target_url->host_ip = (char *)calloc(1, templen + 1);
		memcpy( target_url->host_ip, temppointer1, templen );
	}else
	{
		free( target_url->host_ip );
		free( target_url );
		return NULL;
	}

	/* path (or uri) */
	temppointer2[0] = '/';
	templen = strlen( temppointer2 );
	if( templen >= MAX_URI_LENGTH )
	{
		free( target_url->host_ip );
		free( target_url );
		return NULL;
	}
	target_url->uri = (char *)calloc( 1, templen + 1 );
	memcpy( target_url->uri, temppointer2, templen );
	
	return target_url;
}

//function: delete url struct and release memory
void end_parse( host_info *target_url )
{
	if( !target_url )
	{
		return ;
	}
	free( target_url->host_ip );
	free( target_url->uri );
	free( target_url );
	
	return ;
}

//function: print url information
void print_url( const host_info *target_url )
{
	if( !target_url )
	{
		return ;
	}
	
	fprintf( stdout, "Protocol: %d\n", target_url->protocol_type );
	fprintf( stdout, "Host IP: %s\n", target_url->host_ip );
	fprintf( stdout, "Host Port: %d\n", target_url->port );
	fprintf( stdout, "Document Path: %s\n", target_url->uri );
	
	return ;
}
