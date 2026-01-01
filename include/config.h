#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// 文件系统配置
typedef struct {
    char redis_addr[256];
    int redis_port;
    int redis_db;
    char redis_password[256];
    char data_dir[512];
    char mountpoint[512];
    int foreground;
    int debug;
} config_t;

// 解析命令行参数
int parse_config(int argc, char *argv[], config_t *config);

#endif
