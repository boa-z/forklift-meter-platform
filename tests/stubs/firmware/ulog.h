/* Compile-only log declaration preserves argument checking. */
void test_boot_log(const char *, ...);
#define LOG_LVL_INFO 1
#define LOG_E(...) test_boot_log(__VA_ARGS__)
#define LOG_W(...) test_boot_log(__VA_ARGS__)
#define LOG_I(...) test_boot_log(__VA_ARGS__)
