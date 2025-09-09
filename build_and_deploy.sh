#!/bin/bash

# Fastbot 编译和部署脚本
# 作者: AI Assistant
# 日期: 2025-07-16

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

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

# 检查必要的工具
check_prerequisites() {
    log_info "检查必要的工具..."
    
    # 检查 adb
    if ! command -v adb &> /dev/null; then
        log_error "adb 未找到，请确保 Android SDK 已安装并添加到 PATH"
        exit 1
    fi
    
    # 检查设备连接
    if ! adb devices | grep -q "device$"; then
        log_error "未检测到 Android 设备，请确保设备已连接并启用 USB 调试"
        exit 1
    fi
    
    # 检查 dx 工具
    DX_PATH="/Users/atmo/Library/Android/sdk/build-tools/30.0.2/dx"
    if [ ! -f "$DX_PATH" ]; then
        log_error "dx 工具未找到: $DX_PATH"
        log_error "请检查 Android SDK build-tools 是否已安装"
        exit 1
    fi
    
    # 检查 gradlew
    if [ ! -f "./gradlew" ]; then
        log_error "gradlew 未找到，请确保在项目根目录执行此脚本"
        exit 1
    fi
    
    # 检查 build_native.sh
    if [ ! -f "./build_native.sh" ]; then
        log_error "build_native.sh 未找到，请确保在项目根目录执行此脚本"
        exit 1
    fi
    
    log_success "所有必要工具检查通过"
}

# 步骤1: Gradle 构建
step1_gradle_build() {
    log_info "步骤 1/7: 执行 Gradle 构建..."
    ./gradlew clean makeJar
    
    if [ $? -eq 0 ]; then
        log_success "Gradle 构建完成"
    else
        log_error "Gradle 构建失败"
        exit 1
    fi
}

# 步骤2: DEX 转换
step2_dex_conversion() {
    log_info "步骤 2/7: 执行 DEX 转换..."
    
    # 检查输入文件是否存在
    if [ ! -f "monkey/build/libs/monkey.jar" ]; then
        log_error "monkey.jar 未找到，请确保 Gradle 构建成功"
        exit 1
    fi
    
    /Users/atmo/Library/Android/sdk/build-tools/30.0.2/dx --dex --output=monkeyq.jar monkey/build/libs/monkey.jar
    
    if [ $? -eq 0 ] && [ -f "monkeyq.jar" ]; then
        log_success "DEX 转换完成"
    else
        log_error "DEX 转换失败"
        exit 1
    fi
}

# 步骤3: 原生库编译
step3_native_build() {
    log_info "步骤 3/7: 编译原生库..."
    sh ./build_native.sh
    
    if [ $? -eq 0 ]; then
        log_success "原生库编译完成"
    else
        log_error "原生库编译失败"
        exit 1
    fi
}

# 步骤4: 推送 monkeyq.jar
step4_push_monkeyq() {
    log_info "步骤 4/7: 推送 monkeyq.jar..."
    
    if [ ! -f "monkeyq.jar" ]; then
        log_error "monkeyq.jar 未找到"
        exit 1
    fi
    
    adb push monkeyq.jar /sdcard/monkeyq.jar
    
    if [ $? -eq 0 ]; then
        log_success "monkeyq.jar 推送完成"
    else
        log_error "monkeyq.jar 推送失败"
        exit 1
    fi
}

# 步骤5: 推送第三方库
step5_push_thirdpart() {
    log_info "步骤 5/7: 推送第三方库..."
    
    if [ ! -f "fastbot-thirdpart.jar" ]; then
        log_error "fastbot-thirdpart.jar 未找到"
        exit 1
    fi
    
    adb push fastbot-thirdpart.jar /sdcard/fastbot-thirdpart.jar
    
    if [ $? -eq 0 ]; then
        log_success "第三方库推送完成"
    else
        log_error "第三方库推送失败"
        exit 1
    fi
}

# 步骤6: 推送原生库
step6_push_libs() {
    log_info "步骤 6/7: 推送原生库..."
    
    if [ ! -d "libs" ]; then
        log_error "libs 目录未找到"
        exit 1
    fi
    
    adb push libs/* /data/local/tmp/
    
    if [ $? -eq 0 ]; then
        log_success "原生库推送完成"
    else
        log_error "原生库推送失败"
        exit 1
    fi
    
    # 推送词汇表文件
    log_info "推送BERT词汇表文件..."
    if [ -f "vocab.txt" ]; then
        adb push vocab.txt /data/local/tmp/vocab.txt
        if [ $? -eq 0 ]; then
            log_success "词汇表文件推送完成"
        else
            log_error "词汇表文件推送失败"
            exit 1
        fi
    else
        log_error "vocab.txt 文件未找到"
        exit 1
    fi
}

# 步骤7: 推送 framework.jar
step7_push_framework() {
    log_info "步骤 7/7: 推送 framework.jar..."
    
    if [ ! -f "framework.jar" ]; then
        log_error "framework.jar 未找到"
        exit 1
    fi
    
    adb push framework.jar /sdcard/framework.jar
    
    if [ $? -eq 0 ]; then
        log_success "framework.jar 推送完成"
    else
        log_error "framework.jar 推送失败"
        exit 1
    fi
}

# 清理函数
cleanup() {
    log_info "清理临时文件..."
    # 可以在这里添加清理逻辑，比如删除临时文件
}

# 主函数
main() {
    echo "========================================"
    echo "    Fastbot 编译和部署脚本"
    echo "========================================"
    echo ""
    
    # 记录开始时间
    START_TIME=$(date +%s)
    
    # 检查先决条件
    check_prerequisites
    
    echo ""
    log_info "开始编译和部署流程..."
    echo ""
    
    # 执行所有步骤
    step1_gradle_build
    echo ""
    
    step2_dex_conversion
    echo ""
    
    step3_native_build
    echo ""
    
    step4_push_monkeyq
    echo ""
    
    step5_push_thirdpart
    echo ""
    
    step6_push_libs
    echo ""
    
    step7_push_framework
    echo ""
    
    # 计算总耗时
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    
    echo "========================================"
    log_success "所有步骤完成！总耗时: ${DURATION} 秒"
    echo "========================================"
    echo ""
    log_info "现在可以运行 Fastbot 测试了！"
    echo ""
}

# 捕获中断信号
trap cleanup EXIT

# 执行主函数
main "$@"
