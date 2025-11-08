# 深度优化方案 - 快速开始指南

本文档提供深度优化方案的快速实施步骤。完整方案详见 [DEEP_OPTIMIZATION_PLAN.md](./DEEP_OPTIMIZATION_PLAN.md)。

## 📊 当前状态和目标

### 已完成（阶段1）
```
✅ libquiche.a:     44 MB → 15 MB  (-66%)
✅ quic-server:     4.6 MB → 3.9 MB  (-15%)
❌ quic-client:     4.6 MB (待优化)
❌ libquiche_engine.a: 60 MB (待优化)
```

### 最终目标（阶段2）
```
🎯 libquiche.a:         15 MB → 6 MB   (-60%, 总计-86%)
🎯 libquiche_engine.a:  60 MB → 13 MB  (-78%)
🎯 quic-client:         4.6 MB → 2.6 MB (-43%)
```

## 🚀 快速实施（推荐步骤）

我们按照从易到难、效果从显著到递减的顺序执行：

### 优先级排序

| 步骤 | 难度 | 效果 | 风险 | 优先级 |
|------|------|------|------|--------|
| **步骤2: LTO** | ⭐ 低 | ⭐⭐⭐ 大 | 低 | **1️⃣ 最高** |
| **步骤3: BoringSSL裁剪** | ⭐⭐ 中 | ⭐⭐ 中 | 中 | **2️⃣ 高** |
| **步骤5: 平台裁剪** | ⭐ 低 | ⭐ 小 | 低 | **3️⃣ 中** |
| 步骤1: 符号控制 | ⭐⭐⭐ 高 | 低 | 中 | 4️⃣ 低 |
| 步骤4: 功能裁剪 | ⭐⭐ 中 | 低 | 低 | 5️⃣ 低 |

## 第一次实施：步骤2 - LTO优化（最重要）

### 为什么先做步骤2？

- ✅ **效果最显著**: 预期减小 40-65%
- ✅ **风险最低**: 只是编译选项，不修改代码
- ✅ **实施最简单**: 改几个配置文件即可
- ✅ **可快速验证**: 几分钟内看到结果

### 实施步骤2 - LTO

#### 2.1 修改 Cargo.toml

```bash
cd /Users/jiangzhongyang/work/live/CDN/study/Queche_Min/quiche
```

编辑 `Cargo.toml`，在末尾添加：

```toml
[profile.release]
lto = "fat"              # 完整LTO
codegen-units = 1        # 单代码单元
opt-level = "z"          # 优化体积
strip = true             # 自动strip
panic = "abort"          # 移除unwinding
overflow-checks = false  # 禁用溢出检查
```

#### 2.2 重新构建 libquiche

```bash
# 清理
cargo clean

# 构建（会比之前慢3-5倍，请耐心等待）
time cargo build --release \
  --no-default-features \
  --features ffi,boringssl-vendored,qlog

# 检查大小
ls -lh target/release/libquiche.a
```

**预期结果**: 15 MB → 10-12 MB

#### 2.3 复制到 quic-demo

```bash
cp target/release/libquiche.a quiche/quic-demo/lib/
```

#### 2.4 修改 engine Makefile

编辑 `engine/Makefile`，找到 `CFLAGS` 和 `CXXFLAGS` 行，添加：

```makefile
CFLAGS += -flto=full -ffunction-sections -fdata-sections -Os
CXXFLAGS += -flto=full -ffunction-sections -fdata-sections -Os
LDFLAGS += -flto=full

# macOS
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -Wl,-dead_strip
endif

# Linux
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -Wl,--gc-sections
endif
```

#### 2.5 重新构建 libquiche_engine

```bash
cd engine

# 清理
make clean

# 构建
time make PLATFORM=macos ARCH=x86_64

# 检查大小
ls -lh lib/macos/x86_64/libquiche_engine.a
```

**预期结果**: 60 MB → 18-22 MB

#### 2.6 验证 quic-client

```bash
cd ../quic-demo

# 重新构建client
make clean && make PLATFORM=macos ARCH=x86_64

# 检查大小
ls -lh quic-client
size -m quic-client
```

**预期结果**: 4.6 MB → 3.0-3.5 MB

#### 2.7 功能测试

