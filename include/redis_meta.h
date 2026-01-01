#ifndef REDIS_META_H
#define REDIS_META_H

#include <stdint.h>
#include <time.h>
#include <hiredis/hiredis.h>

#ifdef __cplusplus
extern "C" {
#endif

// 节点属性
typedef struct {
    uint64_t inode;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint64_t blocks;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    char link_target[4096];
} node_attr_t;

// 目录项
typedef struct {
    char name[256];
    uint64_t inode;
    uint32_t mode;
} dir_entry_t;

// Redis 元数据存储
typedef struct {
    redisContext *ctx;
} redis_meta_t;

// 创建 Redis 元数据存储
redis_meta_t* redis_meta_new(const char *addr, int port, const char *password, int db);
void redis_meta_free(redis_meta_t *meta);

// 分配 inode
uint64_t redis_meta_allocate_inode(redis_meta_t *meta);

// 创建节点
int redis_meta_create_node(redis_meta_t *meta, uint64_t parent, const char *name,
                          uint32_t mode, uint32_t uid, uint32_t gid, node_attr_t **attr);

// 获取节点
int redis_meta_get_node(redis_meta_t *meta, uint64_t inode, node_attr_t **attr);

// 更新节点
int redis_meta_update_node(redis_meta_t *meta, const node_attr_t *attr);

// 查找文件
int redis_meta_lookup(redis_meta_t *meta, uint64_t parent, const char *name, uint64_t *inode);

// 读取目录
int redis_meta_readdir(redis_meta_t *meta, uint64_t inode, dir_entry_t **entries, int *count);

// 删除文件/目录
int redis_meta_unlink(redis_meta_t *meta, uint64_t parent, const char *name);

// 删除节点
int redis_meta_delete_node(redis_meta_t *meta, uint64_t inode);

// 重命名
int redis_meta_rename(redis_meta_t *meta, uint64_t old_parent, const char *old_name,
                     uint64_t new_parent, const char *new_name);

// 释放内存
void node_attr_free(node_attr_t *attr);
void dir_entries_free(dir_entry_t *entries, int count);

#ifdef __cplusplus
}
#endif

#endif
