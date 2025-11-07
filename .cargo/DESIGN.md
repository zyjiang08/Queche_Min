# Cargo 配置设计说明

## 为什么 `.cargo/config.toml` 包含绝对路径？

### 问题背景

在为 Android 交叉编译时，Cargo 需要知道正确的工具链路径：

```toml
[target.aarch64-linux-android]
ar = "/Users/jiangzhongyang/Library/Android/sdk/ndk/21.4.7075529/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-ar"
linker = "/Users/jiangzhongyang/Library/Android/sdk/ndk/21.4.7075529/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android21-clang"
```

### 为什么必须使用绝对路径？

Cargo 的 target 配置**不支持环境变量**和**相对路径**：

❌ **不支持的写法**：
```toml
# ❌ 环境变量不会被展开
ar = "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-ar"

# ❌ ~ 符号不会被展开
linker = "~/Library/Android/sdk/ndk/..."

# ❌ 相对路径可能指向错误位置
ar = "../../ndk/bin/llvm-ar"
```

✅ **唯一支持的写法**：
```toml
# ✅ 必须使用完整的绝对路径
ar = "/Users/jiangzhongyang/Library/Android/sdk/ndk/21.4.7075529/..."
```

## 绝对路径的影响

### ❌ 潜在问题

如果这个文件被提交到 Git 仓库：

1. **其他开发者无法使用**
   ```bash
   # 开发者 A 的路径
   /Users/alice/Library/Android/sdk/ndk/21.4.7075529/...

   # 开发者 B 克隆后
   /Users/bob/Android/Sdk/ndk/23.2.8568313/...  # ❌ 路径完全不同
   ```

2. **CI/CD 环境失败**
   ```bash
   # GitHub Actions
   /usr/local/lib/android/sdk/ndk/25.1.8937393/...

   # Jenkins
   /opt/android-sdk/ndk-bundle/...
   ```

3. **跨平台问题**
   ```bash
   # macOS
   /Users/user/Library/Android/sdk/ndk/...

   # Linux
   /home/user/Android/Sdk/ndk/...

   # Windows
   C:\Users\user\AppData\Local\Android\Sdk\ndk\...
   ```

4. **NDK 版本不同**
   ```bash
   # 开发者使用不同的 NDK 版本
   21.4.7075529  vs  23.2.8568313  vs  25.1.8937393
   ```

## ✅ 我们的解决方案

### 1. **动态生成 + 不提交到 Git**

```bash
# .gitignore 中排除
.cargo/config.toml

# 每次构建时动态生成
./quiche_engine_all.sh android:arm64-v8a
```

### 2. **基于环境变量自动生成**

`quiche_engine_all.sh` 脚本会：

```bash
# 1. 读取环境变量
ANDROID_NDK_HOME=/path/to/your/ndk

# 2. 检测主机平台
HOST_TAG="darwin-x86_64"  # 或 linux-x86_64

# 3. 动态生成配置文件
cat > .cargo/config.toml << EOF
[target.aarch64-linux-android]
ar = "${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/${HOST_TAG}/bin/llvm-ar"
linker = "${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/${HOST_TAG}/bin/aarch64-linux-android21-clang"
rustflags = [
    "-C", "link-arg=-lgcc",
    "-C", "link-arg=-Wl,--allow-shlib-undefined",
    "-L", "${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/${HOST_TAG}/sysroot/usr/lib/aarch64-linux-android/21"
]
EOF
```

### 3. **跨开发者和跨平台支持**

每个开发者只需设置自己的 `ANDROID_NDK_HOME`：

```bash
# 开发者 A (macOS, NDK 21)
export ANDROID_NDK_HOME=/Users/alice/Library/Android/sdk/ndk/21.4.7075529
./quiche_engine_all.sh android:arm64-v8a

# 开发者 B (Linux, NDK 23)
export ANDROID_NDK_HOME=/home/bob/Android/Sdk/ndk/23.2.8568313
./quiche_engine_all.sh android:arm64-v8a

# CI/CD (GitHub Actions, NDK 25)
export ANDROID_NDK_HOME=/usr/local/lib/android/sdk/ndk/25.1.8937393
./quiche_engine_all.sh android:arm64-v8a
```

