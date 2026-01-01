#include "redis_meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <hiredis/hiredis.h>

static const char *NODE_KEY_PREFIX = "node:";
static const char *DIR_KEY_PREFIX = "dir:";
static const char *LOOKUP_COUNTER_KEY = "lookup";

static char* get_node_key(uint64_t inode) {
    char *key = (char*)malloc(32);
    if (!key) return NULL;
    sprintf(key, "%s%lu", NODE_KEY_PREFIX, inode);
    return key;
}

static char* get_dir_key(uint64_t inode) {
    char *key = (char*)malloc(32);
    if (!key) return NULL;
    sprintf(key, "%s%lu", DIR_KEY_PREFIX, inode);
    return key;
}

static redisContext* redis_connect(const char *addr, int port) {
    redisContext *c = redisConnect(addr, port);
    if (c == NULL || c->err) {
        if (c) {
            fprintf(stderr, "Redis connection error: %s\n", c->errstr);
            redisFree(c);
        } else {
            fprintf(stderr, "Redis connection error: can't allocate redis context\n");
        }
        return NULL;
    }
    return c;
}

redis_meta_t* redis_meta_new(const char *addr, int port, const char *password, int db) {
    redis_meta_t *meta = (redis_meta_t*)malloc(sizeof(redis_meta_t));
    if (!meta) {
        return NULL;
    }

    meta->ctx = redis_connect(addr, port);
    if (!meta->ctx) {
        free(meta);
        return NULL;
    }

    // 如果有密码，认证
    if (password && strlen(password) > 0) {
        redisReply *reply = (redisReply*)redisCommand(meta->ctx, "AUTH %s", password);
        if (!reply || reply->type == REDIS_REPLY_ERROR) {
            fprintf(stderr, "Redis authentication failed\n");
            if (reply) freeReplyObject(reply);
            redisFree(meta->ctx);
            free(meta);
            return NULL;
        }
        freeReplyObject(reply);
    }

    // 选择数据库
    if (db > 0) {
        redisReply *reply = (redisReply*)redisCommand(meta->ctx, "SELECT %d", db);
        if (reply) freeReplyObject(reply);
    }

    return meta;
}

void redis_meta_free(redis_meta_t *meta) {
    if (meta) {
        if (meta->ctx) {
            redisFree(meta->ctx);
        }
        free(meta);
    }
}

uint64_t redis_meta_allocate_inode(redis_meta_t *meta) {
    redisReply *reply = (redisReply*)redisCommand(meta->ctx, "INCR %s", LOOKUP_COUNTER_KEY);
    if (!reply || reply->type != REDIS_REPLY_INTEGER) {
        if (reply) freeReplyObject(reply);
        return 0;
    }

    uint64_t inode = (uint64_t)reply->integer;
    freeReplyObject(reply);
    return inode;
}

int redis_meta_create_node(redis_meta_t *meta, uint64_t parent, const char *name,
                          uint32_t mode, uint32_t uid, uint32_t gid, node_attr_t **result_attr) {
    uint64_t inode = redis_meta_allocate_inode(meta);
    if (inode == 0) {
        return -1;
    }

    node_attr_t *attr = (node_attr_t*)malloc(sizeof(node_attr_t));
    if (!attr) {
        return -1;
    }

    uint64_t now = (uint64_t)time(NULL);
    attr->inode = inode;
    attr->mode = mode;
    attr->uid = uid;
    attr->gid = gid;
    attr->size = 0;
    attr->blocks = 0;
    attr->atime = now;
    attr->mtime = now;
    attr->ctime = now;
    attr->link_target[0] = '\0';

    // 序列化属性
    char attr_str[1024];
    snprintf(attr_str, sizeof(attr_str),
             "%lu:%u:%u:%u:%lu:%lu:%lu:%lu:%lu",
             attr->inode, attr->mode, attr->uid, attr->gid,
             attr->size, attr->blocks, attr->atime, attr->mtime, attr->ctime);

    // 使用事务
    redisAppendCommand(meta->ctx, "SET %s%lu %s", NODE_KEY_PREFIX, inode, attr_str);
    redisAppendCommand(meta->ctx, "HSET %s%lu %s %lu", DIR_KEY_PREFIX, parent, name, inode);

    redisReply *reply1, *reply2;
    redisGetReply(meta->ctx, (void**)&reply1);
    redisGetReply(meta->ctx, (void**)&reply2);

    int ret = -1;
    if (reply1 && reply1->type != REDIS_REPLY_ERROR &&
        reply2 && reply2->type != REDIS_REPLY_ERROR) {
        ret = 0;
        *result_attr = attr;
    } else {
        free(attr);
    }

    if (reply1) freeReplyObject(reply1);
    if (reply2) freeReplyObject(reply2);

    return ret;
}

