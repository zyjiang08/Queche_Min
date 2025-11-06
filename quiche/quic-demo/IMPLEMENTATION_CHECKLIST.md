# Quiche Engine 独立库实施检查清单

## 📋 项目信息
- **方案**: 方案 C - 独立库模式
- **预计工期**: 7 个工作日
- **开始日期**: ___________
- **目标完成日期**: ___________

---

## 阶段 1: 准备工作 (1 天)

### 1.1 目录结构创建
- [ ] 创建 `quiche/api/` 主目录
- [ ] 创建 `quiche/api/include/` (公共头文件)
- [ ] 创建 `quiche/api/src/` (实现文件)
- [ ] 创建 `quiche/api/cmake/` (CMake 模块)
- [ ] 创建 `quiche/api/tests/` (测试文件)
- [ ] 创建 `quiche/api/examples/` (示例代码)
- [ ] 创建 `quiche/api/docs/` (文档)

### 1.2 构建配置文件
- [ ] 创建 `CMakeLists.txt`
  - [ ] 项目基本信息
  - [ ] C++17 标准设置
  - [ ] 查找依赖（libquiche, libev）
  - [ ] 编译目标定义
  - [ ] 安装规则
  - [ ] 导出配置
- [ ] 创建 `quiche_engine_config.h.in`
  - [ ] 版本宏定义
  - [ ] 平台检测宏
  - [ ] 特性检测宏
- [ ] 创建 `quiche_engine.pc.in` (pkg-config)
  - [ ] 依赖声明
  - [ ] 编译标志
  - [ ] 链接标志

### 1.3 文档准备
- [ ] 创建 `README.md`
  - [ ] 项目简介
  - [ ] 功能特性
  - [ ] 依赖列表
  - [ ] 快速开始
- [ ] 创建 `API.md`
  - [ ] API 参考文档
  - [ ] 使用示例
- [ ] 创建 `BUILD.md`
  - [ ] 构建说明（各平台）
  - [ ] 依赖安装指南
  - [ ] 故障排除

### 1.4 版本控制
- [ ] 更新 `.gitignore`
  - [ ] 添加 `api/build/`
  - [ ] 添加 CMake 临时文件
- [ ] 创建 `CHANGELOG.md`
  - [ ] v0.1.0 初始版本说明
- [ ] 准备 Git 标签策略

---

## 阶段 2: 迁移代码 (1 天)

### 2.1 移动源文件
- [ ] 移动头文件
  ```bash
  mv quiche/examples/quic-demo/src/quiche_engine.h \
     quiche/api/include/quiche_engine.h
  ```
- [ ] 移动实现文件
  ```bash
  mv quiche/examples/quic-demo/src/quiche_engine_impl.h \
     quiche/api/src/
  mv quiche/examples/quic-demo/src/quiche_engine_impl.cpp \
     quiche/api/src/
  mv quiche/examples/quic-demo/src/quiche_engine_api.cpp \
     quiche/api/src/
  ```
- [ ] 移动工具文件
  ```bash
  mv quiche/examples/quic-demo/src/thread_utils.h \
     quiche/api/src/
  mv quiche/examples/quic-demo/src/thread_utils.cpp \
     quiche/api/src/
  ```

### 2.2 调整头文件包含路径
- [ ] 更新 `quiche_engine_impl.h`
  ```cpp
  #include "quiche_engine.h"  →  #include <quiche_engine.h>
  ```
- [ ] 更新 `quiche_engine_api.cpp`
  ```cpp
  #include "quiche_engine_impl.h"  →  #include "quiche_engine_impl.h"
  ```
- [ ] 更新 `quiche_engine_impl.cpp`
  ```cpp
  #include "thread_utils.h"  →  #include "thread_utils.h"
  ```
- [ ] 验证所有 `#include` 路径正确

### 2.3 应用代码审查改进
- [ ] 添加错误码枚举 (EngineError)
  ```cpp
  enum class EngineError {
      SUCCESS = 0,
      NOT_INITIALIZED = -1,
      NOT_CONNECTED = -2,
      INVALID_STREAM = -3,
      // ...
  };
  ```
