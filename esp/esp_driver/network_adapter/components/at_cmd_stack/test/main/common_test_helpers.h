#ifndef _COMMON_TEST_HELPERS_H_
#define _COMMON_TEST_HELPERS_H_

#define TEST_LOG(format, ...) printf("%s" format "\n", module, ##__VA_ARGS__) 


extern void test_begin_setup();
extern void test_done_clear_up();

#endif