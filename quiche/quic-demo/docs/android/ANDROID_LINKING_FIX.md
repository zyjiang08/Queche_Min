# Android QUIC Client 符号链接问题解决方案

## 问题描述

在Android设备上运行quic-client时遇到以下错误：
```
CANNOT LINK EXECUTABLE "./quic-client-android": cannot locate symbol "quiche_conn_free"
referenced by "/data/local/tmp/libquiche_engine.so"...
```

## 根本原因

libquiche_engine.so依赖libquiche的符号（如quiche_conn_free），但这些符号在.so文件中显示为"未定义"（U = Undefined）：

```bash
$ llvm-nm -D lib/android/arm64-v8a/libquiche_engine.so | grep quiche_conn_free
U quiche_conn_free    # U = Undefined symbol
```

这是因为在构建libquiche_engine.so时，虽然使用了`--whole-archive`，但某些quiche符号仍然没有被正确导出到共享库中。

## ✅ 解决方案 1: 使用静态可执行文件（推荐）

创建一个完全静态链接的可执行文件，不依赖任何动态库。

### 实施步骤：

1. **修改编译脚本生成静态库**

首先需要重新构建，生成合并的静态库libquiche_engine.a。

2. **使用更新的Makefile.android_static**

```makefile
# Makefile.android_static
# 完全静态链接版本

NDK_ROOT = /Users/jiangzhongyang/Library/Android/sdk/ndk/23.2.8568313
TOOLCHAIN = $(NDK_ROOT)/toolchains/llvm/prebuilt/darwin-x86_64
API_LEVEL = 21
TARGET = aarch64-linux-android$(API_LEVEL)

CC = $(TOOLCHAIN)/bin/$(TARGET)-clang
CXX = $(TOOLCHAIN)/bin/$(TARGET)-clang++
AR = $(TOOLCHAIN)/bin/llvm-ar
STRIP = $(TOOLCHAIN)/bin/llvm-strip

SRC_DIR = src
BUILD_DIR = build/android
LIB_DIR = ../../lib/android/arm64-v8a
INCLUDE_DIR = ../../include

# 编译标志
CXXFLAGS = -Wall -Wextra -std=c++11 -Os -fPIE \
           -ffunction-sections -fdata-sections \
           -I./include -I$(INCLUDE_DIR)

# 链接标志 - 完全静态链接
LDFLAGS = -pie \
          -Wl,--gc-sections \
          -static-libstdc++ \
          -Wl,--whole-archive \
          $(LIB_DIR)/libquiche_engine.a \
          -Wl,--no-whole-archive

# 系统库
LIBS = -llog -lm -ldl

CLIENT = quic-client-android-static

all: $(BUILD_DIR) $(CLIENT)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(CLIENT): $(BUILD_DIR)/client.o
	$(CXX) $(LDFLAGS) $^ $(LIBS) -o $@
	@echo "✅ Built static $@"
	@ls -lh $@

$(BUILD_DIR)/client.o: $(SRC_DIR)/client.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR) $(CLIENT)

strip: $(CLIENT)
	$(STRIP) $(CLIENT)
	@echo "✅ Stripped $@"
	@ls -lh $(CLIENT)
```

3. **部署到Android设备**

```bash
# 推送静态可执行文件
adb push quic-client-android-static /data/local/tmp/quic-client
adb shell chmod +x /data/local/tmp/quic-client

# 直接运行（无需LD_LIBRARY_PATH）
adb shell "/data/local/tmp/quic-client 192.168.1.100 4433"
```

### 优势：
- ✅ 无需推送.so文件
- ✅ 无需设置LD_LIBRARY_PATH
- ✅ 部署简单，单个文件
- ✅ 符号全部包含在可执行文件中

### 劣势：
- ❌ 文件较大（预计2-3MB）

---

## 解决方案 2: 修复libquiche_engine.so符号导出

重新构建libquiche_engine.so，确保所有quiche符号正确导出。

### 实施步骤：

修改`quiche/src/build.rs`中的链接参数：

```rust
// 在Android构建时添加导出符号标志
if target.contains("android") {
    cxxflags.push("-fvisibility=default"); // 导出所有符号
}
```

或者修改`quiche_engine_all.sh`中创建.so的命令：

```bash
"$NDK_COMPILER" \
    -shared \
    -o "$SO_FILE" \
    -Wl,--whole-archive \
    -Wl,--export-dynamic \      # 添加这一行
    "$LIBQUICHE_PATH" \
    "$LIBEV_PATH" \
    "$LIBENGINE_PATH" \
    "$LIBCRYPTO_PATH" \
    "$LIBSSL_PATH" \
    -Wl,--no-whole-archive \
    -lc++_shared \
    -llog \
    -lm
```

### 验证符号：

```bash
# 检查符号是否定义（应该看到T或D，而不是U）
llvm-nm -D lib/android/arm64-v8a/libquiche_engine.so | grep quiche_conn_free

# 期望输出:
# 00012340 T quiche_conn_free    # T = Text (code) section, defined
```

---

## 解决方案 3: 创建符号导出列表

创建版本脚本明确导出所有quiche符号。

### version.script:

```
{
  global:
    quiche_*;
    QuicheEngine*;
    ev_*;
  local:
    *;
};
```

### 修改构建命令：

```bash
"$NDK_COMPILER" \
    -shared \
    -o "$SO_FILE" \
    -Wl,--version-script=version.script \
    -Wl,--whole-archive \
    "$LIBQUICHE_PATH" \
    ...
```

---

## 当前最佳实践（临时解决方案）

在修复之前，使用以下方法部署：

### 1. 推送所有必需文件

```bash
adb shell mkdir -p /data/local/tmp/quiche
adb push quic-client-android /data/local/tmp/quiche/
adb push lib/android/arm64-v8a/libquiche_engine.so /data/local/tmp/quiche/
adb push $ANDROID_NDK_HOME/.../libc++_shared.so /data/local/tmp/quiche/
adb shell chmod +x /data/local/tmp/quiche/quic-client-android
```

### 2. 运行时设置库路径

```bash
adb shell "cd /data/local/tmp/quiche && LD_LIBRARY_PATH=. ./quic-client-android <server> <port>"
```

---

## 诊断命令

### 检查符号定义状态

```bash
# U = Undefined (需要其他库提供)
# T = Defined in text section (代码)
# D = Defined in data section (数据)

llvm-nm -D libquiche_engine.so | grep quiche_
```

### 检查依赖库

```bash
llvm-readelf -d quic-client-android | grep NEEDED
```

### 检查RPATH

```bash
llvm-readelf -d quic-client-android | grep -E "RPATH|RUNPATH"
```

---

## 推荐方案

**短期**: 使用临时解决方案（LD_LIBRARY_PATH）

**长期**: 实施解决方案1（完全静态链接），这是Android NDK推荐的做法，可以避免动态库版本兼容性问题。

---

## 下一步行动

1. ✅ 修改构建脚本生成libquiche_engine.a
2. ✅ 创建Makefile.android_static
3. ✅ 编译静态版本
4. ✅ 测试部署

预期结果：
- 单个可执行文件
- 大小约2-3MB
- 无需动态库依赖
- 简化部署流程
