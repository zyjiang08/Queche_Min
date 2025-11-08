# Android QUIC Client 构建指南

## ✅ 构建成功

Android ARM64版本的QUIC客户端已成功编译！

### 构建产物

```
quiche/quic-demo/
├── quic-client-android             # Debug版本 (71KB)
└── quic-client-android-stripped    # 优化版本 (53KB) ⭐️推荐
```

### 文件信息

```bash
# 文件类型
ELF 64-bit LSB pie executable, ARM aarch64

# 目标平台
Android API Level 21+ (Android 5.0+)
Architecture: arm64-v8a (aarch64)

# 大小对比
macOS x86_64:    2.1M (quic-client)
Android ARM64:   53K (quic-client-android-stripped)  ⬇️ -97%
```

---

## 🔧 构建方法

### 前置要求

1. **Android NDK 23.2.8568313**
   ```bash
   export ANDROID_NDK_HOME=/Users/jiangzhongyang/Library/Android/sdk/ndk/23.2.8568313
   ```

2. **已编译的libquiche_engine.so**
   - 位置: `lib/android/arm64-v8a/libquiche_engine.so`
   - 如果没有，先运行: `./quiche_engine_all.sh android arm64-v8a`

### 编译命令

```bash
cd quiche/quic-demo

# 编译debug版本
make -f Makefile.android all

# 创建优化版本（推荐）
make -f Makefile.android strip

# 清理
make -f Makefile.android clean
```

---

## 📦 依赖库

### 必需的共享库

Android quic-client需要以下动态库：

```
libquiche_engine.so    # QUIC引擎库 (1.4M)
libc++_shared.so       # C++标准库（Android NDK自带）
liblog.so              # Android日志库（系统自带）
libm.so                # 数学库（系统自带）
libdl.so               # 动态链接库（系统自带）
libc.so                # C标准库（系统自带）
```

**注意**: 除了`libquiche_engine.so`和`libc++_shared.so`，其他库都是Android系统自带。

---

## 🚀 在Android设备上运行

### 方法1: 使用自动部署脚本（推荐） ⭐️

我们提供了一个自动部署脚本来简化部署过程：

```bash
cd quiche/quic-demo

# 运行部署脚本
./deploy_android.sh
```

脚本会自动：
1. ✅ 检查所有必需文件
2. ✅ 创建设备目录
3. ✅ 推送可执行文件和库
4. ✅ 设置权限
5. ✅ 显示使用说明

然后运行：
```bash
adb shell "cd /data/local/tmp/quiche && LD_LIBRARY_PATH=. ./quic-client <server_ip> <port>"
```

### 方法2: 手动部署

```bash
# 1. 创建设备目录
adb shell mkdir -p /data/local/tmp/quiche

# 2. 推送可执行文件
adb push quic-client-android /data/local/tmp/quiche/quic-client
adb shell chmod +x /data/local/tmp/quiche/quic-client

# 3. 推送依赖库
adb push ../../lib/android/arm64-v8a/libquiche_engine.so /data/local/tmp/quiche/
adb push $ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so /data/local/tmp/quiche/

# 4. 在设备上运行（⚠️ 必须设置LD_LIBRARY_PATH）
adb shell "cd /data/local/tmp/quiche && LD_LIBRARY_PATH=. ./quic-client <server_ip> <port>"
```

**示例**:
```bash
# 连接到QUIC服务器
adb shell "cd /data/local/tmp/quiche && LD_LIBRARY_PATH=. ./quic-client 192.168.1.100 4433"
```

⚠️ **重要**:
- 必须使用 `LD_LIBRARY_PATH=.` 来加载当前目录的共享库
- 必须使用 `cd /data/local/tmp/quiche &&` 确保在正确目录运行

### 方法2: 集成到Android应用（生产环境）

将库文件放入APK的`lib/arm64-v8a/`目录：

```
your-app.apk
└── lib/
    └── arm64-v8a/
        ├── libquiche_engine.so      # 您的QUIC库
        └── libc++_shared.so          # NDK的C++库
```

在Java/Kotlin代码中加载库：
```java
static {
    System.loadLibrary("c++_shared");
    System.loadLibrary("quiche_engine");
}
```

---

## 🔍 验证构建

### 检查架构
```bash
file quic-client-android
# 输出: ELF 64-bit LSB pie executable, ARM aarch64
```

### 检查依赖
```bash
llvm-readelf -d quic-client-android | grep NEEDED
```

输出:
```
0x0000000000000001 (NEEDED)  Shared library: [libquiche_engine.so]
0x0000000000000001 (NEEDED)  Shared library: [liblog.so]
0x0000000000000001 (NEEDED)  Shared library: [libm.so]
0x0000000000000001 (NEEDED)  Shared library: [libdl.so]
0x0000000000000001 (NEEDED)  Shared library: [libc++_shared.so]
0x0000000000000001 (NEEDED)  Shared library: [libc.so]
```