int redis_meta_get_node(redis_meta_t *meta, uint64_t inode, node_attr_t **result_attr) {
    redisReply *reply = (redisReply*)redisCommand(meta->ctx, "GET %s%lu", NODE_KEY_PREFIX, inode);
    if (!reply || reply->type != REDIS_REPLY_STRING) {
        if (reply) freeReplyObject(reply);
        return -1;
    }

    node_attr_t *attr = (node_attr_t*)malloc(sizeof(node_attr_t));
    if (!attr) {
        freeReplyObject(reply);
        return -1;
    }

    // 解析属性字符串
    char *str = reply->str;
    sscanf(str, "%lu:%u:%u:%u:%lu:%lu:%lu:%lu:%lu",
           &attr->inode, &attr->mode, &attr->uid, &attr->gid,
           &attr->size, &attr->blocks, &attr->atime, &attr->mtime, &attr->ctime);

    freeReplyObject(reply);
    *result_attr = attr;
    return 0;
}

int redis_meta_update_node(redis_meta_t *meta, const node_attr_t *attr) {
    char attr_str[1024];
    snprintf(attr_str, sizeof(attr_str),
             "%lu:%u:%u:%u:%lu:%lu:%lu:%lu:%lu",
             attr->inode, attr->mode, attr->uid, attr->gid,
             attr->size, attr->blocks, attr->atime, attr->mtime, attr->ctime);

    redisReply *reply = (redisReply*)redisCommand(meta->ctx, "SET %s%lu %s",
                                                  NODE_KEY_PREFIX, attr->inode, attr_str);
    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        if (reply) freeReplyObject(reply);
        return -1;
    }

    freeReplyObject(reply);
    return 0;
}

int redis_meta_lookup(redis_meta_t *meta, uint64_t parent, const char *name, uint64_t *inode) {
    redisReply *reply = (redisReply*)redisCommand(meta->ctx, "HGET %s%lu %s",
                                                  DIR_KEY_PREFIX, parent, name);
    if (!reply || reply->type != REDIS_REPLY_STRING) {
        if (reply) freeReplyObject(reply);
        return -1;
    }

    *inode = (uint64_t)atoll(reply->str);
    freeReplyObject(reply);
    return 0;
}

int redis_meta_readdir(redis_meta_t *meta, uint64_t inode, dir_entry_t **entries, int *count) {
    redisReply *reply = (redisReply*)redisCommand(meta->ctx, "HGETALL %s%lu", DIR_KEY_PREFIX, inode);
    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
        if (reply) freeReplyObject(reply);
        return -1;
    }

    *count = reply->elements / 2;
    dir_entry_t *result = (dir_entry_t*)malloc(sizeof(dir_entry_t) * (*count));
    if (!result) {
        freeReplyObject(reply);
        return -1;
    }

    for (size_t i = 0; i < reply->elements; i += 2) {
        strncpy(result[i/2].name, reply->element[i]->str, sizeof(result[i/2].name) - 1);
        result[i/2].inode = (uint64_t)atoll(reply->element[i+1]->str);

        // 获取 mode
        node_attr_t *attr;
        if (redis_meta_get_node(meta, result[i/2].inode, &attr) == 0) {
            result[i/2].mode = attr->mode;
            node_attr_free(attr);
        }
    }

    freeReplyObject(reply);
    *entries = result;
    return 0;
}

int redis_meta_unlink(redis_meta_t *meta, uint64_t parent, const char *name) {
    redisReply *reply = (redisReply*)redisCommand(meta->ctx, "HDEL %s%lu %s",
                                                  DIR_KEY_PREFIX, parent, name);
    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        if (reply) freeReplyObject(reply);
        return -1;
    }

    freeReplyObject(reply);
    return 0;
}

int redis_meta_delete_node(redis_meta_t *meta, uint64_t inode) {
    redisReply *reply = (redisReply*)redisCommand(meta->ctx, "DEL %s%lu", NODE_KEY_PREFIX, inode);
    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        if (reply) freeReplyObject(reply);
        return -1;
    }

    freeReplyObject(reply);
    return 0;
}

int redis_meta_rename(redis_meta_t *meta, uint64_t old_parent, const char *old_name,
                     uint64_t new_parent, const char *new_name) {
    uint64_t inode;
    if (redis_meta_lookup(meta, old_parent, old_name, &inode) != 0) {
        return -1;
    }

    // 使用事务
    redisAppendCommand(meta->ctx, "HDEL %s%lu %s", DIR_KEY_PREFIX, old_parent, old_name);
    redisAppendCommand(meta->ctx, "HSET %s%lu %s %lu", DIR_KEY_PREFIX, new_parent, new_name, inode);

    redisReply *reply1, *reply2;
    redisGetReply(meta->ctx, (void**)&reply1);
    redisGetReply(meta->ctx, (void**)&reply2);

    int ret = -1;
    if (reply1 && reply1->type != REDIS_REPLY_ERROR &&
        reply2 && reply2->type != REDIS_REPLY_ERROR) {
        ret = 0;
    }

    if (reply1) freeReplyObject(reply1);
    if (reply2) freeReplyObject(reply2);

    return ret;
}

void node_attr_free(node_attr_t *attr) {
    if (attr) {
        free(attr);
    }
}

void dir_entries_free(dir_entry_t *entries, int count) {
    if (entries) {
        free(entries);
    }
}
