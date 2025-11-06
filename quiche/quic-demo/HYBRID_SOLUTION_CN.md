# 混合方案：通过 Cargo 构建 C++ Engine

## 🎯 方案概述

**核心思路**：采用方案C的目录结构和架构分离，但通过 Cargo 的 `build.rs` 构建C++代码（类似BoringSSL）。

```
用户视角:
  cargo build --features cpp-engine    →  自动构建所有内容

技术实现:
  build.rs  →  编译 BoringSSL (C/C++)
           →  编译 quiche_engine (C++)
           →  链接所有库
```

---

## 📊 方案对比

| 方案 | 构建方式 | 用户体验 | 复杂度 | 推荐度 |
|------|---------|---------|--------|--------|
| 原方案C | Cargo + CMake独立 | 需要两步构建 | 低 | ⭐⭐⭐⭐ |
| **混合方案** | Cargo统一 | 一步构建 ✨ | 中 | ⭐⭐⭐⭐⭐ |

---

## ✅ 优势分析

### 1. 用户体验极佳
```bash
# 用户只需要一条命令
cargo build --features cpp-engine

# 或者设置默认特性
cargo build
```

### 2. 保持架构分离
```
quiche/api/          # C++ Engine 代码（物理分离）
    ├── include/
    ├── src/
    └── ...
build.rs             # 构建脚本（逻辑集成）
    └── 条件编译 cpp-engine
```

### 3. 依赖管理统一
```toml
[dependencies]
# Rust 依赖...

[build-dependencies]
cmake = "0.1"        # 构建 BoringSSL
cc = "1.0"           # 构建 C++ Engine
pkg-config = "0.3"   # 查找 libev
```

### 4. 与现有模式一致
```rust
// build.rs 已经在编译 BoringSSL (C/C++)
let bssl_dir = get_boringssl_cmake_config().build();

// 可以类似地编译 C++ Engine
let engine_dir = build_cpp_engine();
```

---

## 📁 目录结构（与方案C相同）

```
quiche/
├── quiche/
│   ├── src/
│   │   ├── lib.rs
│   │   ├── ffi.rs
│   │   └── build.rs          # ⭐ 扩展：编译C++
│   ├── include/
│   │   └── quiche.h
│   ├── api/                   # ⭐ C++ Engine
│   │   ├── include/
│   │   │   └── quiche_engine.h
│   │   ├── src/
│   │   │   ├── quiche_engine_impl.{h,cpp}
│   │   │   ├── quiche_engine_api.cpp
│   │   │   └── thread_utils.{h,cpp}
│   │   └── CMakeLists.txt    # 可选：用于独立构建
│   └── Cargo.toml            # ⭐ 添加 cpp-engine 特性
└── examples/quic-demo/
```

---

## 🔧 实施方案

### 步骤 1: 更新 Cargo.toml

```toml
# quiche/Cargo.toml

[features]
default = ["boringssl-vendored", "http3"]

# ⭐ 新增：C++ Engine 特性
cpp-engine = ["dep:libev-sys"]

[build-dependencies]
cmake = "0.1"
cc = "1.0"
pkg-config = { version = "0.3", optional = true }
cdylib-link-lines = { version = "0.1", optional = true }

# ⭐ 新增：libev 查找（可选依赖）
[dependencies]
libev-sys = { version = "0.1", optional = true }
# 或者不添加依赖，在 build.rs 中用 pkg-config 查找
```

### 步骤 2: 扩展 build.rs

