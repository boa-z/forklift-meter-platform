#pragma once
int fw_env_open(void);
int fw_env_close(void);
char *fw_getenv(char *key);