- [ ] 改进异常处理（捕获所有异常类型）
- [ ] 使用 unique_ptr 包装 C 资源
  ```cpp
  struct QuicheConfigDeleter { /* ... */ };
  std::unique_ptr<quiche_config, QuicheConfigDeleter> mQuicheCfg;
  ```
- [ ] 修改 WriteData 使用堆分配
  ```cpp
  struct WriteData {
      uint64_t stream_id;
      std::vector<uint8_t> data;  // 代替固定数组
      bool fin;
  };
  ```

### 2.4 添加版本信息
- [ ] 在 `quiche_engine.h` 添加版本宏
  ```cpp
  #define QUICHE_ENGINE_VERSION_MAJOR 0
  #define QUICHE_ENGINE_VERSION_MINOR 1
  #define QUICHE_ENGINE_VERSION_PATCH 0
  #define QUICHE_ENGINE_VERSION "0.1.0"
  ```
- [ ] 添加版本查询 API
  ```cpp
  const char* getEngineVersion();
  ```

---

## 阶段 3: 构建系统 (2 天)

### 3.1 CMake 配置实现
- [ ] 实现项目基本信息
  ```cmake
  cmake_minimum_required(VERSION 3.15)
  project(quiche_engine VERSION 0.1.0 LANGUAGES CXX)
  ```
- [ ] 实现 C++17 标准要求
- [ ] 实现 libev 查找逻辑
  ```cmake
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBEV REQUIRED libev)
  ```
- [ ] 实现 libquiche 查找逻辑
  ```cmake
  find_library(QUICHE_LIB quiche ...)
  find_path(QUICHE_INCLUDE quiche.h ...)
  ```
- [ ] 实现编译目标
  ```cmake
  add_library(quiche_engine STATIC src/*.cpp)
  ```
- [ ] 实现头文件目录配置
  ```cmake
  target_include_directories(...)
  ```
- [ ] 实现链接库配置
  ```cmake
  target_link_libraries(...)
  ```

### 3.2 平台特定配置
- [ ] Linux 配置
  - [ ] pthread 链接
  - [ ] dl 库链接
  - [ ] m 库链接
- [ ] macOS 配置
  - [ ] Security framework
  - [ ] Foundation framework
  - [ ] 检测 Xcode 版本
- [ ] iOS 配置
  - [ ] Bitcode 支持
  - [ ] 架构检测（arm64, x86_64）
  - [ ] SDK 路径配置
- [ ] Android 配置
  - [ ] NDK 路径检测
  - [ ] ABI 配置
  - [ ] API Level 设置
- [ ] Windows 配置
  - [ ] ws2_32 链接
  - [ ] userenv 链接
  - [ ] MSVC 特定标志

### 3.3 安装和导出
- [ ] 实现安装目标
  ```cmake
  install(TARGETS quiche_engine ...)
  ```
- [ ] 实现头文件安装
  ```cmake
  install(FILES include/quiche_engine.h ...)
  ```
- [ ] 实现 CMake 配置导出
  ```cmake
  install(EXPORT quiche_engine-targets ...)
  ```
- [ ] 实现 pkg-config 生成
  ```cmake
  configure_file(quiche_engine.pc.in ...)
  ```

### 3.4 构建测试
- [ ] Linux 构建测试
  - [ ] GCC 7+
  - [ ] GCC 11+
  - [ ] Clang 10+
  - [ ] Clang 14+
- [ ] macOS 构建测试
  - [ ] Apple Clang 12+
  - [ ] Apple Clang 14+
- [ ] 静态库构建测试
- [ ] 动态库构建测试（可选）
- [ ] 验证安装文件完整性

---

## 阶段 4: 更新示例和测试 (2 天)

### 4.1 更新示例程序
- [ ] 更新 `examples/quic-demo/Makefile`
  ```makefile
  QUICHE_ENGINE_INCLUDE = ../../api/include
  QUICHE_ENGINE_SRC = ../../api/src
  INCLUDES = -I$(QUICHE_ENGINE_INCLUDE) ...
  ```
