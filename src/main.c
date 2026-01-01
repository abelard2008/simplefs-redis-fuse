#define FUSE_USE_VERSION 30
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fuse3/fuse_lowlevel.h>
#include "config.h"
#include "redis_meta.h"
#include "storage.h"
#include "fuse_ops.h"

static volatile int keep_running = 1;

void signal_handler(int sig) {
    (void)sig;
    keep_running = 0;
}

int main(int argc, char *argv[]) {
    config_t config;

    // 解析配置
    int ret = parse_config(argc, argv, &config);
    if (ret < 0) {
        fprintf(stderr, "Failed to parse configuration\n");
        return 1;
    } else if (ret > 0) {
        return 0; // 显示了帮助信息
    }

    printf("Simple-FS-C Configuration:\n");
    printf("  Redis: %s:%d (db: %d)\n", config.redis_addr, config.redis_port, config.redis_db);
    printf("  Data Dir: %s\n", config.data_dir);
    printf("  Mount Point: %s\n", config.mountpoint);

    // 创建挂载点目录
    if (mkdir(config.mountpoint, 0755) != 0 && errno != EEXIST) {
        perror("Failed to create mount point");
        return 1;
    }

    // 初始化 Redis 元数据存储
    redis_meta_t *meta = redis_meta_new(config.redis_addr, config.redis_port,
                                        config.redis_password, config.redis_db);
    if (!meta) {
        fprintf(stderr, "Failed to initialize Redis\n");
        return 1;
    }
    printf("Connected to Redis\n");

    // 初始化存储层
    storage_t *storage = storage_new(config.data_dir);
    if (!storage) {
        fprintf(stderr, "Failed to initialize storage\n");
        redis_meta_free(meta);
        return 1;
    }
    printf("Initialized storage layer\n");

    // 创建根目录（如果不存在）
    node_attr_t *root_attr;
    if (redis_meta_get_node(meta, 1, &root_attr) != 0) {
        printf("Creating root directory...\n");
        redis_meta_create_node(meta, 0, "", 0755 | S_IFDIR, 0, 0, &root_attr);
    }

    // 创建文件系统上下文
    fs_context_t fs_ctx;
    fs_ctx.meta = meta;
    fs_ctx.storage = storage;

    // 设置全局上下文
    fs_set_context(&fs_ctx);

    // 设置 FUSE 操作
    struct fuse_operations operations = {
        .getattr    = fs_getattr,
        .mkdir      = fs_mkdir,
        .unlink     = fs_unlink,
        .rmdir      = fs_rmdir,
        .rename     = fs_rename,
        .chmod      = fs_chmod,
        .chown      = fs_chown,
        .truncate   = fs_truncate,
        .open       = fs_open,
        .read       = fs_read,
        .write      = fs_write,
        .release    = fs_release,
        .statfs     = fs_statfs,
        .flush      = NULL,
        .fsync      = fs_fsync,
        .opendir    = fs_open,
        .readdir    = fs_readdir,
        .releasedir = fs_release,
        .init       = fs_init,
        .destroy    = fs_destroy,
        .access     = fs_access,
        .create     = fs_create,
        .utimens    = fs_utimens,
    };

    // 准备 FUSE 参数
    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);

    // 添加程序名（必需）
    fuse_opt_add_arg(&args, "simplefs-c");

    // 添加挂载点
    fuse_opt_add_arg(&args, config.mountpoint);

    // 添加前台选项
    if (config.foreground) {
        fuse_opt_add_arg(&args, "-f");
    }

    // 添加调试选项
    if (config.debug) {
        fuse_opt_add_arg(&args, "-d");
    }

    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 挂载文件系统
    printf("Mounting filesystem...\n");
    ret = fuse_main(args.argc, args.argv, &operations, NULL);

    // 清理
    printf("\nCleaning up...\n");
    fuse_opt_free_args(&args);
    storage_free(storage);
    redis_meta_free(meta);

    printf("Filesystem unmounted.\n");
    return ret;
}
