# Android Crash 调试完整指南

**适用场景**: Android NDK应用崩溃调试
**目标平台**: Android arm64-v8a (API 21+)
**工具**: Android NDK 23.2.8568313

---

## 📋 目录

1. [问题现象](#问题现象)
2. [调试方法](#调试方法)
3. [完整调试流程](#完整调试流程)
4. [工具使用详解](#工具使用详解)
5. [常见问题](#常见问题)
6. [快速参考](#快速参考)

---

## 🔍 问题现象

### 典型崩溃输出

```bash
$ adb shell "cd /data/local/tmp/quiche && ./quic-client 192.168.1.4 1234"
Segmentation fault
```

**问题**: 程序崩溃，但不知道具体原因和位置。

---

## 🛠️ 调试方法

### 方法1: 使用 logcat（实时日志）✅ 推荐

**适用**: 大多数场景，无需root权限

#### 优点
- ✅ 无需root权限
- ✅ 实时监控
- ✅ 获取完整堆栈信息
- ✅ 可以看到所有系统日志

#### 缺点
- ❌ 日志较多，需要过滤
- ❌ 需要手动解析地址

#### 使用步骤

**步骤1: 清空现有日志**
```bash
adb logcat -c
```

**步骤2: 实时监控崩溃日志（单独终端）**
```bash
adb logcat | grep -E "DEBUG|FATAL|SIGSEGV|signal" --color=always
```

**步骤3: 运行程序**
```bash
# 在另一个终端运行
adb shell "cd /data/local/tmp/quiche && ./quic-client <host> <port>"
```

**步骤4: 获取完整崩溃日志**
```bash
# 保存完整日志
adb logcat -d > /tmp/crash_log.txt

# 提取crash相关内容
grep -A 50 "*** *** ***" /tmp/crash_log.txt
```

#### 示例输出

```
F DEBUG   : *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***
F DEBUG   : Build fingerprint: 'Android/sdk_phone64_arm64/generic_arm64:...'
F DEBUG   : Revision: '0'
F DEBUG   : ABI: 'arm64'
F DEBUG   : Timestamp: 2025-11-08 15:30:45.123456789+0800
F DEBUG   : Process uptime: 0s
F DEBUG   : Cmdline: ./quic-client
F DEBUG   : pid: 12345, tid: 12346, name: quic-client  >>> ./quic-client <<<
F DEBUG   : uid: 2000
F DEBUG   : signal 11 (SIGSEGV), code 2 (SEGV_ACCERR), fault addr 0x7d8e846000
F DEBUG   : Cause: [address 0x7d8e846000 is in a read-only mapping]
F DEBUG   :
F DEBUG   :     x0  0000007d8e846000  x1  0000000000000000  x2  0000000000000001  x3  0000000000000000
F DEBUG   :     x4  0000000000000000  x5  0000007d8e846000  x6  0000000000000001  x7  0000000000000000
F DEBUG   :     ...
F DEBUG   :
F DEBUG   : backtrace:
F DEBUG   :       #00 pc 00000000000a1234  /system/lib64/libc++_shared.so (std::time_get::do_get+124)
F DEBUG   :       #01 pc 00000000000a5678  /system/lib64/libc++_shared.so (std::ostream::operator<<+456)
F DEBUG   :       #02 pc 000000000001abcd  /data/local/tmp/quiche/quic-client (main+789)
F DEBUG   :       #03 pc 00000000000b9876  /apex/com.android.runtime/lib64/bionic/libc.so (__libc_init+112)
```

---

### 方法2: 使用 Tombstone 文件

**适用**: 需要详细分析，有root权限

#### 优点
- ✅ 包含最完整的崩溃信息
- ✅ 永久保存，可后续分析
- ✅ 包含完整寄存器状态

#### 缺点
- ❌ 需要root权限或shell权限
- ❌ 可能被系统清理

#### 使用步骤

**步骤1: 运行程序触发崩溃**
```bash
adb shell "cd /data/local/tmp/quiche && ./quic-client <host> <port>"
```

**步骤2: 查找最新tombstone**
```bash
adb shell "ls -lt /data/tombstones/ | head -10"
```

输出示例:
```
-rw------- 1 system system 123456 2025-11-08 15:30 tombstone_00
-rw------- 1 system system 234567 2025-11-07 10:20 tombstone_01
```

**步骤3: 拉取tombstone文件**
```bash
# 如果有权限
adb pull /data/tombstones/tombstone_00 /tmp/

# 如果需要root
adb shell "su -c 'cp /data/tombstones/tombstone_00 /sdcard/'"
adb pull /sdcard/tombstone_00 /tmp/
```

**常见问题**: Permission denied
```bash
adb: error: failed to stat remote object '/data/tombstones/tombstone_00': Permission denied
```

**解决方案**: 使用logcat方法或通过su提权

---

### 方法3: 地址解析（llvm-addr2line）✅ 最终解决方案

**适用**: 当tombstone不可用时，通过logcat地址手动解析

#### 关键要求
- ✅ 必须有debug符号（-g编译）
- ✅ 使用NDK的llvm-addr2line（不是系统addr2line）
- ✅ 二进制文件未被strip

---

## 🔬 完整调试流程

### 阶段1: 重现崩溃并获取日志

#### 1.1 清空日志
```bash
adb logcat -c
```

#### 1.2 启动日志监控（可选，方便实时查看）
```bash
# 在单独终端运行
adb logcat | grep -E "DEBUG|FATAL" --color=always
```

#### 1.3 运行崩溃程序
```bash
adb shell "cd /data/local/tmp/quiche && ./quic-client 192.168.1.4 1234"
```

#### 1.4 保存崩溃日志
```bash
adb logcat -d > crash_$(date +%Y%m%d_%H%M%S).txt
```

---

### 阶段2: 分析崩溃信息

#### 2.1 查看崩溃信号
```bash
grep "signal" crash_*.txt
```

输出:
```
F DEBUG   : signal 11 (SIGSEGV), code 2 (SEGV_ACCERR), fault addr 0x7d8e846000
```

**常见信号**:
- `SIGSEGV` (11): 段错误，访问无效内存
- `SIGABRT` (6): 程序主动终止（如assert失败）
- `SIGILL` (4): 非法指令
- `SIGBUS` (7): 总线错误，内存对齐问题

**SIGSEGV代码**:
- `SEGV_MAPERR` (1): 地址未映射
- `SEGV_ACCERR` (2): 权限错误（如写只读内存）

#### 2.2 提取堆栈地址
```bash
grep "backtrace:" -A 20 crash_*.txt
```

输出:
```
backtrace:
      #00 pc 00000000000a1234  /system/lib64/libc++_shared.so
      #01 pc 00000000000a5678  /system/lib64/libc++_shared.so
      #02 pc 000000000001abcd  /data/local/tmp/quiche/quic-client
      #03 pc 00000000000b9876  /apex/com.android.runtime/lib64/bionic/libc.so
```

**重点关注**: 你自己程序的地址（quic-client那一行）

---

### 阶段3: 编译带符号的版本

如果当前版本无符号（stripped），需要重新编译。

#### 3.1 修改编译选项

**文件**: `Makefile.android`

**修改前** (生产版本):
```makefile
CXXFLAGS = -Wall -Wextra -std=c++11 -Os -fPIE \
           -ffunction-sections -fdata-sections \
           -I./include -I$(INCLUDE_DIR)
```

**修改后** (调试版本):
```makefile
# -O0: 禁用优化，便于调试
# -g: 包含debug符号
CXXFLAGS = -Wall -Wextra -std=c++11 -O0 -g -fPIE \
           -ffunction-sections -fdata-sections \
           -I./include -I$(INCLUDE_DIR)
```

#### 3.2 重新编译
```bash
cd quiche/quic-demo
make -f Makefile.android clean
make -f Makefile.android all
```

#### 3.3 重新部署
```bash
adb push quic-client-android /data/local/tmp/quiche/quic-client
```

#### 3.4 重新运行获取新地址
```bash
adb logcat -c
adb shell "cd /data/local/tmp/quiche && ./quic-client 192.168.1.4 1234"
adb logcat -d > crash_with_symbols.txt
```

---

### 阶段4: 地址解析

#### 4.1 设置NDK工具路径
```bash
export ANDROID_NDK_HOME=/Users/jiangzhongyang/Library/Android/sdk/ndk/23.2.8568313
export ADDR2LINE=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-addr2line
```

**注意**: Linux系统路径为 `linux-x86_64`

#### 4.2 解析单个地址
```bash
# 从logcat提取的地址: pc 000000000001abcd
$ADDR2LINE -e quic-client-android -f -C 0x1abcd
```

**参数说明**:
- `-e <binary>`: 指定二进制文件
- `-f`: 显示函数名
- `-C`: C++ name demangling（符号还原）
- `0x1abcd`: 地址（必须加0x前缀）

**成功输出**:
```
main
/path/to/client.cpp:230
```

**失败输出** (无符号):
```
??
??:0
```

#### 4.3 批量解析多个地址
```bash
# 从crash日志提取所有地址
grep "pc.*quic-client" crash_with_symbols.txt | awk '{print $3}' > addresses.txt

# 批量解析
while read addr; do
    echo "=== Address: $addr ==="
    $ADDR2LINE -e quic-client-android -f -C 0x$addr
    echo ""
done < addresses.txt
```

---

### 阶段5: 分析堆栈

#### 示例堆栈解析结果

```bash
# 地址 #00: 0xa1234
std::time_get<char, std::istreambuf_iterator<char>>::__get_day_year_num() const
/path/to/ndk/sources/cxx-stl/llvm-libc++/src/locale.cpp:2074

# 地址 #01: 0xa5678
std::time_get<char, std::istreambuf_iterator<char>>::do_get()
/path/to/ndk/sources/cxx-stl/llvm-libc++/src/locale.cpp:2303

# 地址 #02: 0x1abcd (我们的代码!)
main
/Users/jiangzhongyang/work/live/CDN/study/Queche_Min/quiche/quic-demo/src/client.cpp:230
```

#### 分析结论

**崩溃位置**: client.cpp 第230行的main函数

**崩溃原因**:
1. 调用链: main() → std::cout → locale初始化 → std::time_get
2. Android bionic libc的locale支持不完整
3. 访问未实现的time_get函数导致SIGSEGV

---

## 🧰 工具使用详解

### llvm-addr2line

**完整路径**:
```
macOS: $NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-addr2line
Linux: $NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-addr2line
```

**常用参数**:
```bash
-e <file>      # 指定可执行文件
-f             # 显示函数名
-C             # C++ name demangling
-a             # 显示地址
-i             # 如果地址是内联函数，显示所有内联调用者
-p             # 每个地址输出一行（更紧凑）
```

**完整示例**:
```bash
$ADDR2LINE \
    -e quic-client-android \
    -f -C -a -i \
    0x1234 0x5678 0xabcd
```

---

### llvm-nm (符号查看)

查看二进制文件的符号表：

```bash
# 查看所有符号
$NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm quic-client-android

# 查看特定符号
$NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm quic-client-android | grep main

# 查看导出符号
$NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm -D libquiche_engine.so
```

**符号类型**:
- `T`: Text段（代码），已定义
- `U`: Undefined，未定义（需要动态链接）
- `W`: Weak symbol
- `t`: Local text段

---

### llvm-readelf (ELF文件分析)

查看ELF文件详细信息：

```bash
# 查看动态链接库依赖
$NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-readelf -d quic-client-android

# 查看段信息
$NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-readelf -S quic-client-android

# 查看符号表
$NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-readelf -s quic-client-android
```

---

### llvm-objdump (反汇编)

查看汇编代码：

```bash
# 反汇编整个文件
$NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-objdump -d quic-client-android > disasm.txt

# 反汇编特定函数
$NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-objdump -d quic-client-android | grep -A 50 "<main>:"

# 查看特定地址的汇编
$NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-objdump -d quic-client-android | grep -B 5 -A 10 "1abcd:"
```

---

## ❓ 常见问题

### Q1: addr2line返回 `??:0`

**原因**: 二进制文件被strip或没有debug符号

**解决**:
```bash
# 检查是否有debug信息
file quic-client-android

# 应该看到: not stripped
# 如果看到: stripped，需要重新编译

# 重新编译带符号版本
make -f Makefile.android clean
# 确保CXXFLAGS包含 -g
make -f Makefile.android all
```

---

### Q2: 地址解析到错误位置

**原因**: 使用的二进制文件与崩溃时不一致

**解决**:
```bash
# 确保使用的是同一个二进制文件
md5sum quic-client-android
adb shell "md5sum /data/local/tmp/quiche/quic-client"

# 如果不一致，重新部署
adb push quic-client-android /data/local/tmp/quiche/quic-client
```

---

### Q3: tombstone权限被拒绝

**原因**: 普通adb shell没有读取tombstone权限

**解决方案1**: 使用logcat（推荐）
```bash
adb logcat -d | grep -A 50 "DEBUG"
```

**解决方案2**: 通过su提权
```bash
adb shell "su -c 'cp /data/tombstones/tombstone_00 /sdcard/'"
adb pull /sdcard/tombstone_00 /tmp/
```

**解决方案3**: 使用开发者设备（已root）

---

### Q4: 找不到崩溃日志

**原因**: logcat缓冲区被覆盖

**解决**:
```bash
# 运行前先清空日志
adb logcat -c

# 崩溃后立即保存
adb logcat -d > crash.txt

# 增加logcat缓冲区大小
adb logcat -G 16M
```

---

### Q5: 系统库崩溃如何调试？

**场景**: 崩溃在 libc++_shared.so 中

**方法1**: 查看调用者
```bash
# 找到你的代码调用系统库的位置
grep "quic-client" crash.txt
```

**方法2**: 分析崩溃原因
```
#00 std::time_get::do_get()  <- 系统库
#01 std::ostream::operator<<  <- 系统库
#02 main                       <- 你的代码! (这里是根因)
```

**结论**: 虽然崩溃在系统库，但根因在你的代码（第#02帧）

---

## 📝 快速参考

### 常用命令速查

```bash
# === 基础调试 ===

# 清空日志
adb logcat -c

# 监控崩溃（实时）
adb logcat | grep -E "DEBUG|FATAL|SIGSEGV"

# 保存崩溃日志
adb logcat -d > crash.txt

# 提取堆栈
grep -A 50 "backtrace:" crash.txt

# === 地址解析 ===

# 设置工具路径
export NDK=$ANDROID_NDK_HOME
export ADDR2LINE=$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-addr2line

# 解析地址
$ADDR2LINE -e quic-client-android -f -C 0x1234

# === 符号检查 ===

# 检查是否有debug信息
file quic-client-android | grep "not stripped"

# 查看符号
$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-nm quic-client-android

# === 编译选项 ===

# Debug版本
CXXFLAGS += -O0 -g

# Release版本
CXXFLAGS += -Os
```

---

### 调试检查清单

**准备阶段**:
- [ ] 设置ANDROID_NDK_HOME环境变量
- [ ] 确认设备已连接 (`adb devices`)
- [ ] 清空logcat缓冲区 (`adb logcat -c`)

**崩溃捕获**:
- [ ] 运行程序触发崩溃
- [ ] 立即保存日志 (`adb logcat -d > crash.txt`)
- [ ] 确认日志包含"backtrace"

**地址解析**:
- [ ] 检查二进制有debug符号 (`file quic-client-android`)
- [ ] 确认使用NDK的addr2line（不是系统自带）
- [ ] 地址格式正确（0x前缀）
- [ ] 使用正确的二进制文件

**分析结果**:
- [ ] 找到你的代码在堆栈中的位置
- [ ] 识别崩溃的根本原因
- [ ] 查看源代码验证分析

---

## 🎯 实战案例

### 案例: Android QUIC Client 崩溃

**现象**:
```
Segmentation fault
```

**步骤1**: 获取日志
```bash
adb logcat -c
adb shell "./quic-client 192.168.1.4 1234"
adb logcat -d | grep -A 50 "DEBUG" > crash.txt
```

**步骤2**: 分析堆栈
```
F DEBUG   : signal 11 (SIGSEGV), code 2 (SEGV_ACCERR)
F DEBUG   : backtrace:
F DEBUG   :       #00 pc 00000000000a1234  /system/lib64/libc++_shared.so
F DEBUG   :       #02 pc 000000000001abcd  /data/local/tmp/quiche/quic-client
```

**步骤3**: 编译debug版本
```bash
# 修改 Makefile.android: CXXFLAGS += -g -O0
make -f Makefile.android clean all
adb push quic-client-android /data/local/tmp/quiche/quic-client
```

**步骤4**: 解析地址
```bash
$ADDR2LINE -e quic-client-android -f -C 0x1abcd
```

**结果**:
```
main
/path/to/client.cpp:230
```

**步骤5**: 查看源代码
```cpp
// client.cpp:230
std::cout << "Connection established" << std::endl;  // <- 崩溃位置
```

**步骤6**: 分析根因
- std::cout初始化触发locale加载
- Android bionic libc的locale不完整
- 访问未实现的std::time_get导致SIGSEGV

**步骤7**: 修复
```cpp
// 替换为
printf("Connection established\n");
fflush(stdout);
```

**验证**: ✅ 不再崩溃

---

## 🔗 参考资源

### 官方文档
- [Android NDK Debugging](https://developer.android.com/ndk/guides/debugging)
- [NDK Stack Tool](https://developer.android.com/ndk/guides/ndk-stack)
- [Android Logcat](https://developer.android.com/tools/logcat)

### 相关工具
- `adb`: Android Debug Bridge
- `llvm-addr2line`: 地址到源码行映射
- `llvm-nm`: 符号表查看
- `llvm-readelf`: ELF文件分析
- `ndk-stack`: NDK官方堆栈分析工具（可选）

### 项目相关文档
- `ANDROID_CRASH_FIX.md`: 本项目crash修复详解
- `SOLUTION_A_SUCCESS.md`: 符号链接问题修复
- `ANDROID_PROJECT_COMPLETE.md`: 项目完成总结

---

**最后更新**: 2025-11-08
**作者**: Android QUIC Client项目团队
**适用版本**: Android NDK 23.2.8568313, API 21+