- [ ] 更新 `examples/quic-demo/src/client.cpp`
  - [ ] 修改 include 路径
  - [ ] 验证编译通过
- [ ] 创建 CMake 示例
  ```cmake
  find_package(quiche_engine REQUIRED)
  add_executable(demo demo.cpp)
  target_link_libraries(demo quiche::quiche_engine)
  ```

### 4.2 单元测试
- [ ] 选择测试框架（Google Test 或 Catch2）
- [ ] 创建 `tests/CMakeLists.txt`
- [ ] 测试: 引擎初始化
  ```cpp
  TEST(QuicheEngine, Initialization) { /* ... */ }
  ```
- [ ] 测试: 配置参数
  ```cpp
  TEST(QuicheEngine, Configuration) { /* ... */ }
  ```
- [ ] 测试: 连接建立（需要 mock）
- [ ] 测试: 数据读写
- [ ] 测试: 错误处理
- [ ] 测试: 线程安全
- [ ] 测试: 资源清理
- [ ] 运行所有测试并验证通过

### 4.3 集成测试
- [ ] 测试: 客户端-服务器通信
- [ ] 测试: 大数据传输
- [ ] 测试: 并发连接
- [ ] 测试: 异常断开
- [ ] 测试: 重连机制
- [ ] 性能基准测试
  - [ ] 吞吐量测试
  - [ ] 延迟测试
  - [ ] CPU 使用率
  - [ ] 内存使用量
- [ ] 内存泄漏检测
  - [ ] Valgrind (Linux)
  - [ ] Instruments (macOS)
  - [ ] AddressSanitizer

### 4.4 跨平台测试
- [ ] Ubuntu 20.04 LTS 测试
- [ ] Ubuntu 22.04 LTS 测试
- [ ] CentOS 7/8 测试
- [ ] macOS 12+ 测试
- [ ] iOS 13+ 测试（真机/模拟器）
- [ ] Android NDK r21+ 测试
- [ ] Windows 10/11 测试（可选）
- [ ] FreeBSD 测试（可选）

---

## 阶段 5: 文档和发布 (1 天)

### 5.1 API 文档
- [ ] `quiche_engine.h` 头文件注释
  - [ ] Doxygen 格式注释
  - [ ] 每个类的说明
  - [ ] 每个方法的说明
  - [ ] 参数和返回值说明
  - [ ] 使用示例
- [ ] 生成 API 文档（Doxygen）
  ```bash
  doxygen Doxyfile
  ```

### 5.2 用户文档
- [ ] `README.md` 完善
  - [ ] 快速开始教程
  - [ ] 完整示例代码
  - [ ] 常见问题 FAQ
- [ ] `BUILD.md` 完善
  - [ ] 各平台构建详细步骤
  - [ ] 依赖安装指南
    - [ ] apt-get (Ubuntu/Debian)
    - [ ] yum (CentOS/RHEL)
    - [ ] brew (macOS)
    - [ ] vcpkg (Windows)
  - [ ] 故障排除指南
- [ ] `API.md` 完善
  - [ ] 完整 API 参考
  - [ ] 使用最佳实践
  - [ ] 性能优化建议
- [ ] `MIGRATION.md`
  - [ ] 从 examples 迁移到 api 的指南

### 5.3 开发者文档
- [ ] `CONTRIBUTING.md`
  - [ ] 代码规范
  - [ ] 提交规范
  - [ ] Pull Request 流程
- [ ] `ARCHITECTURE.md`
  - [ ] 架构设计说明
  - [ ] 线程模型
  - [ ] 内存管理策略

### 5.4 发布准备
- [ ] 更新 `CHANGELOG.md`
  ```markdown
  ## [0.1.0] - 2025-11-XX
  ### Added
  - Initial release
  - C++17 high-level API
  - Cross-platform thread naming
  - Event loop integration (libev)
  ```
- [ ] 创建 GitHub Release
  - [ ] 标签: `v0.1.0`
  - [ ] 标题: `Quiche Engine v0.1.0 - Initial Release`
  - [ ] 发布说明（从 CHANGELOG 复制）
