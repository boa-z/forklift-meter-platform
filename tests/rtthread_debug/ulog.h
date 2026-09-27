#ifndef TEST_ULOG_H
#define TEST_ULOG_H
/* 仅替换原生输出调用以验证等级，不模拟 Logger 或输出队列。 */
void test_native_log(int level, const char *tag, const char *format, ...);
#define ulog_e(tag, ...) test_native_log(3, tag, __VA_ARGS__)
#define ulog_w(tag, ...) test_native_log(2, tag, __VA_ARGS__)
#define ulog_i(tag, ...) test_native_log(1, tag, __VA_ARGS__)
#define ulog_d(tag, ...) test_native_log(0, tag, __VA_ARGS__)
#endif
