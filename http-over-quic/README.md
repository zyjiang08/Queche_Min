# HTTP-over-QUIC

基于 Cloudflare quiche 的 HTTP/1.1 over QUIC 文件传输服务实现。

## 快速开始

### 编译
```bash
make all
```

### 运行服务器
```bash
./http-server 127.0.0.1 8443
```

### 下载文件
```bash
./http-client 127.0.0.1 8443 /test.bin output.bin
```

## 项目特点

✅ **完整的 QUIC 支持**
- TLS 1.3 加密连接
- 基于 Cloudflare quiche 库
- 支持自签名证书

✅ **HTTP/1.1 协议**
- GET 请求处理
- 标准 HTTP 头部
- 文件服务功能
- SHA256 完整性验证

✅ **双语言实现**
- 服务器: C 语言 (直接使用 quiche C API)
- 客户端: C++ (使用 QuicheEngine 封装)

## 详细文档

完整的测试指南和技术细节请参考 **[TEST_GUIDE.md](TEST_GUIDE.md)**。

## 许可证

本项目基于 Cloudflare quiche，遵循 BSD-2-Clause 许可证。
