#ifndef HMI_SHA_CONFIG_H
#define HMI_SHA_CONFIG_H
/* 只采用 SDK 已带的 Mbed TLS 软件 SHA256，不引入密钥或认证声明。 */
#define MBEDTLS_CONFIG_FILE "platform/rtthread/hmi_sha_config.h"
#define MBEDTLS_SHA256_C
#endif