```rust
// quiche/src/build.rs

fn main() {
    // 现有：构建 BoringSSL
    if cfg!(feature = "boringssl-vendored") &&
        !cfg!(feature = "boringssl-boring-crate") &&
        !cfg!(feature = "openssl")
    {
        let bssl_dir = build_boringssl();
        println!("cargo:rustc-link-search=native={}", bssl_dir);
        println!("cargo:rustc-link-lib=static=ssl");
        println!("cargo:rustc-link-lib=static=crypto");
    }

    // ⭐ 新增：构建 C++ Engine
    #[cfg(feature = "cpp-engine")]
    {
        build_cpp_engine();
    }

    // 现有：pkg-config, cdylib-link-lines 等
    // ...
}

/// 构建 C++ Engine
#[cfg(feature = "cpp-engine")]
fn build_cpp_engine() {
    use std::env;
    use std::path::PathBuf;

    println!("cargo:rerun-if-changed=api/src");
    println!("cargo:rerun-if-changed=api/include");

    // 方案 A：使用 cc crate 直接编译
    build_with_cc();

    // 方案 B：使用 cmake（如果已有 CMakeLists.txt）
    // build_with_cmake();
}

/// 方案 A：使用 cc crate 编译 C++ 代码
#[cfg(feature = "cpp-engine")]
fn build_with_cc() {
    use cc::Build;

    // 查找 libev
    let libev = pkg_config::probe_library("libev")
        .expect("libev not found. Please install libev: \
                 apt-get install libev-dev (Ubuntu) or \
                 brew install libev (macOS)");

    // 编译 C++ Engine
    let mut build = Build::new();
    build
        .cpp(true)
        .flag("-std=c++17")
        .include("api/include")
        .include("api/src")
        .include("include")  // quiche.h
        .file("api/src/quiche_engine_api.cpp")
        .file("api/src/quiche_engine_impl.cpp")
        .file("api/src/thread_utils.cpp");

    // 添加 libev 包含路径
    for path in &libev.include_paths {
        build.include(path);
    }

    // 平台特定配置
    let target_os = std::env::var("CARGO_CFG_TARGET_OS").unwrap();
    match target_os.as_str() {
        "macos" | "ios" => {
            build.flag("-framework").flag("Security");
            build.flag("-framework").flag("Foundation");
        }
        "linux" | "android" => {
            // Linux 特定标志
        }
        "windows" => {
            // Windows 特定标志
        }
        _ => {}
    }

    // 编译
    build.compile("quiche_engine");

    // 链接 libev
    for path in &libev.link_paths {
        println!("cargo:rustc-link-search=native={}", path.display());
    }
    for lib in &libev.libs {
        println!("cargo:rustc-link-lib={}", lib);
    }

    // 平台特定链接
    match target_os.as_str() {
        "macos" | "ios" => {
            println!("cargo:rustc-link-lib=framework=Security");
            println!("cargo:rustc-link-lib=framework=Foundation");
        }
        "linux" | "android" => {
            println!("cargo:rustc-link-lib=pthread");
            println!("cargo:rustc-link-lib=dl");
            println!("cargo:rustc-link-lib=m");
        }
        "windows" => {
            println!("cargo:rustc-link-lib=ws2_32");
            println!("cargo:rustc-link-lib=userenv");
        }
        _ => {}
    }

    println!("cargo:warning=Successfully built quiche_engine (C++)");
}

/// 方案 B：使用 CMake 编译（如果喜欢 CMake）
#[cfg(feature = "cpp-engine")]
fn build_with_cmake() {
    use cmake::Config;

    let dst = Config::new("api")
        .define("CMAKE_BUILD_TYPE", "Release")
        .define("BUILD_SHARED_LIBS", "OFF")
        .build();

    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    println!("cargo:rustc-link-lib=static=quiche_engine");

    // 链接 libev 和其他依赖
    // ...
}
```

### 步骤 3: 添加 C FFI 导出（可选）

如果想将 C++ Engine 的功能暴露给 C API：

```rust
// quiche/src/ffi.rs

#[cfg(feature = "cpp-engine")]
extern "C" {
    // C++ Engine 的 C 兼容接口
    pub fn quiche_engine_new(
        host: *const c_char,
        port: *const c_char,
    ) -> *mut std::ffi::c_void;

    pub fn quiche_engine_free(engine: *mut std::ffi::c_void);

    // ... 其他函数
}

#[cfg(feature = "cpp-engine")]
#[no_mangle]
pub extern "C" fn quiche_engine_create(
    host: *const c_char,
    port: *const c_char,
) -> *mut std::ffi::c_void {
    unsafe { quiche_engine_new(host, port) }
}
```

### 步骤 4: 更新 C++ 代码以导出 C 接口

```cpp
// api/src/quiche_engine_api.cpp

// 保留现有 C++ API
namespace quiche {
    QuicheEngine::QuicheEngine(...) { /* ... */ }
}

// ⭐ 新增：C 接口（用于 Rust FFI）
extern "C" {
    void* quiche_engine_new(const char* host, const char* port) {
        try {
            auto config = quiche::ConfigMap();
            auto* engine = new quiche::QuicheEngine(host, port, config);
            return static_cast<void*>(engine);
        } catch (...) {
            return nullptr;
        }
    }

    void quiche_engine_free(void* engine) {
        if (engine) {
            delete static_cast<quiche::QuicheEngine*>(engine);
        }
    }

    // ... 其他 C 接口函数
}
```

---

## 🎯 使用体验

### 开发者视角

#### 场景 1: 只需要 Rust 核心
```bash
cargo build
# 只构建 libquiche (Rust)
```

