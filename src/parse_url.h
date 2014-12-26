#ifndef __PARSE_URL_H__
#define __PARSE_URL_H__

#include "common_struct.h"

host_info* begin_parse( char *url_string );

void end_parse( host_info *target_url );

void print_url( const host_info *target_url );

#endif //__PARSE_URL_H__
