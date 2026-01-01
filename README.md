# Simple-FS-C - 基于 FUSE 的简单文件系统（C 语言实现）

这是一个基于 FUSE 3.x 的用户空间文件系统实现，参考了 JuiceFS 的架构设计理念，使用 Redis 存储元数据，本地文件系统存储实际数据。

**架构参考**: 本项目参考了 [JuiceFS](https://github.com/juicedata/juicefs) 的架构设计，将元数据存储与数据存储分离，但实现更为简化，主要用于学习和演示 FUSE 文件系统的开发。

## 特性

- **元数据存储**: 使用 Redis 存储文件系统元数据
- **数据存储**: 使用本地文件系统存储实际数据
- **FUSE 接口**: 支持 FUSE 3.x 挂载
- **C 语言实现**: 纯 C 实现，性能优异
- **Make 构建**: 使用标准的 Makefile 构建

## 架构设计

```
用户空间应用
    ↓
FUSE Kernel Module
    ↓
simple-fs-c (用户空间文件系统)
    ├── Redis 层 (元数据存储)
    │   ├── 节点属性 (inode, mode, uid, gid, size, time)
    │   └── 目录结构 (目录 -> 文件列表)
    └── Storage 层 (数据存储)
        └── 本地文件系统 (/data/xfs/)
```

## 设计特点

本项目的核心设计特点包括：

1. **元数据与数据分离**: 参考 JuiceFS 的架构，将文件系统元数据存储在 Redis 中，实际数据存储在本地文件系统
2. **简化的存储模型**: 不像 JuiceFS 那样将数据分块存储到对象存储，而是直接以文件形式存储在本地 XFS 文件系统
3. **纯 C 实现**: 使用标准 C 语言和 POSIX API，便于理解 FUSE 文件系统的工作原理
4. **独立编译**: 完全独立于 JuiceFS 源代码，可作为单独的项目编译和运行

## 依赖

### 编译依赖

- GCC 或 Clang
- Make
- pkg-config

### 运行时依赖

- FUSE 3.x
- libhiredis
- Redis 服务器

### 安装依赖（Ubuntu/Debian）

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    make \
    pkg-config \
    libfuse3-dev \
    libhiredis-dev \
    redis-server
```

### 安装依赖（CentOS/RHEL）

```bash
sudo yum groupinstall -y "Development Tools"
sudo yum install -y \
    make \
    pkgconfig \
    fuse3-devel \
    hiredis-devel \
    redis
```

### 安装依赖（macOS）

```bash
brew install make fuse3 hiredis redis
```

## 编译

### 方法 1: 使用构建脚本（推荐）

```bash
cd /Users/abelard/simple-fs-c
./build.sh
```

### 方法 2: 使用 Make

```bash
cd /Users/abelard/simple-fs-c

# 检查依赖
make check-deps

# 编译
make

# 查看所有可用命令
make help
```

### 编译输出

成功编译后会生成 `simplefs-c` 可执行文件。

### 其他 Make 命令

```bash
make clean       # 清理编译文件
make rebuild     # 重新编译
make install     # 安装到 /usr/local/bin
make uninstall   # 从系统中卸载
```

## 运行

### 1. 启动 Redis

```bash
# 使用 systemd (Linux)
sudo systemctl start redis

# 或手动启动
redis-server

# 使用 Docker
docker run -d -p 6379:6379 redis:latest
```

### 2. 准备数据目录

```bash
# 创建数据存储目录
sudo mkdir -p /data/xfs
sudo chmod 755 /data/xfs

# 创建挂载点
sudo mkdir -p /mnt/simplefs
sudo chmod 755 /mnt/simplefs
```

### 3. 挂载文件系统

```bash
# 基本用法
sudo ./simplefs-c \
    --redis-addr localhost \
    --redis-port 6379 \
    --redis-db 0 \
    --data-dir /data/xfs \
    --mountpoint /mnt/simplefs \
    -f -d

# 参数说明：
# --redis-addr: Redis 服务器地址
# --redis-port: Redis 端口
# --redis-password: Redis 密码（可选）
# --redis-db: Redis 数据库编号
# --data-dir: 数据存储目录
# --mountpoint: 挂载点（必需）
# -f, --foreground: 在前台运行
# -d, --debug: 启用调试日志
# -h, --help: 显示帮助信息
```

### 4. 使用文件系统

```bash
# 在另一个终端中操作
cd /mnt/simplefs

# 创建目录
mkdir test_dir

# 创建文件
echo "Hello, Simple-FS-C!" > test_dir/hello.txt

# 读取文件
cat test_dir/hello.txt

# 查看文件信息
ls -lh test_dir/
stat test_dir/hello.txt

# 删除文件
rm test_dir/hello.txt

# 删除目录
rmdir test_dir
```

### 5. 卸载文件系统

```bash
# 方法 1: 使用 umount 命令
sudo umount /mnt/simplefs

# 方法 2: 在运行 simplefs-c 的终端按 Ctrl+C
```

## 项目结构

```
simple-fs-c/
├── include/
│   ├── config.h       # 配置管理
│   ├── redis_meta.h   # Redis 元数据接口
│   ├── storage.h      # 存储层接口
│   └── fuse_ops.h     # FUSE 操作接口
├── src/
│   ├── main.c         # 主程序
│   ├── config.c       # 配置实现
│   ├── storage.c      # 存储层实现
│   ├── redis_meta.c   # Redis 客户端实现
│   └── fuse_ops.c     # FUSE 操作实现
├── Makefile           # Make 构建配置
├── build.sh           # 快速构建脚本
├── README.md          # 本文档
└── .gitignore         # Git 忽略规则
```

## 核心实现说明

### 1. Redis 元数据存储

**数据结构** ([include/redis_meta.h](include/redis_meta.h)):

```c
typedef struct {
    uint64_t inode;      // 节点 ID
    uint32_t mode;       // 权限和类型
    uint32_t uid;        // 用户 ID
    uint32_t gid;        // 组 ID
    uint64_t size;       // 文件大小
    uint64_t blocks;     // 块数
    uint64_t atime;      // 访问时间
    uint64_t mtime;      // 修改时间
    uint64_t ctime;      // 创建时间
} node_attr_t;
```

**Redis 键结构**:
- `node:$inode` - 节点属性（字符串，格式：`inode:mode:uid:gid:size:blocks:atime:mtime:ctime`）
- `dir:$inode` - 目录内容（Hash，name -> inode）
- `lookup` - inode 分配计数器

**主要操作**:
- `redis_meta_create_node()` - 创建新节点
- `redis_meta_get_node()` - 获取节点属性
- `redis_meta_lookup()` - 查找文件
- `redis_meta_readdir()` - 读取目录
- `redis_meta_unlink()` - 删除文件/目录

### 2. 本地存储层

**实现** ([src/storage.c](src/storage.c)):

- 文件路径: `/data/xfs/data_$inode`
- 使用标准 POSIX 文件操作
- 支持随机读写

**主要操作**:
- `storage_write()` - 写入数据（使用 pwrite）
- `storage_read()` - 读取数据（使用 pread）
- `storage_truncate()` - 截断文件
- `storage_delete()` - 删除文件
- `storage_sync()` - 同步到磁盘

### 3. FUSE 操作

**实现** ([src/fuse_ops.c](src/fuse_ops.c)):

实现的 FUSE 操作：

- `fs_getattr()` - 获取文件属性
- `fs_mkdir()` - 创建目录
- `fs_rmdir()` - 删除目录
- `fs_unlink()` - 删除文件
- `fs_rename()` - 重命名
- `fs_create()` - 创建文件
- `fs_open()` - 打开文件
- `fs_read()` - 读取文件
- `fs_write()` - 写入文件
- `fs_truncate()` - 截断文件
- `fs_chmod()` - 修改权限
- `fs_chown()` - 修改所有者
- `fs_utimens()` - 修改时间戳
- `fs_readdir()` - 读取目录
- `fs_fsync()` - 同步文件

## Makefile 说明

### 主要目标

- `all` (默认) - 编译项目
- `clean` - 清理编译文件
- `rebuild` - 清理并重新编译
- `install` - 安装到系统
- `uninstall` - 从系统卸载
- `check-deps` - 检查依赖
- `help` - 显示帮助信息

### 编译流程

1. 编译每个 `.c` 文件为 `.o` 目标文件
2. 链接所有目标文件为可执行文件
3. 输出 `simplefs-c` 可执行文件

### 自定义编译

```bash
# 使用不同的编译器
make CC=clang

# 添加额外的编译选项
make CFLAGS="-Wall -O3"

# 添加额外的链接选项
make LDFLAGS="-lpthread -lm"
```

## 调试

启用调试模式查看详细的 FUSE 操作：

```bash
sudo ./simplefs-c \
    --redis-addr localhost \
    --data-dir /data/xfs \
    --mountpoint /mnt/simplefs \
    -f -d
```

FUSE 会输出所有操作到标准错误。

## 使用示例

### 示例 1: 基本文件操作

```bash
# 挂载文件系统
sudo ./simplefs-c \
    --redis-addr localhost \
    --data-dir /data/xfs \
    --mountpoint /mnt/simplefs \
    -f

# 在另一个终端
cd /mnt/simplefs

# 写入测试
echo "Hello World" > test.txt
cat test.txt

# 查看信息
stat test.txt
```

### 示例 2: 查看 Redis 数据

```bash
# 连接到 Redis
redis-cli

# 查看所有键
KEYS *

# 查看根目录内容
HGETALL dir:1

# 查看某个文件的属性
GET node:2
```

### 示例 3: 性能测试

```bash
# 使用 dd 测试写入性能
dd if=/dev/zero of=/mnt/simplefs/testfile bs=1M count=100

# 使用 dd 测试读取性能
dd if=/mnt/simplefs/testfile of=/dev/null bs=1M

# 清理
rm /mnt/simplefs/testfile
```

## 故障排除

### 问题: 编译错误 - 找不到 fuse3/fuse.h

```bash
# 检查 fuse3 是否安装
pkg-config --modversion fuse3

# 安装 FUSE 3 开发包
sudo apt-get install libfuse3-dev  # Ubuntu/Debian
sudo yum install fuse3-devel       # CentOS/RHEL
```

### 问题: 链接错误 - 找不到 -lfuse3

```bash
# 检查库路径
pkg-config --libs fuse3

# 更新动态链接库缓存
sudo ldconfig
```

### 问题: 运行时错误 - 找不到 libfuse3.so

```bash
# 查找库文件
ldconfig -p | grep fuse3

# 添加库路径（如果需要）
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/fuse3.conf
sudo ldconfig
```

### 问题: 挂载失败 - Permission denied

```bash
# 使用 sudo 运行
sudo ./simplefs-c ...
```

### 问题: Redis 连接失败

```bash
# 检查 Redis 是否运行
redis-cli ping

# 检查端口
netstat -an | grep 6379

# 查看 Redis 日志
tail -f /var/log/redis/redis-server.log
```

## 性能优化建议

1. **启用 Redis 持久化**: 根据需求选择 RDB 或 AOF
2. **调整缓存**: 修改 FUSE 内核缓存参数
3. **批量操作**: 使用 Redis Pipeline 批量执行命令
4. **编译优化**: 使用 `make CFLAGS="-O3"` 编译

## 安全考虑

1. **权限控制**: 实现完整的 POSIX 权限检查
2. **认证**: Redis 使用密码认证
3. **TLS**: Redis 使用 TLS 连接
4. **输入验证**: 检查所有用户输入

## 已知限制

1. **不支持符号链接**: 未实现符号链接功能
2. **不支持硬链接**: 每个文件只有一个引用
3. **不支持扩展属性**: 未实现 xattr 操作
4. **不支持文件锁**: 未实现 flock/posix lock
5. **错误处理简化**: 部分错误情况未完全处理

## 扩展建议

如果想进一步完善：

1. **添加文件锁**: 实现 `flock` 和 `posix_lock` 操作
2. **实现缓存**: 添加元数据和数据缓存
3. **支持符号链接**: 实现 `symlink` 和 `readlink`
4. **添加属性**: 实现 `getxattr`/`setxattr`/`listxattr`
5. **权限控制**: 实现完整的 POSIX ACL
6. **快照功能**: 基于 Redis 实现文件系统快照

## 参考资料

- [JuiceFS](https://github.com/juicedata/juicefs) - 架构设计参考
- [FUSE 文档](https://libfuse.github.io/doxygen/)
- [hiredis 文档](https://github.com/redis/hiredis)
- [Redis 命令参考](https://redis.io/commands)
- [GNU Make 手册](https://www.gnu.org/software/make/manual/)


## 演示
···
[root@xfile-123 simple-fs-c]# ./simplefs-c --redis-addr localhost --redis-port 6379 --redis-db 3 --data-dir /data/xfs --mountpoint /opt/simplefs -f -d
Simple-FS-C Configuration:
  Redis: localhost:6379 (db: 3)
  Data Dir: /data/xfs
  Mount Point: /opt/simplefs
Connected to Redis
Initialized storage layer
Mounting filesystem...
FUSE library version: 3.10.5
nullpath_ok: 0
unique: 2, opcode: INIT (26), nodeid: 0, insize: 56, pid: 0
INIT: 7.33
flags=0x13fffffb
max_readahead=0x00020000
   INIT: 7.31
   flags=0x0040f039
   max_readahead=0x00020000
   max_write=0x00100000
   max_background=0
   congestion_threshold=0
   time_gran=1
   unique: 2, success, outsize: 80
···
## 许可证

MIT License

## 作者

基于 Simple-FS 架构的 C 语言实现版本

## 致谢

- FUSE 开发团队
- hiredis 开发团队
- Redis 开发团队