#### 场景 2: 需要 C++ Engine
```bash
cargo build --features cpp-engine
# 自动构建 BoringSSL + libquiche + quiche_engine
```

#### 场景 3: 使用 C API
```c
#include <quiche.h>
#include <quiche_engine.h>  // C++ API

// 或使用 C 包装（如果实现了）
void* engine = quiche_engine_new("example.com", "443");
```

#### 场景 4: 使用 C++ API
```cpp
#include <quiche_engine.h>

quiche::QuicheEngine engine("example.com", "443");
engine.start();
```

---

## ⚖️ 优缺点分析

### 优点 ✅

1. **用户体验极佳**
   - 单一命令构建
   - 无需手动管理依赖
   - 与 Rust 生态集成

2. **保持架构清晰**
   - 代码物理分离（`api/` 目录）
   - 可独立开发和测试
   - 可选择性编译

3. **与现有模式一致**
   - 类似 BoringSSL 的构建方式
   - 团队熟悉的模式
   - 减少学习成本

4. **分发简单**
   - 用户只需 `cargo install`
   - 不需要分发两个包
   - crates.io 统一发布

5. **CI/CD 友好**
   - 单一构建流程
   - 容易集成测试
   - 自动化构建

### 缺点 ⚠️

1. **build.rs 复杂度增加**
   - 需要处理 C++ 编译
   - 需要查找 libev
   - 平台特定逻辑增加

2. **外部依赖 (libev)**
   - 用户需要预装 libev
   - 不同平台安装方式不同
   - 可能需要提供 vendored 版本

3. **编译时间增加**
   - C++ 编译需要时间
   - 首次构建较慢
   - 但可以通过特性门控

4. **C++ 标准库依赖**
   - 需要 C++17 编译器
   - libstdc++/libc++ 版本要求
   - 跨平台兼容性考虑

5. **调试复杂性**
   - Rust + C++ 混合调试
   - 构建错误定位困难
   - 需要更多文档

---

## 🔄 与原方案C的对比

| 维度 | 原方案C<br>(CMake独立) | 混合方案<br>(Cargo统一) |
|------|----------------------|----------------------|
| **构建命令** | `cmake + make` 两步 | `cargo build` 一步 ✅ |
| **用户体验** | 需要理解两套系统 | 只需理解 Cargo ✅ |
| **架构分离** | ✅ 完全独立 | ✅ 物理分离 |
| **依赖管理** | 手动 | Cargo 统一 ✅ |
| **分发方式** | 两个包 | 一个包 ✅ |
| **构建复杂度** | 🟢 低 | 🟡 中 |
| **调试难度** | 🟢 低（独立） | 🟡 中（混合） |
| **CI/CD** | 需要两套流程 | 一套流程 ✅ |
| **开发灵活性** | ✅ 高（可独立） | 🟡 中（需特性门控） |
| **社区接受度** | ✅ 高 | ✅ 高 |
| **总评** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |

---

## 🚧 实施挑战和解决方案

### 挑战 1: libev 依赖

**问题**：用户需要预装 libev，不同平台安装方式不同

**解决方案 A**：提供清晰的安装指南
```markdown
# Ubuntu/Debian
sudo apt-get install libev-dev

# macOS
brew install libev

# Fedora/RHEL
sudo yum install libev-devel

# Windows
vcpkg install libev
```

**解决方案 B**：Vendored libev（可选）
```toml
[features]
cpp-engine = []
cpp-engine-vendored = ["cpp-engine"]  # 包含 vendored libev
```

```rust
#[cfg(feature = "cpp-engine-vendored")]
fn build_vendored_libev() {
    // 类似构建 BoringSSL 的方式
    let libev_dir = cmake::Config::new("deps/libev")
        .build();
    println!("cargo:rustc-link-search=native={}/lib", libev_dir.display());
    println!("cargo:rustc-link-lib=static=ev");
}
```

### 挑战 2: C++ 编译器要求

**问题**：需要 C++17 编译器

**解决方案**：
```rust
fn check_cpp_compiler() {
    let compiler = cc::Build::new()
        .cpp(true)
        .get_compiler();

    // 检查 C++17 支持
    if !compiler.is_like_clang() && !compiler.is_like_gnu() {
        println!("cargo:warning=C++17 compiler required for cpp-engine feature");
        println!("cargo:warning=Please install GCC 7+ or Clang 5+");
    }
}
```

### 挑战 3: 跨平台构建

**问题**：不同平台编译标志不同

