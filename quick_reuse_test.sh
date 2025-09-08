#!/bin/bash

# Fastbot 重用模型快速测试脚本
# 进行两轮5分钟测试，快速验证重用效果
# 作者: AI Assistant
# 日期: 2025-07-16

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# 配置参数
PACKAGE_NAME="com.netease.cloudmusic.iot"
TEST_DURATION=5   # 分钟 (快速测试)
THROTTLE=600      # 毫秒
RESULTS_DIR="quick_test_$(date +%Y%m%d_%H%M%S)"
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

# 分析活动覆盖率
analyze_coverage() {
    local round=$1
    local log_file="${RESULTS_DIR}/round${round}_test.log"
    local coverage_file="${RESULTS_DIR}/round${round}_activities.txt"

    if [ -f "$log_file" ]; then
        # 使用 "Explored app activities:" 后面的活动列表，这更准确
        if grep -q "Explored app activities:" "$log_file"; then
            # 提取活动列表，去掉序号，只保留活动名称，过滤空行和非活动内容
            grep -A 50 "Explored app activities:" "$log_file" | grep "^.*[0-9].*com\." | sed 's/^.*[0-9][[:space:]]*//' | grep -v "^$" | grep "^com\." > "$coverage_file"
            local count=$(wc -l < "$coverage_file" 2>/dev/null || echo "0")
            echo "$count"
        else
            # 如果没有找到活动统计，使用备用方法
            grep "current activity is" "$log_file" | sed 's/.*current activity is //' | sort | uniq > "$coverage_file"
            local count=$(wc -l < "$coverage_file" 2>/dev/null || echo "0")
            echo "$count"
        fi
    else
        echo "0"
    fi
}

# 快速比较结果
quick_compare() {
    local round1_log="${RESULTS_DIR}/round1_test.log"
    local round2_log="${RESULTS_DIR}/round2_test.log"
    local round1_activities="${RESULTS_DIR}/round1_activities.txt"
    local round2_activities="${RESULTS_DIR}/round2_activities.txt"

    log_header "快速结果比较"

    if [ -f "$round1_log" ] && [ -f "$round2_log" ]; then
        local count1=$(analyze_coverage 1)
        local count2=$(analyze_coverage 2)

        echo "第1轮探索活动数: $count1"
        echo "第2轮探索活动数: $count2"

        if [ "$count2" -gt "$count1" ]; then
            local improvement=$((count2 - count1))
            log_success "第2轮比第1轮多探索了 $improvement 个活动"
        elif [ "$count2" -eq "$count1" ]; then
            log_info "两轮探索的活动数相同"
        else
            local decrease=$((count1 - count2))
            log_warning "第2轮比第1轮少探索了 $decrease 个活动"
        fi

        # 详细活动比较
        if [ -f "$round1_activities" ] && [ -f "$round2_activities" ] && [ "$count1" -gt 0 ] && [ "$count2" -gt 0 ]; then
            echo ""
            log_info "详细活动比较分析..."

            # 计算重复和新增的活动
            local common_activities=$(comm -12 <(sort "$round1_activities") <(sort "$round2_activities") | wc -l)
            local round1_only=$(comm -23 <(sort "$round1_activities") <(sort "$round2_activities") | wc -l)
            local round2_only=$(comm -13 <(sort "$round1_activities") <(sort "$round2_activities") | wc -l)

            echo "两轮都探索的活动数: $common_activities"
            echo "仅第1轮探索的活动数: $round1_only"
            echo "仅第2轮探索的活动数: $round2_only"

            # 计算重复率
            if [ "$count1" -gt 0 ]; then
                local overlap_rate=$(echo "scale=1; $common_activities * 100 / $count1" | bc -l 2>/dev/null || echo "0")
                echo "活动重复率: ${overlap_rate}%"
            fi

            # 显示新增的活动
            if [ "$round2_only" -gt 0 ]; then
                echo ""
                log_success "第2轮新探索的活动:"
                comm -13 <(sort "$round1_activities") <(sort "$round2_activities") | sed 's/^/  - /'
            fi

            # 显示第1轮独有的活动
            if [ "$round1_only" -gt 0 ]; then
                echo ""
                log_warning "第1轮独有的活动:"
                comm -23 <(sort "$round1_activities") <(sort "$round2_activities") | sed 's/^/  - /'
            fi
        fi
        
        # 检查重用模型关键日志
        echo ""
        log_info "检查重用模型关键日志..."
        
        if grep -q "select action in reuse model" "$round2_log"; then
            log_success "✓ 第2轮测试中发现了重用模型选择操作"
        else
            log_warning "✗ 第2轮测试中未发现重用模型选择操作"
        fi
        
        if grep -q "save widget reuse model" "$round2_log"; then
            log_success "✓ 第2轮测试中发现了模型保存操作"
        else
            log_warning "✗ 第2轮测试中未发现模型保存操作"
        fi
        
        # 检查模型文件
        if check_model_exists; then
            local size=$(get_model_size)
            log_success "✓ 最终模型文件存在，大小: $size 字节"
        else
            log_warning "✗ 最终模型文件不存在"
        fi
        
    else
        log_error "缺少测试日志文件"
    fi
}

# 主函数
main() {
    log_header "Fastbot 重用模型快速测试"
    
    check_device
    create_results_dir
    
    # 第1轮 - 使用现有模型
    log_info "开始第1轮测试 (使用现有模型)..."
    clean_old_model  # 注释掉清理旧模型，保留现有模型进行测试
    force_stop_app
    start_app
    
    if run_test 1; then
        if check_model_exists; then
            local size=$(get_model_size)
            log_success "第1轮生成模型文件，大小: $size 字节"
        else
            log_warning "第1轮未生成模型文件"
        fi
    else
        exit 1
    fi
    
    sleep 3
    
    # 第2轮 - 重用模型
    log_info "开始第2轮测试 (重用模型)..."
    force_stop_app
    start_app
    
    if run_test 2; then
        log_success "第2轮测试完成"
    else
        exit 1
    fi
    
    # 快速比较
    quick_compare
    
    log_header "测试完成"
    log_success "结果保存在: ${RESULTS_DIR}"
    echo ""
    echo "主要文件:"
    echo "  - 第1轮日志: ${RESULTS_DIR}/round1_test.log"
    echo "  - 第2轮日志: ${RESULTS_DIR}/round2_test.log"
    echo ""
    echo "如需详细分析，请运行: ./run_reuse_test.sh"
}

# 清理函数
cleanup() {
    force_stop_app 2>/dev/null || true
}

trap cleanup EXIT INT TERM

main "$@"
