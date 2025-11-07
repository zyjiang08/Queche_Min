# Quiche Engine 快速参考

## 一行命令编译

```bash
# iOS
./quiche_engine_all.sh ios arm64        # 真机
./quiche_engine_all.sh ios x86_64       # 模拟器
./quiche_engine_all.sh ios all          # 所有

# macOS
./quiche_engine_all.sh macos arm64      # Apple Silicon
./quiche_engine_all.sh macos x86_64     # Intel
./quiche_engine_all.sh macos all        # 所有

# Android (需要先设置 ANDROID_NDK_HOME)
export ANDROID_NDK_HOME=/path/to/ndk
./quiche_engine_all.sh android arm64-v8a    # ARM64
./quiche_engine_all.sh android all          # 所有

# 所有平台
./quiche_engine_all.sh all
```

## 产物路径

```
lib/
├── ios/arm64/libquiche_engine.a
├── macos/arm64/libquiche_engine.a
└── android/arm64-v8a/libquiche_engine.so

include/
└── quiche_engine.h
```

## 多平台编译

```bash
./quiche_engine_all.sh ios arm64 android arm64-v8a
./quiche_engine_all.sh ios all android all
```

## 集成到项目

### iOS (Xcode)
- 链接: `lib/ios/arm64/libquiche_engine.a`
- 头文件: `include/`

### Android (CMake)
```cmake
add_library(quiche_engine SHARED IMPORTED)
set_target_properties(quiche_engine PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/android/${ANDROID_ABI}/libquiche_engine.so
)
include_directories(${CMAKE_SOURCE_DIR}/include)
target_link_libraries(your_target quiche_engine)
```

## 详细文档

- 📘 编译指南: [BUILD_GUIDE.md](BUILD_GUIDE.md)
- 📗 变更说明: [CHANGES.md](CHANGES.md)
- 📙 构建说明: [README_BUILD.md](README_BUILD.md)
