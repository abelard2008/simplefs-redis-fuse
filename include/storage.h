#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// 存储层
typedef struct {
    char base_dir[512];
} storage_t;

// 创建存储层
storage_t* storage_new(const char *base_dir);
void storage_free(storage_t *storage);

// 写入数据
ssize_t storage_write(storage_t *storage, uint64_t inode, const void *data, size_t size, off_t offset);

// 读取数据
ssize_t storage_read(storage_t *storage, uint64_t inode, void *buf, size_t size, off_t offset);

// 删除数据文件
int storage_delete(storage_t *storage, uint64_t inode);

// 截断文件
int storage_truncate(storage_t *storage, uint64_t inode, uint64_t size);

// 同步到磁盘
int storage_sync(storage_t *storage, uint64_t inode);

// 获取文件大小
int storage_get_size(storage_t *storage, uint64_t inode, int64_t *size);

#ifdef __cplusplus
}
#endif

#endif
