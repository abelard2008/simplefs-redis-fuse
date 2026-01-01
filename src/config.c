#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s [OPTIONS]\n", prog_name);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  --redis-addr ADDR      Redis server address (default: localhost)\n");
    fprintf(stderr, "  --redis-port PORT      Redis server port (default: 6379)\n");
    fprintf(stderr, "  --redis-password PASS  Redis password (default: none)\n");
    fprintf(stderr, "  --redis-db DB          Redis database number (default: 0)\n");
    fprintf(stderr, "  --data-dir DIR         Data storage directory (default: /data/xfs)\n");
    fprintf(stderr, "  --mountpoint PATH      Mount point (required)\n");
    fprintf(stderr, "  -f, --foreground       Run in foreground\n");
    fprintf(stderr, "  -d, --debug            Enable debug logging\n");
    fprintf(stderr, "  -h, --help             Show this help message\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  %s --redis-addr localhost --redis-db 3 --mountpoint /mnt/fs -f\n", prog_name);
    fprintf(stderr, "  %s --redis-addr localhost --mountpoint /mnt/fs -f -d\n", prog_name);
}

int parse_config(int argc, char *argv[], config_t *config) {
    if (!config) {
        return -1;
    }

    // 设置默认值
    strcpy(config->redis_addr, "localhost");
    config->redis_port = 6379;
    config->redis_db = 0;
    config->redis_password[0] = '\0';
    strcpy(config->data_dir, "/data/xfs");
    config->mountpoint[0] = '\0';
    config->foreground = 0;
    config->debug = 0;

    static struct option long_options[] = {
        {"redis-addr", required_argument, 0, 'a'},
        {"redis-port", required_argument, 0, 'p'},
        {"redis-password", required_argument, 0, 'P'},
        {"redis-db", required_argument, 0, 'D'},
        {"data-dir", required_argument, 0, 't'},  // 改用 -t
        {"mountpoint", required_argument, 0, 'm'},
        {"foreground", no_argument, 0, 'f'},
        {"debug", no_argument, 0, 'd'},  // 改用 -d
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "fhd", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'a':
                strncpy(config->redis_addr, optarg, sizeof(config->redis_addr) - 1);
                break;
            case 'p':
                config->redis_port = atoi(optarg);
                break;
            case 'P':
                strncpy(config->redis_password, optarg, sizeof(config->redis_password) - 1);
                break;
            case 'D':
                config->redis_db = atoi(optarg);
                break;
            case 't':
                strncpy(config->data_dir, optarg, sizeof(config->data_dir) - 1);
                break;
            case 'd':
                config->debug = 1;
                break;
            case 'm':
                strncpy(config->mountpoint, optarg, sizeof(config->mountpoint) - 1);
                break;
            case 'f':
                config->foreground = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 1;
            default:
                print_usage(argv[0]);
                return -1;
        }
    }

    // 检查必需参数
    if (strlen(config->mountpoint) == 0) {
        fprintf(stderr, "Error: mountpoint is required\n");
        print_usage(argv[0]);
        return -1;
    }

    return 0;
}
