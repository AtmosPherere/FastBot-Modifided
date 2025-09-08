#!/bin/bash

# 测试模型保存修复效果的脚本
# 进行短时间测试，验证模型是否能正确保存和加载

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# 配置参数
PACKAGE_NAME="com.netease.cloudmusic"
TEST_DURATION=3   # 3分钟测试，验证短时间内的保存
THROTTLE=600
RESULTS_DIR="model_save_test_$(date +%Y%m%d_%H%M%S)"
MODEL_FILE="/sdcard/fastbot_${PACKAGE_NAME}.fbm"

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
    echo -e "${CYAN}========================================${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}========================================${NC}"
}

# 检查设备连接
check_device() {
    if ! adb devices | grep -q "device$"; then
        log_error "未检测到 Android 设备"
        exit 1
    fi
    log_success "设备连接正常"
}

# 创建结果目录
create_results_dir() {
    mkdir -p "${RESULTS_DIR}"
    log_info "创建结果目录: ${RESULTS_DIR}"
}

# 清理旧模型
clean_old_model() {
    log_info "清理旧模型文件..."
    adb shell "rm -f ${MODEL_FILE}" 2>/dev/null || true
    log_success "旧模型文件已清理"
}

# 检查模型文件
check_model_exists() {
    if adb shell "test -f ${MODEL_FILE}" 2>/dev/null; then
        return 0
    else
        return 1
    fi
}

# 获取模型大小
get_model_size() {
    local size=$(adb shell "stat -c%s ${MODEL_FILE} 2>/dev/null" | tr -d '\r\n' || echo "0")
    echo "$size"
}

# 强制停止应用
force_stop_app() {
    log_info "停止应用..."
    adb shell am force-stop "${PACKAGE_NAME}"
    sleep 2
}

# 启动应用
start_app() {
    log_info "启动应用..."
    adb shell monkey -p "${PACKAGE_NAME}" -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1
    sleep 3
}

# 运行测试
run_test() {
    local round=$1
    local log_file="${RESULTS_DIR}/round${round}_test.log"
    
    log_header "第 ${round} 轮测试 (${TEST_DURATION} 分钟)"
    
    local test_cmd="export LD_LIBRARY_PATH=/data/local/tmp:\$LD_LIBRARY_PATH && CLASSPATH=/sdcard/monkeyq.jar:/sdcard/framework.jar:/sdcard/fastbot-thirdpart.jar exec app_process /system/bin com.android.commands.monkey.Monkey -p ${PACKAGE_NAME} --agent reuseq --running-minutes ${TEST_DURATION} --throttle ${THROTTLE} -v -v"
    
    echo "开始时间: $(date)" > "$log_file"
    echo "测试命令: $test_cmd" >> "$log_file"
    echo "=========================================" >> "$log_file"
    
    if adb shell "$test_cmd" >> "$log_file" 2>&1; then
        echo "结束时间: $(date)" >> "$log_file"
        log_success "第 ${round} 轮测试完成"
        return 0
    else
        log_error "第 ${round} 轮测试失败"
        return 1
    fi
}

# 分析模型文件内容
analyze_model_file() {
    local round=$1
    local log_file="${RESULTS_DIR}/round${round}_test.log"
    
    log_info "分析第 ${round} 轮后的模型文件..."
    
    if check_model_exists; then
        local size=$(get_model_size)
        log_success "模型文件存在，大小: $size 字节"
        
        # 备份模型文件
        local backup_file="${RESULTS_DIR}/round${round}_model.fbm"
        if adb pull "$MODEL_FILE" "$backup_file" >/dev/null 2>&1; then
            log_success "模型文件已备份到: $backup_file"
        fi
        
        # 检查日志中的保存相关信息
        if [ -f "$log_file" ]; then
            local save_count=$(grep -c "save.*model" "$log_file" 2>/dev/null || echo "0")
            local force_save_count=$(grep -c "Force.*save" "$log_file" 2>/dev/null || echo "0")
            local update_count=$(grep -c "updateReuseModel" "$log_file" 2>/dev/null || echo "0")
            
            echo "第 ${round} 轮模型分析:"
            echo "  - 模型保存次数: $save_count"
            echo "  - 强制保存次数: $force_save_count"
            echo "  - 模型更新次数: $update_count"
            echo "  - 模型文件大小: $size 字节"
        fi
        
        return 0
    else
        log_warning "第 ${round} 轮后模型文件不存在"
        return 1
    fi
}