### 检查符号
```bash
llvm-nm -D quic-client-android | grep -i quiche | head -5
# 应该看到quiche_engine相关的符号
```

---

## 🎯 优化说明

### 编译优化选项

```makefile
# 优化标志
-Os                      # 优化体积
-fPIE                   # 位置无关可执行文件（Android要求）
-ffunction-sections     # 函数分段
-fdata-sections         # 数据分段

# 链接优化标志
-pie                    # 创建PIE可执行文件
-Wl,--gc-sections       # 移除未使用的段
```

### 体积优化效果

```
Debug版本:     71KB
Stripped版本:  53KB  (-25%)
```

**总体积对比**:
```
macOS x86_64:  2.1M
Android ARM64: 53K   (-97% ⬇️)
```

Android版本更小的原因：
1. 动态链接libquiche_engine.so（macOS版本静态链接）
2. 使用Android系统库
3. PIE和死代码消除优化

---

## ⚠️ 注意事项

### 1. 运行时库路径

在Android设备上运行时，必须设置`LD_LIBRARY_PATH`：

```bash
# ✅ 正确
LD_LIBRARY_PATH=/path/to/libs ./quic-client

# ❌ 错误（找不到libquiche_engine.so）
./quic-client
```

### 2. libc++_shared.so

Android可执行文件依赖NDK的C++标准库：

**位置**:
```bash
$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so
```

**如何获取**:
```bash
# 从NDK复制
cp $ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so .
```

### 3. SELinux权限

在某些Android设备上，可能需要临时关闭SELinux：

```bash
# 查看SELinux状态
adb shell getenforce

# 临时关闭（需要root）
adb shell su -c setenforce 0

# 测试完成后恢复
adb shell su -c setenforce 1
```

### 4. 文件权限

确保可执行文件有执行权限：

```bash
adb shell chmod +x /data/local/tmp/quiche/quic-client
```

---

## 📊 完整构建统计

### 平台对比

| 平台 | 二进制大小 | 链接方式 | 依赖库 |
|------|-----------|---------|--------|
| **macOS x86_64** | 2.1M | 静态链接 | 无外部依赖 |
| **Android ARM64** | 53K | 动态链接 | libquiche_engine.so (1.4M) |

### 总体积对比（包含依赖）

```
macOS:   2.1M (单文件)
Android: 53K + 1.4M = 1.45M (客户端 + 库)

总优化: 2.1M → 1.45M = -0.65M (-31%)
```

---

## 🛠️ 故障排查

### 问题1: 找不到libquiche_engine.so

```
error: cannot locate symbol "quiche_engine_..."
```

**解决方案**:
```bash
# 设置库路径
export LD_LIBRARY_PATH=/data/local/tmp/quiche:$LD_LIBRARY_PATH
```

### 问题2: 找不到libc++_shared.so

```
CANNOT LINK EXECUTABLE: cannot locate symbol "_ZNSt..."
```

**解决方案**:
```bash
# 推送C++库到设备
adb push $ANDROID_NDK_HOME/.../libc++_shared.so /data/local/tmp/quiche/
```

### 问题3: Permission denied

```
/system/bin/sh: ./quic-client: Permission denied
```

**解决方案**:
```bash
adb shell chmod +x /data/local/tmp/quiche/quic-client
```

---

## 📝 示例：完整测试流程

```bash
#!/bin/bash
# 完整的Android客户端部署和测试脚本

# 1. 编译
cd quiche/quic-demo
make -f Makefile.android clean
make -f Makefile.android all
make -f Makefile.android strip

# 2. 准备文件
NDK_PATH=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android
LIB_PATH=../../lib/android/arm64-v8a

# 3. 推送到设备
adb shell mkdir -p /data/local/tmp/quiche
adb push quic-client-android-stripped /data/local/tmp/quiche/quic-client
adb push $LIB_PATH/libquiche_engine.so /data/local/tmp/quiche/
adb push $NDK_PATH/libc++_shared.so /data/local/tmp/quiche/
adb shell chmod +x /data/local/tmp/quiche/quic-client

# 4. 测试运行
adb shell "cd /data/local/tmp/quiche && LD_LIBRARY_PATH=. ./quic-client 192.168.1.100 4433"
```

---

## ✅ 构建成功确认

如果您看到以下信息，说明构建成功：

```
✅ Built quic-client-android successfully
-rwxr-xr-x  1 user  staff    71K Nov  8 14:15 quic-client-android

Size comparison:
-rwxr-xr-x  1 user  staff    71K Nov  8 14:15 quic-client-android
-rwxr-xr-x  1 user  staff    53K Nov  8 14:15 quic-client-android-stripped

✅ Stripped binary: quic-client-android-stripped
```

恭喜！您的Android QUIC客户端已准备就绪！ 🎉
