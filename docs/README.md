# quiche 文档索引

## 📱 移动平台支持

完整的 Android 和 iOS 平台支持文档位于 [`mobile/`](mobile/) 目录。

### 快速导航

| 文档 | 描述 |
|------|------|
| **[README_MOBILE.md](mobile/README_MOBILE.md)** | 移动平台支持主页 - 从这里开始 |
| **[QUICK_START_ENGINE.md](mobile/QUICK_START_ENGINE.md)** | 5分钟快速开始指南 |
| **[BUILD_SCRIPT_USAGE.md](mobile/BUILD_SCRIPT_USAGE.md)** | 构建脚本详细使用指南 ⚡ |
| **[MOBILE_BUILD_GUIDE.md](mobile/MOBILE_BUILD_GUIDE.md)** | 完整构建指南（自动 + 手动） |
| **[MOBILE_INTEGRATION_EXAMPLE.md](mobile/MOBILE_INTEGRATION_EXAMPLE.md)** | iOS/Android 集成示例 |
| **[MOBILE_PLATFORM_SUMMARY.md](mobile/MOBILE_PLATFORM_SUMMARY.md)** | 技术实现总结 |
| **[ENGINE_WITH_VENDORED_LIBEV.md](mobile/ENGINE_WITH_VENDORED_LIBEV.md)** | 引擎架构详解 |

### iOS 特定文档

| 文档 | 描述 |
|------|------|
| **[iOS_QUICK_START.md](mobile/iOS_QUICK_START.md)** | iOS 快速开始 |
| **[iOS_BUILD_FIX.md](mobile/iOS_BUILD_FIX.md)** | iOS 构建问题修复 |
| **[iOS_CHKSTK_FIX_SUMMARY.md](mobile/iOS_CHKSTK_FIX_SUMMARY.md)** | iOS chkstk 问题总结 |

## 🚀 推荐阅读顺序

### 第一次使用移动平台？

1. 📖 **[README_MOBILE.md](mobile/README_MOBILE.md)** - 了解平台支持概况
2. ⚡ **[QUICK_START_ENGINE.md](mobile/QUICK_START_ENGINE.md)** - 5分钟快速上手
3. 🏗️ **[MOBILE_BUILD_GUIDE.md](mobile/MOBILE_BUILD_GUIDE.md)** - 详细构建步骤
4. 📱 **[MOBILE_INTEGRATION_EXAMPLE.md](mobile/MOBILE_INTEGRATION_EXAMPLE.md)** - 集成到你的应用

### 深入了解技术细节？

1. 📊 **[MOBILE_PLATFORM_SUMMARY.md](mobile/MOBILE_PLATFORM_SUMMARY.md)** - 实现架构和性能
2. 🔧 **[ENGINE_WITH_VENDORED_LIBEV.md](mobile/ENGINE_WITH_VENDORED_LIBEV.md)** - 引擎内部工作原理

### iOS 平台特定问题？

1. 🍎 **[iOS_QUICK_START.md](mobile/iOS_QUICK_START.md)** - iOS 专属快速指南
2. 🔧 **[iOS_BUILD_FIX.md](mobile/iOS_BUILD_FIX.md)** - 常见构建问题
3. 📝 **[iOS_CHKSTK_FIX_SUMMARY.md](mobile/iOS_CHKSTK_FIX_SUMMARY.md)** - 特定技术问题

## 🛠️ 构建脚本

自动化构建脚本位于项目根目录：

- **[build_mobile_libs.sh](../build_mobile_libs.sh)** - 一键构建 iOS/Android 库

```bash
# 从项目根目录运行
./build_mobile_libs.sh ios       # 构建 iOS 库
./build_mobile_libs.sh android   # 构建 Android 库
./build_mobile_libs.sh all       # 构建所有平台
```

## 📂 文档组织结构

```
docs/
├── README.md                          # 本文件 - 文档索引
└── mobile/                            # 移动平台文档
    ├── README_MOBILE.md               # 移动平台主页
    ├── QUICK_START_ENGINE.md          # 快速开始
    ├── MOBILE_BUILD_GUIDE.md          # 构建指南
    ├── MOBILE_INTEGRATION_EXAMPLE.md  # 集成示例
    ├── MOBILE_PLATFORM_SUMMARY.md     # 技术总结
    ├── ENGINE_WITH_VENDORED_LIBEV.md  # 引擎架构
    ├── iOS_QUICK_START.md             # iOS 快速开始
    ├── iOS_BUILD_FIX.md               # iOS 构建修复
    └── iOS_CHKSTK_FIX_SUMMARY.md      # iOS chkstk 修复
```

## 🆘 获取帮助

如果遇到问题：

1. 📖 查看 [MOBILE_BUILD_GUIDE.md](mobile/MOBILE_BUILD_GUIDE.md) 的常见问题部分
2. 🔍 查看相关平台的特定文档
3. 📝 提交 GitHub Issue 并附带详细信息

## 🔗 相关资源

- **主项目 README**: [../README.md](../README.md)
- **CLAUDE.md**: [../CLAUDE.md](../CLAUDE.md) - 项目指南
- **构建脚本**: [../build_mobile_libs.sh](../build_mobile_libs.sh)

---

**最后更新**: 2025-11-06
**文档版本**: 1.0
