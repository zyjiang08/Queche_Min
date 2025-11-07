# Quiche Engine 构建系统更新说明

## 版本 2.0

### 🎯 主要变更

#### 1. **命令行参数格式改进**

**旧格式** (使用冒号):
```bash
./quiche_engine_all.sh macos:arm64
./quiche_engine_all.sh ios:x86_64
./quiche_engine_all.sh android:arm64-v8a
```

**新格式** (使用空格):
```bash
./quiche_engine_all.sh macos arm64
./quiche_engine_all.sh ios x86_64
./quiche_engine_all.sh android arm64-v8a
```

✅ **优势**: 更符合标准命令行工具的习惯

#### 2. **支持 "all" 关键字编译所有架构**

**编译平台所有架构**:
```bash
# 编译所有 iOS 架构 (arm64 + x86_64)
./quiche_engine_all.sh ios all

# 编译所有 macOS 架构 (arm64 + x86_64)
./quiche_engine_all.sh macos all

# 编译所有 Android 架构 (arm64-v8a + armeabi-v7a + x86 + x86_64)
./quiche_engine_all.sh android all
```

✅ **优势**: 一条命令编译多个架构，方便 CI/CD

#### 3. **目录结构优化**

**旧结构**:
```
libs/
├── ios/
├── android/
├── macos/
└── include/
    └── quiche_engine.h
```

**新结构**:
```
lib/                        # 库文件目录
├── ios/
│   ├── arm64/
│   └── x86_64/
├── android/
│   ├── arm64-v8a/
│   ├── armeabi-v7a/
│   ├── x86/
│   └── x86_64/
└── macos/
    ├── arm64/
    └── x86_64/

include/                    # 头文件独立目录
└── quiche_engine.h
```

✅ **优势**:
- `lib/` 和 `include/` 分离，符合 Unix 标准
- 更清晰的目录结构
- 便于集成到项目中

### 📋 命令对照表

| 功能 | 旧命令 | 新命令 |
|------|--------|--------|
| iOS 真机 | `ios:arm64` | `ios arm64` |
| iOS 模拟器 | `ios:x86_64` | `ios x86_64` |
| iOS 所有架构 | ❌ 不支持 | `ios all` |
| macOS Apple Silicon | `macos:arm64` | `macos arm64` |
| macOS Intel | `macos:x86_64` | `macos x86_64` |
| macOS 所有架构 | ❌ 不支持 | `macos all` |
| Android ARM64 | `android:arm64-v8a` | `android arm64-v8a` |
| Android 所有架构 | `android` | `android all` 或 `android` |
| 所有平台 | `all` | `all` |

### 🆕 新功能

1. **多架构编译**: 在一次构建中编译多个架构
   ```bash
   ./quiche_engine_all.sh ios all android all
   ```

2. **混合编译**: 指定多个平台和架构
   ```bash
   ./quiche_engine_all.sh ios arm64 macos arm64 android arm64-v8a
   ```

3. **构建计划预览**: 执行前显示将要构建的内容
   ```
   [INFO] Build plan:
   [INFO]   iOS: arm64 x86_64
   [INFO]   Android: arm64-v8a
   ```

### 📁 产物路径变更

| 内容 | 旧路径 | 新路径 |
|------|--------|--------|
| iOS 库 | `libs/ios/arm64/...` | `lib/ios/arm64/...` |
| macOS 库 | `libs/macos/arm64/...` | `lib/macos/arm64/...` |
| Android 库 | `libs/android/arm64-v8a/...` | `lib/android/arm64-v8a/...` |
| 头文件 | `libs/include/quiche_engine.h` | `include/quiche_engine.h` |

### 🔧 .gitignore 更新

```diff
- libs/
- include/
+ /lib/
+ /include/
```

现在使用根目录绝对路径，避免忽略子目录中的同名文件夹。

### 📚 文档更新

#### 新文档

