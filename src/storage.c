#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

storage_t* storage_new(const char *base_dir) {
    if (!base_dir) {
        return NULL;
    }

    storage_t *storage = (storage_t*)malloc(sizeof(storage_t));
    if (!storage) {
        return NULL;
    }

    strncpy(storage->base_dir, base_dir, sizeof(storage->base_dir) - 1);

    // 创建数据目录
    if (mkdir(base_dir, 0755) != 0 && errno != EEXIST) {
        perror("Failed to create data directory");
        free(storage);
        return NULL;
    }

    return storage;
}

void storage_free(storage_t *storage) {
    if (storage) {
        free(storage);
    }
}

static char* get_data_path(storage_t *storage, uint64_t inode) {
    char *path = (char*)malloc(strlen(storage->base_dir) + 32);
    if (!path) {
        return NULL;
    }
    sprintf(path, "%s/data_%lu", storage->base_dir, inode);
    return path;
}

ssize_t storage_write(storage_t *storage, uint64_t inode, const void *data, size_t size, off_t offset) {
    char *path = get_data_path(storage, inode);
    if (!path) {
        return -1;
    }

    int fd = open(path, O_CREAT | O_WRONLY, 0644);
    free(path);

    if (fd < 0) {
        perror("Failed to open file for writing");
        return -1;
    }

    ssize_t written = pwrite(fd, data, size, offset);
    close(fd);

    return written;
}

ssize_t storage_read(storage_t *storage, uint64_t inode, void *buf, size_t size, off_t offset) {
    char *path = get_data_path(storage, inode);
    if (!path) {
        return -1;
    }

    int fd = open(path, O_RDONLY);
    free(path);

    if (fd < 0) {
        if (errno == ENOENT) {
            // 文件不存在，返回 0
            return 0;
        }
        perror("Failed to open file for reading");
        return -1;
    }

    ssize_t nread = pread(fd, buf, size, offset);
    close(fd);

    return nread;
}

int storage_delete(storage_t *storage, uint64_t inode) {
    char *path = get_data_path(storage, inode);
    if (!path) {
        return -1;
    }

    int ret = unlink(path);
    free(path);

    if (ret != 0 && errno != ENOENT) {
        perror("Failed to delete file");
        return -1;
    }

    return 0;
}

int storage_truncate(storage_t *storage, uint64_t inode, uint64_t size) {
    char *path = get_data_path(storage, inode);
    if (!path) {
        return -1;
    }

    // 如果文件不存在，创建空文件
    if (access(path, F_OK) != 0) {
        int fd = open(path, O_CREAT | O_WRONLY, 0644);
        free(path);
        if (fd < 0) {
            perror("Failed to create file");
            return -1;
        }
        close(fd);
    } else {
        free(path);
    }

    // 重新获取路径（可能在上面被释放）
    path = get_data_path(storage, inode);
    if (!path) {
        return -1;
    }

    int ret = truncate(path, (off_t)size);
    free(path);

    if (ret != 0) {
        perror("Failed to truncate file");
        return -1;
    }

    return 0;
}

int storage_sync(storage_t *storage, uint64_t inode) {
    char *path = get_data_path(storage, inode);
    if (!path) {
        return -1;
    }

    int fd = open(path, O_RDONLY);
    free(path);

    if (fd < 0) {
        if (errno == ENOENT) {
            return 0;
        }
        perror("Failed to open file for sync");
        return -1;
    }

    int ret = fsync(fd);
    close(fd);

    if (ret != 0) {
        perror("Failed to sync file");
        return -1;
    }

    return 0;
}

int storage_get_size(storage_t *storage, uint64_t inode, int64_t *size) {
    char *path = get_data_path(storage, inode);
    if (!path) {
        return -1;
    }

    struct stat st;
    int ret = stat(path, &st);
    free(path);

    if (ret != 0) {
        if (errno == ENOENT) {
            *size = 0;
            return 0;
        }
        perror("Failed to stat file");
        return -1;
    }

    *size = st.st_size;
    return 0;
}
