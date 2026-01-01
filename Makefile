# Simple-FS-C Makefile

# 编译器和选项
CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -Iinclude -I/usr/local/include
LDFLAGS = -lfuse3 -lhiredis -lpthread -L/usr/local/lib

# 目录
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = .

# 源文件
SOURCES = $(SRC_DIR)/main.c \
          $(SRC_DIR)/config.c \
          $(SRC_DIR)/storage.c \
          $(SRC_DIR)/redis_meta.c \
          $(SRC_DIR)/fuse_ops.c

# 目标文件
OBJECTS = $(BUILD_DIR)/main.o \
          $(BUILD_DIR)/config.o \
          $(BUILD_DIR)/storage.o \
          $(BUILD_DIR)/redis_meta.o \
          $(BUILD_DIR)/fuse_ops.o

# 可执行文件
TARGET = $(BIN_DIR)/simplefs-c

# 默认目标
all: $(TARGET)

# 创建构建目录
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# 编译目标文件
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# 链接可执行文件
$(TARGET): $(OBJECTS)
	@echo "Linking $@..."
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	@echo "Build complete: $@"

# 清理
clean:
	@echo "Cleaning..."
	rm -rf $(BUILD_DIR) $(TARGET)
	@echo "Clean complete"

# 安装
install: $(TARGET)
	@echo "Installing $(TARGET) to /usr/local/bin..."
	install -m 755 $(TARGET) /usr/local/bin/
	@echo "Install complete"

# 卸载
uninstall:
	@echo "Uninstalling $(TARGET) from /usr/local/bin..."
	rm -f /usr/local/bin/$(TARGET)
	@echo "Uninstall complete"

# 重新编译
rebuild: clean all

# 检查依赖
check-deps:
	@echo "Checking dependencies..."
	@which gcc > /dev/null || (echo "Error: gcc not found"; exit 1)
	@echo "✓ gcc"
	@pkg-config --exists fuse3 || (echo "Error: fuse3 not found"; exit 1)
	@echo "✓ fuse3"
	@pkg-config --exists hiredis || (echo "Error: hiredis not found"; exit 1)
	@echo "✓ hiredis"
	@echo "All dependencies satisfied"

# 帮助
help:
	@echo "Simple-FS-C Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  all         - Build the project (default)"
	@echo "  clean       - Remove build files"
	@echo "  install     - Install to /usr/local/bin"
	@echo "  uninstall   - Uninstall from /usr/local/bin"
	@echo "  rebuild     - Clean and build"
	@echo "  check-deps  - Check required dependencies"
	@echo "  help        - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  make              # Build"
	@echo "  make clean        # Clean"
	@echo "  make install      # Install"
	@echo "  sudo ./simplefs-c --help"

.PHONY: all clean install uninstall rebuild check-deps help