```bash
# 启动server（在终端1）
./quic-server 127.0.0.1 4433

# 运行client（在终端2）
./quic-client 127.0.0.1 4433

# 或使用测试脚本
./test_communication.sh
```

✅ **如果测试通过，步骤2完成！**

---

## 第二次实施：步骤3 - BoringSSL裁剪

### 3.1 分析当前包含的DTLS文件

```bash
cd quiche/quic-demo
ar -t lib/libquiche.a | grep -E "dtls|d1_"
```

预期输出：
```
d1_both.cc.o
d1_lib.cc.o
d1_pkt.cc.o
d1_srtp.cc.o
dtls_method.cc.o
dtls_record.cc.o
```

### 3.2 创建BoringSSL裁剪脚本

创建 `/Users/jiangzhongyang/work/live/CDN/study/Queche_Min/quiche/trim_boringssl.sh`:

```bash
#!/bin/bash

set -e

BSSL_DIR="deps/boringssl"

# 检查是否已有备份
if [ ! -d "${BSSL_DIR}.backup" ]; then
    echo "创建备份..."
    cp -r $BSSL_DIR ${BSSL_DIR}.backup
else
    echo "备份已存在，跳过"
fi

# 移除DTLS相关文件
echo "移除DTLS文件..."
rm -f $BSSL_DIR/ssl/d1_both.cc
rm -f $BSSL_DIR/ssl/d1_lib.cc
rm -f $BSSL_DIR/ssl/d1_pkt.cc
rm -f $BSSL_DIR/ssl/d1_srtp.cc
rm -f $BSSL_DIR/ssl/dtls_method.cc
rm -f $BSSL_DIR/ssl/dtls_record.cc

# 移除RC4/RC2
echo "移除过时加密算法..."
rm -rf $BSSL_DIR/crypto/rc4
rm -rf $BSSL_DIR/crypto/rc2
rm -f $BSSL_DIR/crypto/cipher_extra/e_rc4.c
rm -f $BSSL_DIR/crypto/cipher_extra/e_rc2.c

# 移除DSA
rm -rf $BSSL_DIR/crypto/dsa

# 移除HRSS (如果不需要后量子密码)
rm -rf $BSSL_DIR/crypto/hrss

echo "✅ BoringSSL裁剪完成"
echo "备份位置: ${BSSL_DIR}.backup"
echo ""
echo "如需恢复："
echo "  rm -rf $BSSL_DIR"
echo "  mv ${BSSL_DIR}.backup $BSSL_DIR"
```

### 3.3 执行裁剪

```bash
cd /Users/jiangzhongyang/work/live/CDN/study/Queche_Min/quiche
chmod +x trim_boringssl.sh
./trim_boringssl.sh
```

### 3.4 重新构建

```bash
# 清理
cargo clean

# 重新构建
cargo build --release \
  --no-default-features \
  --features ffi,boringssl-vendored,qlog

# 检查大小
ls -lh target/release/libquiche.a
```

**预期结果**: 10-12 MB → 7-9 MB

### 3.5 更新quic-demo

```bash
cp target/release/libquiche.a quiche/quic-demo/lib/

cd quiche/quic-demo
make clean && make

ls -lh quic-client
```

**预期结果**: 3.0-3.5 MB → 2.8-3.0 MB

### 3.6 功能测试

```bash
./test_communication.sh
```

✅ **如果TLS握手正常，步骤3完成！**

---

## 第三次实施：步骤5 - 平台专用裁剪

### 5.1 分析多余的平台文件

```bash
cd quiche/quic-demo
ar -t lib/libquiche.a | grep "cpu-"
```

预期输出：
```
cpu-aarch64-fuchsia.c.o
cpu-aarch64-linux.c.o
cpu-aarch64-win.c.o
cpu-arm-linux.c.o
cpu-arm.c.o
cpu-ppc64le.c.o
```

### 5.2 移除未使用的CPU文件（仅macOS x86_64需要）

```bash
cd /Users/jiangzhongyang/work/live/CDN/study/Queche_Min

# 从libquiche.a中移除不需要的CPU文件
ar -d target/release/libquiche.a \
  cpu-aarch64-fuchsia.c.o \
  cpu-aarch64-win.c.o \
  cpu-arm-linux.c.o \
  cpu-ppc64le.c.o \
  windows.c.o \
  fuchsia.c.o \
  thread_win.c.o 2>/dev/null || true

# Strip
strip -S target/release/libquiche.a

# 检查大小
ls -lh target/release/libquiche.a
```