- [ ] 打包发布文件
  - [ ] 源码 tarball
  - [ ] 预编译库（可选）
  - [ ] 示例代码
- [ ] 更新主项目 README
  - [ ] 添加 quiche-engine 链接
  - [ ] 说明高层 C++ API 可用

---

## 验收标准 ✅

### 编译
- [ ] Linux GCC 编译通过
- [ ] Linux Clang 编译通过
- [ ] macOS 编译通过
- [ ] iOS 编译通过（可选）
- [ ] Android 编译通过（可选）
- [ ] Windows 编译通过（可选）
- [ ] 无编译警告（-Wall -Wextra）

### 测试
- [ ] 所有单元测试通过
- [ ] 所有集成测试通过
- [ ] 测试覆盖率 > 80%
- [ ] 无内存泄漏
- [ ] 无数据竞争（ThreadSanitizer）
- [ ] 性能开销 < 5%

### 文档
- [ ] API 文档完整
- [ ] 用户文档清晰
- [ ] 构建说明详细
- [ ] 示例代码可运行
- [ ] 拼写检查通过

### 质量
- [ ] 代码审查通过
- [ ] 符合 C++ Core Guidelines
- [ ] 符合项目编码规范
- [ ] 无已知安全问题
- [ ] License 文件正确

---

## 后续工作 (v0.2.0+)

### 性能优化
- [ ] 对象池实现
- [ ] 零拷贝优化
- [ ] 使用 shared_mutex
- [ ] SIMD 优化（可选）

### 功能扩展
- [ ] HTTP/3 支持
- [ ] Datagram 支持
- [ ] 多路径 QUIC
- [ ] 0-RTT 支持

### 工具和生态
- [ ] Conan 包管理支持
- [ ] vcpkg 包管理支持
- [ ] Bazel 构建支持
- [ ] Docker 镜像

---

## 风险和缓解措施

### 风险 1: 依赖查找失败
- **概率**: 中
- **影响**: 高
- **缓解**: 提供详细的依赖安装指南，支持手动指定路径

### 风险 2: 跨平台兼容性问题
- **概率**: 中
- **影响**: 中
- **缓解**: 早期进行跨平台测试，准备 CI/CD 流水线

### 风险 3: 性能回归
- **概率**: 低
- **影响**: 中
- **缓解**: 建立性能基准测试，持续监控

### 风险 4: API 设计不满足需求
- **概率**: 低
- **影响**: 高
- **缓解**: 收集用户反馈，预留 API 扩展点

---

## 进度跟踪

| 阶段 | 计划天数 | 实际天数 | 完成率 | 状态 |
|------|---------|---------|--------|------|
| 阶段 1: 准备 | 1 | ___ | ___% | ⬜️ 未开始 |
| 阶段 2: 迁移 | 1 | ___ | ___% | ⬜️ 未开始 |
| 阶段 3: 构建 | 2 | ___ | ___% | ⬜️ 未开始 |
| 阶段 4: 测试 | 2 | ___ | ___% | ⬜️ 未开始 |
| 阶段 5: 文档 | 1 | ___ | ___% | ⬜️ 未开始 |
| **总计** | **7** | **___** | **___%** | |

**状态图例**:
- ⬜️ 未开始
- 🟦 进行中
- ✅ 已完成
- ⚠️ 有问题
- ❌ 已阻塞

---

## 团队和资源

### 负责人
- **项目负责人**: ___________
- **技术负责人**: ___________

### 开发人员
- **核心开发**: ___________
- **测试工程师**: ___________
- **文档编写**: ___________

### 外部资源
- **CI/CD 系统**: GitHub Actions / GitLab CI
- **代码托管**: GitHub
- **问题跟踪**: GitHub Issues
- **文档托管**: GitHub Pages / Read the Docs

---

## 签署

| 角色 | 姓名 | 签名 | 日期 |
|------|------|------|------|
| 项目经理 | | | |
| 技术负责人 | | | |
| 质量保证 | | | |

---

**文档版本**: 1.0
**创建日期**: 2025-11-06
**最后更新**: 2025-11-06
