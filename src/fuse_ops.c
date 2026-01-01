#define FUSE_USE_VERSION 30
#include "fuse_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

// 全局文件系统上下文
static fs_context_t *g_fs_context = NULL;

void fs_set_context(fs_context_t *ctx) {
    g_fs_context = ctx;
}

// 路径解析：将路径分解为父目录 inode 和文件名
// 返回：parent_out=父目录的inode, name_out=最后一个组件名
// 对于 /a/b/c，返回 parent=b的inode, name=c
static int resolve_to_parent_and_name(const char *path, uint64_t *parent_out, char *name_out) {
    if (!path || path[0] != '/') {
        return -EINVAL;
    }

    uint64_t parent = 1; // 根目录 inode

    // 如果是根目录
    if (strcmp(path, "/") == 0) {
        *parent_out = parent;
        name_out[0] = '\0';
        return 0;
    }

    char path_copy[512];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char *saveptr = NULL;
    char *token = strtok_r(path_copy + 1, "/", &saveptr);
    char *prev_token = NULL;

    while (token != NULL) {
        // 保存前一个token
        char *next_token = strtok_r(NULL, "/", &saveptr);

        if (next_token != NULL) {
            // 当前token不是最后一个，需要查找并前进
            uint64_t inode;
            if (redis_meta_lookup(g_fs_context->meta, parent, token, &inode) != 0) {
                return -ENOENT;
            }
            parent = inode;
            prev_token = token;
            token = next_token;
        } else {
            // 最后一个token，不查找，直接作为name_out返回
            break;
        }
    }

    *parent_out = parent;
    if (token) {
        strncpy(name_out, token, 255);
        name_out[255] = '\0';
    } else {
        name_out[0] = '\0';
    }

    return 0;
}

// 简化的路径解析，用于查找已存在的文件
static int resolve_path(const char *path, uint64_t *parent_out, char *name_out) {
    return resolve_to_parent_and_name(path, parent_out, name_out);
}

// 解析父目录路径（用于创建新文件）
static int resolve_parent_path(const char *path, uint64_t *parent_out, char *name_out) {
    return resolve_to_parent_and_name(path, parent_out, name_out);
}

int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi;
    memset(stbuf, 0, sizeof(struct stat));

    fprintf(stderr, "fs_getattr: path=%s\n", path);

    uint64_t parent;
    char name[256];

    if (strcmp(path, "/") == 0) {
        // 根目录
        node_attr_t *attr;
        if (redis_meta_get_node(g_fs_context->meta, 1, &attr) != 0) {
            fprintf(stderr, "fs_getattr: root not found\n");
            return -ENOENT;
        }

        stbuf->st_ino = attr->inode;
        stbuf->st_mode = attr->mode;
        stbuf->st_nlink = 1;
        stbuf->st_uid = attr->uid;
        stbuf->st_gid = attr->gid;
        stbuf->st_size = attr->size;
        stbuf->st_blocks = attr->blocks;
        stbuf->st_atim.tv_sec = attr->atime;
        stbuf->st_mtim.tv_sec = attr->mtime;
        stbuf->st_ctim.tv_sec = attr->ctime;

        node_attr_free(attr);
        return 0;
    }

    int ret = resolve_path(path, &parent, name);
    if (ret != 0) {
        fprintf(stderr, "fs_getattr: resolve_path failed: %d\n", ret);
        return ret;
    }

    fprintf(stderr, "fs_getattr: parent=%lu name=%s\n", parent, name);

    uint64_t inode;
    if (redis_meta_lookup(g_fs_context->meta, parent, name, &inode) != 0) {
        fprintf(stderr, "fs_getattr: lookup failed\n");
        return -ENOENT;
    }

    fprintf(stderr, "fs_getattr: found inode=%lu\n", inode);

    node_attr_t *attr;
    if (redis_meta_get_node(g_fs_context->meta, inode, &attr) != 0) {
        fprintf(stderr, "fs_getattr: get_node failed\n");
        return -ENOENT;
    }

    stbuf->st_ino = attr->inode;
    stbuf->st_mode = attr->mode;
    stbuf->st_nlink = 1;
    stbuf->st_uid = attr->uid;
    stbuf->st_gid = attr->gid;
    stbuf->st_size = attr->size;
    stbuf->st_blocks = attr->blocks;
    stbuf->st_atim.tv_sec = attr->atime;
    stbuf->st_mtim.tv_sec = attr->mtime;
    stbuf->st_ctim.tv_sec = attr->ctime;

    node_attr_free(attr);
    return 0;
}