**预期结果**: 7-9 MB → 6-8 MB

### 5.3 更新并验证

```bash
cp target/release/libquiche.a quiche/quic-demo/lib/

cd quiche/quic-demo
make clean && make
ls -lh quic-client

./test_communication.sh
```

**预期结果**: quic-client 2.8-3.0 MB → 2.6-2.8 MB

---

## 📊 优化效果汇总

完成步骤2+3+5后的预期效果：

| 库/程序 | 原始 | 阶段1 | 阶段2 | 总计减小 |
|---------|------|-------|-------|----------|
| libquiche.a | 44 MB | 15 MB | **6-8 MB** | **-82-86%** |
| libquiche_engine.a | 60 MB | - | **13-15 MB** | **-75-78%** |
| quic-client | 4.6 MB | - | **2.6-2.8 MB** | **-39-43%** |

## ⏱️ 预计时间

- **步骤2（LTO）**: 构建时间 20-30分钟，实施总时间 ~40分钟
- **步骤3（BoringSSL）**: 构建时间 15-20分钟，实施总时间 ~30分钟
- **步骤5（平台裁剪）**: 构建时间 0分钟，实施总时间 ~5分钟

**总计**: ~1.5小时

## ⚠️ 注意事项

### 如果出现问题

**LTO 编译错误**:
```bash
# 降低LTO级别
[profile.release]
lto = "thin"  # 从 "fat" 改为 "thin"
```

**BoringSSL 裁剪后链接失败**:
```bash
# 恢复BoringSSL
cd quiche
rm -rf deps/boringssl
mv deps/boringssl.backup deps/boringssl
```

**功能测试失败**:
```bash
# 回滚到阶段1
cd quiche/quic-demo
# 使用之前备份的库文件
```

### 保留备份

建议在每个步骤后保存备份：

```bash
# 步骤2完成后
cp target/release/libquiche.a target/release/libquiche_step2.a

# 步骤3完成后
cp target/release/libquiche.a target/release/libquiche_step3.a

# 步骤5完成后
cp target/release/libquiche.a target/release/libquiche_final.a
```

## 🎯 下一步（可选）

如果需要进一步优化：

1. **步骤1: 符号可见性控制** - 减少导出符号（技术难度高）
2. **步骤4: 功能裁剪** - 移除datagram-socket（效果小）
3. **多平台构建** - 为iOS/Android创建专用脚本

## 📝 验证检查清单

每个步骤完成后检查：

- [ ] 库大小是否按预期减小
- [ ] `./test_communication.sh` 测试通过
- [ ] TLS握手正常（查看日志）
- [ ] 数据传输正常
- [ ] 无明显性能下降

## 🚀 一键脚本（高级用户）

创建 `optimize_quick.sh`:

```bash
#!/bin/bash
set -e

echo "🚀 开始快速优化..."

# 步骤2: LTO
echo "步骤2: 启用LTO..."
cd /Users/jiangzhongyang/work/live/CDN/study/Queche_Min/quiche
# 备份Cargo.toml
cp Cargo.toml Cargo.toml.bak

# 添加LTO配置（需要手动完成，或使用sed）
# 然后构建...

# 步骤3: 裁剪BoringSSL
echo "步骤3: 裁剪BoringSSL..."
./trim_boringssl.sh

# 重新构建
cargo clean
cargo build --release --no-default-features --features ffi,boringssl-vendored,qlog

# 步骤5: 平台裁剪
echo "步骤5: 平台专用裁剪..."
ar -d target/release/libquiche.a cpu-aarch64-fuchsia.c.o cpu-aarch64-win.c.o cpu-arm-linux.c.o cpu-ppc64le.c.o windows.c.o fuchsia.c.o 2>/dev/null || true
strip -S target/release/libquiche.a

# 更新quic-demo
cp target/release/libquiche.a quiche/quic-demo/lib/

# 重新构建engine
cd engine
make clean && make PLATFORM=macos ARCH=x86_64

# 重新构建client
cd ../quic-demo
make clean && make

echo "✅ 优化完成！"
ls -lh quic-client
```

---

**文档创建**: 2025-11-08
**版本**: v1.0
**适用平台**: macOS x86_64（其他平台类似）
