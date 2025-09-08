#!/bin/bash

# 快速部署模型保存修复的脚本
# 只重新编译和部署 native 库

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_header() {
    echo "========================================"
    echo "$1"
    echo "========================================"
}

# 检查先决条件
check_prerequisites() {
    log_info "检查先决条件..."
    
    # 检查 adb
    if ! command -v adb &> /dev/null; then
        log_error "adb 未找到，请确保 Android SDK 已安装"
        exit 1
    fi
    
    # 检查设备连接
    if ! adb devices | grep -q "device$"; then
        log_error "未检测到 Android 设备"
        exit 1
    fi
    
    # 检查 NDK
    if [ -z "$ANDROID_NDK_HOME" ] && [ -z "$NDK_HOME" ]; then
        log_error "未设置 ANDROID_NDK_HOME 或 NDK_HOME 环境变量"
        exit 1
    fi
    
    log_success "先决条件检查通过"
}

# 编译 native 库
build_native() {
    log_header "编译 Native 库"
    
    cd native
    
    # 清理之前的构建
    log_info "清理之前的构建..."
    if [ -d "project/obj" ]; then
        rm -rf project/obj
    fi
    if [ -d "project/libs" ]; then
        rm -rf project/libs
    fi
    
    # 编译
    log_info "开始编译 native 库..."
    if ndk-build -C project; then
        log_success "Native 库编译成功"
    else
        log_error "Native 库编译失败"
        exit 1
    fi
    
    cd ..
}

# 部署 native 库
deploy_native() {
    log_header "部署 Native 库"
    
    # 获取设备架构
    local abi=$(adb shell getprop ro.product.cpu.abi | tr -d '\r\n')
    log_info "设备架构: $abi"
    
    # 确定库文件路径
    local lib_path="native/project/libs/${abi}/libfastbot_native.so"
    
    if [ ! -f "$lib_path" ]; then
        log_error "未找到编译后的库文件: $lib_path"
        exit 1
    fi
    
    # 推送库文件
    log_info "推送 native 库到设备..."
    if adb push "$lib_path" "/data/local/tmp/${abi}/libfastbot_native.so"; then
        log_success "Native 库部署成功"
    else
        log_error "Native 库部署失败"
        exit 1
    fi
    
    # 设置权限
    adb shell chmod 755 "/data/local/tmp/${abi}/libfastbot_native.so"
}

# 验证部署
verify_deployment() {
    log_header "验证部署"
    
    # 检查库文件是否存在
    local abi=$(adb shell getprop ro.product.cpu.abi | tr -d '\r\n')
    local remote_lib="/data/local/tmp/${abi}/libfastbot_native.so"
    
    if adb shell "test -f $remote_lib"; then
        local size=$(adb shell "stat -c%s $remote_lib" | tr -d '\r\n')
        log_success "库文件验证成功，大小: $size 字节"
    else
        log_error "库文件验证失败"
        exit 1
    fi
}

# 主函数
main() {
    log_header "模型保存修复 - 快速部署"
    
    local start_time=$(date +%s)
    
    check_prerequisites
    echo ""
    
    build_native
    echo ""
    
    deploy_native
    echo ""
    
    verify_deployment
    echo ""
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    log_header "部署完成"
    log_success "总耗时: ${duration} 秒"
    echo ""
    log_info "修复内容:"
    echo "  ✓ 后台保存间隔从10分钟改为2分钟"
    echo "  ✓ 添加了强制保存模型的方法"
    echo "  ✓ 在测试结束时强制保存模型"
    echo "  ✓ 改进了模型保存的日志输出"
    echo ""
    log_info "现在可以运行测试验证修复效果:"
    echo "  ./test_model_save_fix.sh"
    echo ""
}

# 错误处理
cleanup() {
    if [ $? -ne 0 ]; then
        log_error "部署过程中发生错误"
    fi
}

trap cleanup EXIT

# 执行主函数
main "$@"