**解决方案**：参考 BoringSSL 的做法
```rust
fn configure_platform_specific() {
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap();

    match (target_os.as_str(), target_arch.as_str()) {
        ("android", "aarch64") => configure_android_aarch64(),
        ("android", "armv7") => configure_android_armv7(),
        ("ios", "aarch64") => configure_ios_aarch64(),
        ("macos", _) => configure_macos(),
        ("linux", _) => configure_linux(),
        ("windows", _) => configure_windows(),
        _ => {}
    }
}
```

### 挑战 4: 增量编译

**问题**：每次修改都重新编译所有 C++ 代码

**解决方案**：使用 cc crate 的增量编译特性
```rust
fn build_with_cc() {
    cc::Build::new()
        .cpp(true)
        .flag("-std=c++17")
        // cc crate 自动处理增量编译
        .file("api/src/quiche_engine_api.cpp")
        .file("api/src/quiche_engine_impl.cpp")
        .file("api/src/thread_utils.cpp")
        .compile("quiche_engine");
}
```

---

## 📋 实施检查清单

### 阶段 1: 基础设施 (1 天)
- [ ] 移动代码到 `api/` 目录（与方案C相同）
- [ ] 更新 `Cargo.toml` 添加 `cpp-engine` 特性
- [ ] 准备 `build.rs` 扩展
- [ ] 测试基础构建

### 阶段 2: build.rs 实现 (2 天)
- [ ] 实现 `build_cpp_engine()` 函数
- [ ] 实现 libev 查找逻辑
- [ ] 实现平台特定配置
- [ ] 测试各平台编译

### 阶段 3: C FFI 接口（可选）(1 天)
- [ ] 实现 C 包装函数
- [ ] 更新 `ffi.rs`
- [ ] 测试 C API

### 阶段 4: 测试和文档 (2 天)
- [ ] 单元测试
- [ ] 集成测试
- [ ] 更新文档
- [ ] CI/CD 配置

### 阶段 5: 优化和发布 (1 天)
- [ ] 性能测试
- [ ] 增量编译优化
- [ ] CHANGELOG
- [ ] 发布准备

**总计**: 7 个工作日（与方案C相同）

---

## 🎯 推荐决策

### 情况 1: 简单项目，团队小
**推荐**: 原方案C (CMake独立)
- 构建系统简单
- 调试容易
- 维护成本低

### 情况 2: 需要广泛分发，用户体验优先
**推荐**: 混合方案 (Cargo统一) ✨
- 用户只需 `cargo install`
- 无需理解多套构建系统
- crates.io 统一发布

### 情况 3: 企业内部使用
**推荐**: 原方案C (CMake独立)
- 更灵活
- 可独立版本管理
- 适合自定义构建流程

---

## 💡 建议

**我的推荐**：

1. **先实施原方案C**（CMake独立）
   - 快速验证可行性
   - 架构清晰
   - 降低风险

2. **后续迁移到混合方案**（如果需要）
   - 在 v0.2.0 引入 `cpp-engine` 特性
   - 保持 CMake 构建作为备选
   - 渐进式迁移

3. **提供两种构建方式**
   ```bash
   # 方式 1: Cargo (推荐给最终用户)
   cargo build --features cpp-engine

   # 方式 2: CMake (推荐给开发者)
   cd api && mkdir build && cd build && cmake .. && make
   ```

---

## 📊 最终评分

| 方案 | 评分 | 适用场景 |
|------|------|---------|
| 原方案A (核心集成) | 1.7/10 ❌ | 不推荐 |
| 原方案B (可选特性) | 5.0/10 🟡 | 复杂场景 |
| **原方案C (CMake独立)** | **9.1/10 ✅** | **通用场景** |
| **混合方案 (Cargo统一)** | **9.5/10 ⭐** | **用户体验优先** |

---

## 🚀 快速决策

### 如果您的目标是：

**✅ 快速实施、低风险、易维护**
→ 选择**原方案C** (CMake独立)

**✅ 最佳用户体验、统一构建、crates.io发布**
→ 选择**混合方案** (Cargo统一)

**✅ 两者都要**
→ 先实施原方案C，再添加混合方案作为特性

---

## 📞 结论

**混合方案是可行的**，而且用户体验更好。它结合了：
- ✅ 方案C的架构清晰
- ✅ 方案B的特性门控
- ✅ BoringSSL的构建模式
- ✅ 一键构建的便利性

**实施建议**：
1. **阶段1**: 实施原方案C，验证可行性
2. **阶段2**: 添加 `cpp-engine` 特性到 build.rs
3. **阶段3**: 保持两种构建方式并存

这样既保险又灵活！

---

**版本**: 1.0
**日期**: 2025-11-06
**状态**: 可选方案