1. **BUILD_GUIDE.md** - 完整的编译指南
   - 详细的命令格式说明
   - 丰富的示例
   - 产物说明
   - 平台架构支持表

2. **CHANGES.md** - 本变更说明文档

#### 更新的文档

1. **README_BUILD.md** - 已更新命令格式
2. **quiche_engine_all.sh** - 内置帮助信息已更新

### 🚀 快速迁移指南

#### 如果你使用旧命令格式

只需将 `:` 改为空格即可：

```bash
# 旧
./quiche_engine_all.sh ios:arm64

# 新
./quiche_engine_all.sh ios arm64
```

#### 如果你依赖旧目录结构

更新你的路径：

```bash
# 旧
cp libs/ios/arm64/libquiche_engine.a /your/project/

# 新
cp lib/ios/arm64/libquiche_engine.a /your/project/
cp include/quiche_engine.h /your/project/include/
```

#### 在 CI/CD 中

```yaml
# GitHub Actions 示例
- name: Build iOS
  run: ./quiche_engine_all.sh ios arm64

- name: Build Android
  run: |
    export ANDROID_NDK_HOME=${{ env.ANDROID_NDK_HOME }}
    ./quiche_engine_all.sh android all

- name: Upload artifacts
  uses: actions/upload-artifact@v3
  with:
    name: libraries
    path: |
      lib/
      include/
```

### ✅ 兼容性

| 项目 | 状态 |
|------|------|
| 构建脚本参数 | ⚠️ 不兼容 - 需更新为新格式 |
| 产物路径 | ⚠️ 不兼容 - 需更新路径引用 |
| 库文件内容 | ✅ 完全兼容 - 无变化 |
| API 接口 | ✅ 完全兼容 - 无变化 |
| 依赖要求 | ✅ 完全兼容 - 无变化 |

### 📝 示例更新

#### iOS Xcode 项目

```diff
# Build Phases - Run Script
- ./build_script.sh ios:arm64
+ ./build_script.sh ios arm64

# Copy Files
- ${PROJECT_DIR}/libs/ios/arm64/libquiche_engine.a
+ ${PROJECT_DIR}/lib/ios/arm64/libquiche_engine.a

# Header Search Paths
- ${PROJECT_DIR}/libs/include
+ ${PROJECT_DIR}/include
```

#### Android CMakeLists.txt

```diff
- set(QUICHE_LIB_DIR ${CMAKE_SOURCE_DIR}/libs/android/${ANDROID_ABI})
+ set(QUICHE_LIB_DIR ${CMAKE_SOURCE_DIR}/lib/android/${ANDROID_ABI})

- include_directories(${CMAKE_SOURCE_DIR}/libs/include)
+ include_directories(${CMAKE_SOURCE_DIR}/include)
```

### 🐛 Bug 修复

1. ✅ 修复了无法在一次构建中编译多个架构的问题
2. ✅ 修复了参数解析不直观的问题
3. ✅ 改进了错误提示信息

### 💡 建议

1. **推荐使用新的 "all" 关键字**:
   ```bash
   # 一次性编译所有需要的架构
   ./quiche_engine_all.sh ios all android all
   ```

2. **在 CI/CD 中充分利用多平台编译**:
   ```bash
   # 一条命令完成所有编译
   ./quiche_engine_all.sh ios all macos all android all
   ```

3. **使用新的目录结构**:
   - 将 `lib/` 和 `include/` 作为独立模块
   - 便于版本管理和分发

### 📖 更多信息

- 完整编译指南: [BUILD_GUIDE.md](BUILD_GUIDE.md)
- 故障排除: [README_BUILD.md](README_BUILD.md)
- 配置说明: [.cargo/DESIGN.md](.cargo/DESIGN.md)
- 项目架构: [CLAUDE.md](CLAUDE.md)

---

**更新日期**: 2025-01-07
**版本**: 2.0.0
