#!/bin/bash

# Simple-FS-C 构建脚本（使用 Make）

set -e

echo "========================================="
echo "Simple-FS-C 构建脚本"
echo "========================================="
echo ""

# 检查依赖
echo "1. 检查依赖..."

if ! command -v gcc &> /dev/null; then
    echo "错误: 未找到 gcc"
    echo "  Ubuntu/Debian: sudo apt-get install build-essential"
    echo "  CentOS/RHEL: sudo yum groupinstall 'Development Tools'"
    exit 1
fi
echo "✓ gcc"

if ! pkg-config --exists fuse3; then
    echo "错误: 未找到 fuse3"
    echo "  Ubuntu/Debian: sudo apt-get install libfuse3-dev"
    echo "  CentOS/RHEL: sudo yum install fuse3-devel"
    exit 1
fi
echo "✓ fuse3"

if ! pkg-config --exists hiredis; then
    echo "错误: 未找到 hiredis"
    echo "  Ubuntu/Debian: sudo apt-get install libhiredis-dev"
    echo "  CentOS/RHEL: sudo yum install hiredis-devel"
    exit 1
fi
echo "✓ hiredis"

echo ""

# 编译
echo "2. 编译项目..."
make check-deps
make

echo ""
echo "========================================="
echo "构建成功！"
echo "========================================="
echo ""
echo "可执行文件: ./simplefs-c"
echo ""
echo "使用方法："
echo "  sudo ./simplefs-c --help"
echo ""
echo "运行示例："
echo "  sudo ./simplefs-c \\"
echo "    --redis-addr localhost \\"
echo "    --redis-port 6379 \\"
echo "    --data-dir /data/xfs \\"
echo "    --mountpoint /mnt/simplefs \\"
echo "    -f -d"