int fs_access(const char *path, int mask) {
    // 简化实现：总是允许访问
    (void)path;
    (void)mask;
    return 0;
}

int fs_open(const char *path, struct fuse_file_info *fi) {
    // 简化实现：总是允许打开
    (void)path;
    (void)fi;
    return 0;
}

int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)fi;

    uint64_t parent;
    char name[256];

    int ret = resolve_path(path, &parent, name);
    if (ret != 0) {
        return ret;
    }

    uint64_t inode;
    if (redis_meta_lookup(g_fs_context->meta, parent, name, &inode) != 0) {
        return -ENOENT;
    }

    ssize_t nread = storage_read(g_fs_context->storage, inode, buf, size, offset);
    if (nread < 0) {
        return -EIO;
    }

    return (int)nread;
}

int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)fi;

    uint64_t parent;
    char name[256];

    int ret = resolve_path(path, &parent, name);
    if (ret != 0) {
        return ret;
    }

    uint64_t inode;
    if (redis_meta_lookup(g_fs_context->meta, parent, name, &inode) != 0) {
        return -ENOENT;
    }

    ssize_t nwritten = storage_write(g_fs_context->storage, inode, buf, size, offset);
    if (nwritten < 0) {
        return -EIO;
    }

    // 更新文件大小
    node_attr_t *attr;
    if (redis_meta_get_node(g_fs_context->meta, inode, &attr) == 0) {
        attr->size = offset + nwritten;
        attr->mtime = (uint64_t)time(NULL);
        redis_meta_update_node(g_fs_context->meta, attr);
        node_attr_free(attr);
    }

    return (int)nwritten;
}

int fs_release(const char *path, struct fuse_file_info *fi) {
    (void)path;
    (void)fi;
    return 0;
}

int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void)fi;

    fprintf(stderr, "fs_create: path=%s mode=%o\n", path, mode);

    uint64_t parent;
    char name[256];

    // 使用 resolve_parent_path，因为文件可能不存在
    int ret = resolve_parent_path(path, &parent, name);
    if (ret != 0) {
        fprintf(stderr, "fs_create: resolve_parent_path failed: %d\n", ret);
        return ret;
    }

    fprintf(stderr, "fs_create: parent=%lu name=%s\n", parent, name);

    // 创建文件节点
    node_attr_t *attr;
    if (redis_meta_create_node(g_fs_context->meta, parent, name, mode | S_IFREG, 0, 0, &attr) != 0) {
        fprintf(stderr, "fs_create: redis_meta_create_node failed\n");
        return -EIO;
    }

    fprintf(stderr, "fs_create: created inode=%lu\n", attr->inode);
    node_attr_free(attr);
    return 0;
}

int fs_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    (void)fi;

    uint64_t parent;
    char name[256];

    int ret = resolve_path(path, &parent, name);
    if (ret != 0) {
        return ret;
    }

    uint64_t inode;
    if (redis_meta_lookup(g_fs_context->meta, parent, name, &inode) != 0) {
        return -ENOENT;
    }

    if (storage_truncate(g_fs_context->storage, inode, (uint64_t)size) != 0) {
        return -EIO;
    }

    // 更新元数据
    node_attr_t *attr;
    if (redis_meta_get_node(g_fs_context->meta, inode, &attr) == 0) {
        attr->size = (uint64_t)size;
        attr->mtime = (uint64_t)time(NULL);
        redis_meta_update_node(g_fs_context->meta, attr);
        node_attr_free(attr);
    }

    return 0;
}

