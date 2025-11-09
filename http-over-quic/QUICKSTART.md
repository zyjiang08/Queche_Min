# HTTP-over-QUIC 快速开始

## 一分钟测试

```bash
# 1. 编译
make all

# 2. 启动服务器（终端1）
./http-server 127.0.0.1 8443

# 3. 下载文件（终端2）
./http-client 127.0.0.1 8443 /tiny.bin output.bin

# 4. 验证完整性
shasum -a 256 data/tiny.bin output.bin
```

## 成功示例

```
✓ Download completed!

=== Integrity Verification ===
  Expected SHA256:   5f70bf18a086007016e948b04aed3b82103a36bea41755b6cddfaf10ace3c6ef
  Calculated SHA256: 5f70bf18a086007016e948b04aed3b82103a36bea41755b6cddfaf10ace3c6ef
  ✓ Integrity verification PASSED

✓ Done!
```

## 更多测试

```bash
# 下载不同大小的文件
./http-client 127.0.0.1 8443 /small.bin small.bin    # 4KB
./http-client 127.0.0.1 8443 /test.bin test.bin      # 1MB (部分传输)

# 测试404错误
./http-client 127.0.0.1 8443 /nonexistent.bin test.bin
```

## 详细文档

- **完整测试指南**: [TEST_GUIDE.md](TEST_GUIDE.md)
- **项目概述**: [README.md](README.md)

## 已知限制

⚠️ 当前只能完整传输 < 2KB 的文件（技术限制，详见 TEST_GUIDE.md）