# 验证模型加载
verify_model_loading() {
    local round=$1
    local log_file="${RESULTS_DIR}/round${round}_test.log"
    
    log_info "验证第 ${round} 轮模型加载..."
    
    if [ -f "$log_file" ]; then
        # 检查加载日志
        if grep -q "loaded widget reuse model contains actions:" "$log_file"; then
            local action_count=$(grep "loaded widget reuse model contains actions:" "$log_file" | tail -1 | sed 's/.*contains actions: //')
            if [ "$action_count" -gt 0 ]; then
                log_success "模型加载成功，包含 $action_count 个操作"
                return 0
            else
                log_warning "模型加载成功，但包含 0 个操作"
                return 1
            fi
        else
            log_warning "未找到模型加载日志"
            return 1
        fi
    else
        log_error "日志文件不存在"
        return 1
    fi
}

# 主函数
main() {
    log_header "模型保存修复验证测试"
    
    check_device
    create_results_dir
    
    # 第1轮 - 冷启动，生成模型
    log_info "开始第1轮测试 (生成模型)..."
    clean_old_model
    force_stop_app
    start_app
    
    if run_test 1; then
        analyze_model_file 1
        if check_model_exists; then
            log_success "✓ 第1轮测试成功生成模型文件"
        else
            log_error "✗ 第1轮测试未生成模型文件"
            exit 1
        fi
    else
        exit 1
    fi
    
    sleep 3
    
    # 第2轮 - 加载模型测试
    log_info "开始第2轮测试 (加载模型)..."
    force_stop_app
    start_app
    
    if run_test 2; then
        if verify_model_loading 2; then
            log_success "✓ 第2轮测试成功加载模型"
        else
            log_warning "✗ 第2轮测试模型加载有问题"
        fi
        analyze_model_file 2
    else
        exit 1
    fi
    
    # 生成测试报告
    log_header "测试结果总结"
    
    local report_file="${RESULTS_DIR}/test_report.txt"
    {
        echo "模型保存修复验证测试报告"
        echo "生成时间: $(date)"
        echo "========================================"
        echo ""
        
        echo "测试配置:"
        echo "  - 包名: $PACKAGE_NAME"
        echo "  - 每轮时长: $TEST_DURATION 分钟"
        echo "  - 节流时间: $THROTTLE 毫秒"
        echo ""
        
        echo "文件列表:"
        ls -la "${RESULTS_DIR}/" | grep -E "\.(log|fbm)$" | while read line; do
            echo "  $line"
        done
        echo ""
        
        echo "模型文件分析:"
        if [ -f "${RESULTS_DIR}/round1_model.fbm" ]; then
            local size1=$(stat -c%s "${RESULTS_DIR}/round1_model.fbm" 2>/dev/null || echo "0")
            echo "  - 第1轮模型大小: $size1 字节"
        fi
        
        if [ -f "${RESULTS_DIR}/round2_model.fbm" ]; then
            local size2=$(stat -c%s "${RESULTS_DIR}/round2_model.fbm" 2>/dev/null || echo "0")
            echo "  - 第2轮模型大小: $size2 字节"
        fi
        
    } > "$report_file"
    
    cat "$report_file"
    
    log_header "测试完成"
    log_success "结果保存在: ${RESULTS_DIR}"
    echo ""
    echo "主要文件:"
    echo "  - 测试报告: ${RESULTS_DIR}/test_report.txt"
    echo "  - 第1轮日志: ${RESULTS_DIR}/round1_test.log"
    echo "  - 第2轮日志: ${RESULTS_DIR}/round2_test.log"
    echo "  - 第1轮模型: ${RESULTS_DIR}/round1_model.fbm"
    echo "  - 第2轮模型: ${RESULTS_DIR}/round2_model.fbm"
    echo ""
}

# 清理函数
cleanup() {
    force_stop_app 2>/dev/null || true
}

trap cleanup EXIT INT TERM

main "$@"