脚本会自动生成适合当前环境的配置文件。

### 4. **工作流程**

```
开发者 A                          开发者 B                        CI/CD
   |                                 |                              |
   | git clone                       | git clone                    | git clone
   v                                 v                              v
设置 NDK_HOME                   设置 NDK_HOME                  设置 NDK_HOME
   |                                 |                              |
   | ./quiche_engine_all.sh          | ./quiche_engine_all.sh       | ./quiche_engine_all.sh
   v                                 v                              v
自动生成 config.toml            自动生成 config.toml          自动生成 config.toml
(基于 A 的路径)                 (基于 B 的路径)              (基于 CI 的路径)
   |                                 |                              |
   v                                 v                              v
 ✅ 构建成功                      ✅ 构建成功                    ✅ 构建成功
```

## 验证

### 检查文件是否被 Git 追踪

```bash
# 应该显示：未追踪
git status .cargo/config.toml

# 如果显示已修改，说明之前被提交过，需要移除
git rm --cached .cargo/config.toml
git commit -m "Remove .cargo/config.toml from git tracking"
```

### 检查 .gitignore

```bash
# 应该包含这一行
grep "config.toml" .gitignore
# 输出：.cargo/config.toml
```

### 测试自动生成

```bash
# 删除配置文件
rm .cargo/config.toml

# 重新构建，会自动生成
./quiche_engine_all.sh android:arm64-v8a

# 检查是否生成
cat .cargo/config.toml
```

## 最佳实践

### ✅ 推荐做法

1. **使用构建脚本**
   ```bash
   ./quiche_engine_all.sh android:arm64-v8a
   ```

2. **设置环境变量**
   ```bash
   export ANDROID_NDK_HOME=/path/to/your/ndk
   ```

3. **让脚本自动生成配置**
   - 不要手动创建 `.cargo/config.toml`
   - 不要提交 `.cargo/config.toml` 到 Git

### ❌ 避免的做法

1. **手动编辑 `.cargo/config.toml`**
   - 下次运行脚本会被覆盖

2. **提交 `.cargo/config.toml` 到 Git**
   - 会导致其他开发者无法构建

3. **使用硬编码路径**
   - 无法适应不同开发环境

## 故障排除

### 问题：构建失败，找不到工具链

```bash
error: linker `aarch64-linux-android21-clang` not found
```

**解决方案**：
```bash
# 1. 检查 ANDROID_NDK_HOME 是否设置
echo $ANDROID_NDK_HOME

# 2. 检查路径是否存在
ls $ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/

# 3. 重新生成配置
rm .cargo/config.toml
./quiche_engine_all.sh android:arm64-v8a
```

### 问题：Git 显示 config.toml 已修改

```bash
Changes not staged for commit:
  modified:   .cargo/config.toml
```

**解决方案**：
```bash
# 从 Git 追踪中移除（保留本地文件）
git rm --cached .cargo/config.toml
git commit -m "Stop tracking .cargo/config.toml"

# 确认在 .gitignore 中
echo ".cargo/config.toml" >> .gitignore
```

## 总结

| 方面 | 说明 |
|------|------|
| **为什么用绝对路径** | Cargo 不支持环境变量和相对路径 |
| **是否有影响** | ✅ 无影响 - 文件不提交，每次自动生成 |
| **如何工作** | 基于 `$ANDROID_NDK_HOME` 动态生成 |
| **跨平台支持** | ✅ 支持 - 自动检测 macOS/Linux |
| **跨 NDK 版本** | ✅ 支持 - 使用环境变量指定 |
| **团队协作** | ✅ 无冲突 - 每人生成自己的配置 |
| **CI/CD** | ✅ 支持 - 自动适应 CI 环境 |

**结论**：绝对路径是 Cargo 的限制，但通过自动生成机制完全解决了可移植性问题。
