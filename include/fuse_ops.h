#ifndef FUSE_OPS_H
#define FUSE_OPS_H

#include <fuse3/fuse.h>
#include "redis_meta.h"
#include "storage.h"

#ifdef __cplusplus
extern "C" {
#endif

// 文件系统上下文
typedef struct {
    redis_meta_t *meta;
    storage_t *storage;
} fs_context_t;

// 获取文件属性
int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);

// 访问文件
int fs_access(const char *path, int mask);

// 打开文件
int fs_open(const char *path, struct fuse_file_info *fi);

// 读取文件
int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

// 写入文件
int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

// 释放文件
int fs_release(const char *path, struct fuse_file_info *fi);

// 创建文件
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi);

// 截断文件
int fs_truncate(const char *path, off_t size, struct fuse_file_info *fi);

// 删除文件
int fs_unlink(const char *path);

// 创建目录
int fs_mkdir(const char *path, mode_t mode);

// 删除目录
int fs_rmdir(const char *path);

// 重命名
int fs_rename(const char *oldpath, const char *newpath, unsigned int flags);

// 读取目录
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);

// 同步文件
int fs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi);

// 文件系统统计
int fs_statfs(const char *path, struct statvfs *stbuf);

// 设置文件属性
int fs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi);
int fs_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi);
int fs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi);

// 初始化文件系统
void* fs_init(struct fuse_conn_info *conn, struct fuse_config *cfg);

// 清理文件系统
void fs_destroy(void *private_data);

// 设置全局上下文
void fs_set_context(fs_context_t *ctx);

// 路径解析辅助函数
int parse_path(const char *path, char **parent, char **name);

#ifdef __cplusplus
}
#endif

#endif
