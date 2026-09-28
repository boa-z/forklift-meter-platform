#ifndef TEST_EEPROM_ULOG_H
#define TEST_EEPROM_ULOG_H
#define LOG_LVL_WARNING 4
void test_eeprom_log(const char *format, ...);
#define LOG_W(...) test_eeprom_log(__VA_ARGS__)
#endif