int fs_unlink(const char *path) {
    uint64_t parent;
    char name[256];

    int ret = resolve_path(path, &parent, name);
    if (ret != 0) {
        return ret;
    }

    uint64_t inode;
    if (redis_meta_lookup(g_fs_context->meta, parent, name, &inode) != 0) {
        return -ENOENT;
    }

    // 从目录删除
    redis_meta_unlink(g_fs_context->meta, parent, name);

    // 删除数据
    storage_delete(g_fs_context->storage, inode);

    // 删除元数据
    redis_meta_delete_node(g_fs_context->meta, inode);

    return 0;
}

int fs_mkdir(const char *path, mode_t mode) {
    uint64_t parent;
    char name[256];

    // 使用 resolve_parent_path，因为目录可能不存在
    int ret = resolve_parent_path(path, &parent, name);
    if (ret != 0) {
        return ret;
    }

    // 创建目录节点
    node_attr_t *attr;
    if (redis_meta_create_node(g_fs_context->meta, parent, name, mode | S_IFDIR, 0, 0, &attr) != 0) {
        return -EIO;
    }

    node_attr_free(attr);
    return 0;
}

int fs_rmdir(const char *path) {
    uint64_t parent;
    char name[256];

    int ret = resolve_path(path, &parent, name);
    if (ret != 0) {
        return ret;
    }

    uint64_t inode;
    if (redis_meta_lookup(g_fs_context->meta, parent, name, &inode) != 0) {
        return -ENOENT;
    }

    // 检查目录是否为空
    dir_entry_t *entries;
    int count;
    if (redis_meta_readdir(g_fs_context->meta, inode, &entries, &count) == 0) {
        if (count > 0) {
            dir_entries_free(entries, count);
            return -ENOTEMPTY;
        }
        dir_entries_free(entries, count);
    }

    // 从父目录删除
    redis_meta_unlink(g_fs_context->meta, parent, name);

    // 删除元数据
    redis_meta_delete_node(g_fs_context->meta, inode);

    return 0;
}

int fs_rename(const char *oldpath, const char *newpath, unsigned int flags) {
    (void)flags;

    uint64_t old_parent, new_parent;
    char old_name[256], new_name[256];

    int ret = resolve_path(oldpath, &old_parent, old_name);
    if (ret != 0) return ret;

    ret = resolve_path(newpath, &new_parent, new_name);
    if (ret != 0) return ret;

    return redis_meta_rename(g_fs_context->meta, old_parent, old_name, new_parent, new_name);
}

int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;

    uint64_t parent;
    char name[256];

    if (strcmp(path, "/") == 0) {
        parent = 1;
    } else {
        int ret = resolve_path(path, &parent, name);
        if (ret != 0) {
            return ret;
        }

        if (redis_meta_lookup(g_fs_context->meta, parent, name, &parent) != 0) {
            return -ENOENT;
        }
    }

    dir_entry_t *entries;
    int count;
    if (redis_meta_readdir(g_fs_context->meta, parent, &entries, &count) != 0) {
        return -EIO;
    }

    // 添加标准目录项
    filler(buf, ".", NULL, 0, (enum fuse_fill_dir_flags)0);
    filler(buf, "..", NULL, 0, (enum fuse_fill_dir_flags)0);

    for (int i = 0; i < count; i++) {
        filler(buf, entries[i].name, NULL, 0, (enum fuse_fill_dir_flags)0);
    }

    dir_entries_free(entries, count);
    return 0;
}

int fs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
    (void)isdatasync;
    (void)fi;

    uint64_t parent;
    char name[256];

    int ret = resolve_path(path, &parent, name);
    if (ret != 0) {
        return ret;
    }

    uint64_t inode;
    if (redis_meta_lookup(g_fs_context->meta, parent, name, &inode) != 0) {
        return -ENOENT;
    }

    if (storage_sync(g_fs_context->storage, inode) != 0) {
        return -EIO;
    }

    return 0;
}

