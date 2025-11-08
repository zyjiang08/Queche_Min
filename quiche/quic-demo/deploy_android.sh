#!/bin/bash
# Android QUIC Client 部署脚本
# 将quic-client和所需库推送到Android设备

set -e

# 颜色输出
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

echo_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

echo_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 配置路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR="${SCRIPT_DIR}/../../lib/android/arm64-v8a"
ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-/Users/jiangzhongyang/Library/Android/sdk/ndk/23.2.8568313}"
NDK_LIBCXX="${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"

# 设备目标路径
DEVICE_DIR="/data/local/tmp/quiche"

# 检查ADB
if ! command -v adb &> /dev/null; then
    echo_error "adb not found in PATH"
    echo_info "Please install Android SDK platform-tools"
    exit 1
fi

# 检查设备连接
if ! adb devices | grep -q "device$"; then
    echo_error "No Android device connected"
    echo_info "Please connect an Android device and enable USB debugging"
    exit 1
fi

echo_info "============================================"
echo_info "Android QUIC Client 部署脚本"
echo_info "============================================"
echo ""

# 检查文件存在
echo_info "检查必需文件..."
if [ ! -f "quic-client-android" ]; then
    echo_error "quic-client-android not found"
    echo_info "Please run: make -f Makefile.android"
    exit 1
fi

if [ ! -f "${LIB_DIR}/libquiche_engine.so" ]; then
    echo_error "libquiche_engine.so not found at ${LIB_DIR}"
    exit 1
fi

if [ ! -f "$NDK_LIBCXX" ]; then
    echo_warn "libc++_shared.so not found at $NDK_LIBCXX"
    echo_warn "Looking for alternative location..."
    # 尝试其他可能的位置
    ALT_NDK_LIBCXX="${ANDROID_NDK_HOME}/sources/cxx-stl/llvm-libc++/libs/arm64-v8a/libc++_shared.so"
    if [ -f "$ALT_NDK_LIBCXX" ]; then
        NDK_LIBCXX="$ALT_NDK_LIBCXX"
        echo_info "Found at: $NDK_LIBCXX"
    else
        echo_error "Cannot find libc++_shared.so"
        exit 1
    fi
fi

echo_info "✓ All files found"
echo ""

# 创建设备目录
echo_info "创建设备目录: $DEVICE_DIR"
adb shell "mkdir -p $DEVICE_DIR" 2>/dev/null || true

# 推送文件
echo_info "推送文件到设备..."
echo_info "  1/3 推送 quic-client-android..."
adb push quic-client-android "$DEVICE_DIR/quic-client"

echo_info "  2/3 推送 libquiche_engine.so..."
adb push "${LIB_DIR}/libquiche_engine.so" "$DEVICE_DIR/"

echo_info "  3/3 推送 libc++_shared.so..."
adb push "$NDK_LIBCXX" "$DEVICE_DIR/"

# 设置执行权限
echo_info "设置执行权限..."
adb shell "chmod +x $DEVICE_DIR/quic-client"

# 显示文件信息
echo ""
echo_info "部署完成！文件列表:"
adb shell "ls -lh $DEVICE_DIR"

# 显示使用说明
echo ""
echo_info "============================================"
echo_info "使用方法"
echo_info "============================================"
echo ""
echo_info "运行QUIC客户端:"
echo "  adb shell \"cd $DEVICE_DIR && LD_LIBRARY_PATH=. ./quic-client <server_ip> <port>\""
echo ""
echo_info "示例:"
echo "  adb shell \"cd $DEVICE_DIR && LD_LIBRARY_PATH=. ./quic-client 192.168.1.100 4433\""
echo ""
echo_warn "注意:"
echo "  1. 必须设置 LD_LIBRARY_PATH=. 以加载共享库"
echo "  2. 使用 cd $DEVICE_DIR && ... 确保在正确的目录中运行"
echo "  3. 服务器IP和端口需要根据实际情况修改"
echo ""

# 可选：测试连接
read -p "是否现在测试运行客户端? (需要提供服务器IP和端口) [y/N]: " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    read -p "输入服务器IP: " SERVER_IP
    read -p "输入端口 (默认4433): " PORT
    PORT=${PORT:-4433}

    echo_info "连接到 $SERVER_IP:$PORT ..."
    adb shell "cd $DEVICE_DIR && LD_LIBRARY_PATH=. ./quic-client $SERVER_IP $PORT"
fi

echo ""
echo_info "部署成功完成！"
