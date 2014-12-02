#ifndef __OUTPUT_H__
#define __OUTPUT_H__

#include "comm_struct.h"
#include "comm_const.h"

//本工具的用法
void usage(const char* tool_name) __attribute__ ((noreturn));

//打印读取的参数信息
void print_info(void);

//检查参数设置是否合法
void check_setting(void);

//计算统计数据
void statistics_calculation(final_statistics *overall_stat, const request_data test_data[], const int press_number);

//打印测试结果
void print_statistics(final_statistics *overall_stat);
#endif //__OUTPUT_H__