int fs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void)fi;

    uint64_t parent;
    char name[256];

    if (strcmp(path, "/") == 0) {
        node_attr_t *attr;
        if (redis_meta_get_node(g_fs_context->meta, 1, &attr) == 0) {
            attr->mode = mode;
            attr->ctime = (uint64_t)time(NULL);
            redis_meta_update_node(g_fs_context->meta, attr);
            node_attr_free(attr);
        }
        return 0;
    }

    int ret = resolve_path(path, &parent, name);
    if (ret != 0) return ret;

    uint64_t inode;
    if (redis_meta_lookup(g_fs_context->meta, parent, name, &inode) != 0) {
        return -ENOENT;
    }

    node_attr_t *attr;
    if (redis_meta_get_node(g_fs_context->meta, inode, &attr) == 0) {
        attr->mode = mode;
        attr->ctime = (uint64_t)time(NULL);
        redis_meta_update_node(g_fs_context->meta, attr);
        node_attr_free(attr);
    }

    return 0;
}

int fs_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi) {
    (void)fi;

    uint64_t parent;
    char name[256];

    if (strcmp(path, "/") == 0) {
        node_attr_t *attr;
        if (redis_meta_get_node(g_fs_context->meta, 1, &attr) == 0) {
            attr->uid = uid;
            attr->gid = gid;
            attr->ctime = (uint64_t)time(NULL);
            redis_meta_update_node(g_fs_context->meta, attr);
            node_attr_free(attr);
        }
        return 0;
    }

    int ret = resolve_path(path, &parent, name);
    if (ret != 0) return ret;

    uint64_t inode;
    if (redis_meta_lookup(g_fs_context->meta, parent, name, &inode) != 0) {
        return -ENOENT;
    }

    node_attr_t *attr;
    if (redis_meta_get_node(g_fs_context->meta, inode, &attr) == 0) {
        attr->uid = uid;
        attr->gid = gid;
        attr->ctime = (uint64_t)time(NULL);
        redis_meta_update_node(g_fs_context->meta, attr);
        node_attr_free(attr);
    }

    return 0;
}

int fs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
    (void)fi;

    uint64_t parent;
    char name[256];

    if (strcmp(path, "/") == 0) {
        node_attr_t *attr;
        if (redis_meta_get_node(g_fs_context->meta, 1, &attr) == 0) {
            attr->atime = tv[0].tv_sec;
            attr->mtime = tv[1].tv_sec;
            redis_meta_update_node(g_fs_context->meta, attr);
            node_attr_free(attr);
        }
        return 0;
    }

    int ret = resolve_path(path, &parent, name);
    if (ret != 0) return ret;

    uint64_t inode;
    if (redis_meta_lookup(g_fs_context->meta, parent, name, &inode) != 0) {
        return -ENOENT;
    }

    node_attr_t *attr;
    if (redis_meta_get_node(g_fs_context->meta, inode, &attr) == 0) {
        attr->atime = tv[0].tv_sec;
        attr->mtime = tv[1].tv_sec;
        redis_meta_update_node(g_fs_context->meta, attr);
        node_attr_free(attr);
    }

    return 0;
}

int fs_statfs(const char *path, struct statvfs *stbuf) {
    (void)path;
    memset(stbuf, 0, sizeof(struct statvfs));

    // 设置文件系统统计信息
    stbuf->f_bsize = 4096;              // 块大小
    stbuf->f_frsize = 4096;             // 片大小
    stbuf->f_blocks = 256 * 1024 * 1024; // 总块数 (1TB)
    stbuf->f_bfree = 256 * 1024 * 1024;  // 空闲块数
    stbuf->f_bavail = 256 * 1024 * 1024;  // 可用块数（非root用户）
    stbuf->f_files = 1024 * 1024;        // 总 inode 数
    stbuf->f_ffree = 1024 * 1024;        // 空闲 inode 数
    stbuf->f_favail = 1024 * 1024;       // 可用 inode 数（非root用户）
    stbuf->f_namemax = 255;             // 最大文件名长度

    return 0;
}

void* fs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    (void)conn;
    cfg->kernel_cache = 1;
    return NULL;
}

void fs_destroy(void *private_data) {
    (void)private_data;
}
