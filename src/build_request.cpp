#include <stdio.h>
#include <string.h>

#include "build_request.h"
#include "common_constant.h"
#include "common_struct.h"
#include "version.h"

extern char http_request[]; //sizeof http_request is MAX_REQUEST_SIZE

void build_http_request( const parameters_set *parameters )
{
	
	int bytes = 0;
	bytes = snprintf( http_request, MAX_REQUEST_SIZE, "GET %s HTTP/1.1\r\n", parameters->__uri );
	bytes += snprintf( &http_request[bytes], MAX_REQUEST_SIZE-bytes, "User-Agent: %s\r\n", VERSION );
	bytes += snprintf( &http_request[bytes], MAX_REQUEST_SIZE-bytes, "Host: %s \r\n", parameters->__host_ip );
	bytes += snprintf( &http_request[bytes], MAX_REQUEST_SIZE-bytes, "Pragma: no-cache\r\n" );
	//bytes += snprintf( &http_request[bytes], MAX_REQUEST_SIZE-bytes, "Accept: */*\r\n" );
	bytes += snprintf( &http_request[bytes], MAX_REQUEST_SIZE-bytes, "Connection: close\r\n\r\n" );
	http_request[bytes] = '\0';
	return ;
	
}
