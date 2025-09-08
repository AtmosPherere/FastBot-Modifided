#!/bin/bash

# Fastbot 重用模型测试脚本
# 进行两轮60分钟测试，分析冷启动和重用效果
# 作者: AI Assistant
# 日期: 2025-07-16

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 配置参数
PACKAGE_NAME="com.netease.cloudmusic"
TEST_DURATION=60  # 分钟
THROTTLE=600      # 毫秒
RESULTS_DIR="test_results_$(date +%Y%m%d_%H%M%S)"
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
    log_info "检查设备连接..."
    
    if ! command -v adb &> /dev/null; then
        log_error "adb 未找到，请确保 Android SDK 已安装并添加到 PATH"
        exit 1
    fi
    
    if ! adb devices | grep -q "device$"; then
        log_error "未检测到 Android 设备，请确保设备已连接并启用 USB 调试"
        exit 1
    fi
    
    log_success "设备连接正常"
}

# 检查应用是否安装
check_app() {
    log_info "检查应用 ${PACKAGE_NAME} 是否已安装..."
    
    if ! adb shell pm list packages | grep -q "package:${PACKAGE_NAME}"; then
        log_error "应用 ${PACKAGE_NAME} 未安装，请先安装应用"
        exit 1
    fi
    
    log_success "应用 ${PACKAGE_NAME} 已安装"
}

# 创建结果目录
create_results_dir() {
    mkdir -p "${RESULTS_DIR}"
    log_info "创建结果目录: ${RESULTS_DIR}"
}

# 清理旧的模型文件
clean_old_model() {
    log_info "清理旧的模型文件..."
    adb shell "rm -f ${MODEL_FILE}" 2>/dev/null || true
    log_success "旧模型文件已清理"
}

# 检查模型文件是否存在
check_model_exists() {
    if adb shell "test -f ${MODEL_FILE}" 2>/dev/null; then
        return 0  # 文件存在
    else
        return 1  # 文件不存在
    fi
}

# 获取模型文件大小
get_model_size() {
    local size=$(adb shell "stat -c%s ${MODEL_FILE} 2>/dev/null" | tr -d '\r\n' || echo "0")
    echo "$size"
}

# 强制停止应用
force_stop_app() {
    log_info "强制停止应用 ${PACKAGE_NAME}..."
    adb shell am force-stop "${PACKAGE_NAME}"
    sleep 2
    log_success "应用已停止"
}

# 启动应用
start_app() {
    log_info "启动应用 ${PACKAGE_NAME}..."
    adb shell monkey -p "${PACKAGE_NAME}" -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1
    sleep 3
    log_success "应用已启动"
}

# 运行 Fastbot 测试
run_fastbot_test() {
    local round=$1
    local log_file="${RESULTS_DIR}/round${round}_test.log"
    local start_time=$(date +%s)
    
    log_header "开始第 ${round} 轮测试 (${TEST_DURATION} 分钟)"
    
    # 记录测试开始信息
    {
        echo "========================================="
        echo "Fastbot 重用模型测试 - 第 ${round} 轮"
        echo "开始时间: $(date)"
        echo "包名: ${PACKAGE_NAME}"
        echo "测试时长: ${TEST_DURATION} 分钟"
        echo "节流时间: ${THROTTLE} 毫秒"
        echo "========================================="
        echo ""
    } > "$log_file"
    
    # 执行测试命令
    local test_cmd="export LD_LIBRARY_PATH=/data/local/tmp:\$LD_LIBRARY_PATH && CLASSPATH=/sdcard/monkeyq.jar:/sdcard/framework.jar:/sdcard/fastbot-thirdpart.jar exec app_process /system/bin com.android.commands.monkey.Monkey -p ${PACKAGE_NAME} --agent reuseq --running-minutes ${TEST_DURATION} --throttle ${THROTTLE} -v -v"
    
    log_info "执行测试命令..."
    echo "命令: $test_cmd" >> "$log_file"
    echo "" >> "$log_file"
    
    # 运行测试并记录日志
    if adb shell "$test_cmd" >> "$log_file" 2>&1; then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        log_success "第 ${round} 轮测试完成，耗时: ${duration} 秒"
    else
        log_error "第 ${round} 轮测试失败"
        return 1
    fi
    
    # 记录测试结束信息
    {
        echo ""
        echo "========================================="
        echo "测试结束时间: $(date)"
        echo "实际耗时: $((duration / 60)) 分钟 $((duration % 60)) 秒"
        echo "========================================="
    } >> "$log_file"
}

# 分析活动覆盖率
analyze_coverage() {
    local round=$1
    local log_file="${RESULTS_DIR}/round${round}_test.log"
    local coverage_file="${RESULTS_DIR}/round${round}_coverage.txt"

    log_info "分析第 ${round} 轮活动覆盖率..."

    # 提取访问的活动
    if [ -f "$log_file" ]; then
        # 使用 "Explored app activities:" 后面的活动列表，这更准确
        if grep -q "Explored app activities:" "$log_file"; then
            # 改进的提取方法：找到活动列表的开始和结束
            # 1. 找到 "Explored app activities:" 的行号
            local start_line=$(grep -n "Explored app activities:" "$log_file" | tail -1 | cut -d: -f1)

            if [ -n "$start_line" ]; then
                # 2. 提取所有活动行，更精确的过滤
                # 匹配格式：[时间戳] 序号 活动名称
                # 使用com.开头来确保是包名，然后去重
                grep -A 200 "Explored app activities:" "$log_file" | \
                grep "^.*\][[:space:]]*[0-9][0-9]*[[:space:]].*com\." | \
                sed 's/^.*\][[:space:]]*[0-9][0-9]*[[:space:]]*//' | \
                grep -v "^$" | sort | uniq > "$coverage_file"
            else
                touch "$coverage_file"
            fi
        else
            # 如果没有找到活动统计，使用备用方法
            grep "current activity is" "$log_file" | sed 's/.*current activity is //' | sort | uniq > "$coverage_file" 2>/dev/null || touch "$coverage_file"
        fi

        local activity_count=$(wc -l < "$coverage_file" 2>/dev/null || echo "0")
        log_success "第 ${round} 轮共探索了 ${activity_count} 个不同的活动"

        # 显示前10个活动
        if [ "$activity_count" -gt 0 ]; then
            echo "前10个探索的活动:" >> "${RESULTS_DIR}/round${round}_analysis.txt"
            head -10 "$coverage_file" >> "${RESULTS_DIR}/round${round}_analysis.txt"
        fi
        
        echo "$activity_count"
    else
        log_warning "未找到第 ${round} 轮的日志文件"
        echo "0"
    fi
}

# 比较两轮测试的活动覆盖率
compare_coverage() {
    log_header "比较两轮测试结果"
    
    local round1_coverage="${RESULTS_DIR}/round1_coverage.txt"
    local round2_coverage="${RESULTS_DIR}/round2_coverage.txt"
    local comparison_file="${RESULTS_DIR}/coverage_comparison.txt"
    
    if [ -f "$round1_coverage" ] && [ -f "$round2_coverage" ]; then
        {
            echo "========================================="
            echo "活动覆盖率比较分析"
            echo "生成时间: $(date)"
            echo "========================================="
            echo ""
            
            local count1=$(wc -l < "$round1_coverage")
            local count2=$(wc -l < "$round2_coverage")
            
            echo "第1轮测试探索活动数: $count1"
            echo "第2轮测试探索活动数: $count2"
            echo ""
            
            # 计算重复和新增的活动
            local common_activities=$(comm -12 <(sort "$round1_coverage") <(sort "$round2_coverage") | wc -l)
            local round1_only=$(comm -23 <(sort "$round1_coverage") <(sort "$round2_coverage") | wc -l)
            local round2_only=$(comm -13 <(sort "$round1_coverage") <(sort "$round2_coverage") | wc -l)
            
            echo "两轮都探索的活动数: $common_activities"
            echo "仅第1轮探索的活动数: $round1_only"
            echo "仅第2轮探索的活动数: $round2_only"
            echo ""
            
            # 计算重复率和提升率
            if [ "$count1" -gt 0 ]; then
                local overlap_rate=$(echo "scale=2; $common_activities * 100 / $count1" | bc -l 2>/dev/null || echo "0")
                echo "活动重复率: ${overlap_rate}%"
            fi
            
            if [ "$count1" -gt 0 ]; then
                local improvement_rate=$(echo "scale=2; ($count2 - $count1) * 100 / $count1" | bc -l 2>/dev/null || echo "0")
                echo "覆盖率提升: ${improvement_rate}%"
            fi
            
            echo ""
            echo "========================================="
            echo "详细分析"
            echo "========================================="
            echo ""
            
            if [ "$round2_only" -gt 0 ]; then
                echo "第2轮新探索的活动:"
                comm -13 <(sort "$round1_coverage") <(sort "$round2_coverage")
                echo ""
            fi
            
            if [ "$round1_only" -gt 0 ]; then
                echo "第1轮独有的活动:"
                comm -23 <(sort "$round1_coverage") <(sort "$round2_coverage")
                echo ""
            fi
            
        } > "$comparison_file"
        
        # 显示比较结果
        cat "$comparison_file"
        
        log_success "详细比较结果已保存到: $comparison_file"
    else
        log_warning "无法进行比较，缺少覆盖率文件"
    fi
}

# 分析模型文件变化
analyze_model_changes() {
    log_info "分析模型文件变化..."
    
    local model_analysis="${RESULTS_DIR}/model_analysis.txt"
    
    {
        echo "========================================="
        echo "模型文件变化分析"
        echo "生成时间: $(date)"
        echo "========================================="
        echo ""
        
        if check_model_exists; then
            local final_size=$(get_model_size)
            echo "最终模型文件大小: $final_size 字节"
            echo "模型文件路径: $MODEL_FILE"
            
            # 尝试从设备拉取模型文件进行备份
            local backup_model="${RESULTS_DIR}/final_model.fbm"
            if adb pull "$MODEL_FILE" "$backup_model" >/dev/null 2>&1; then
                echo "模型文件已备份到: $backup_model"
            fi
        else
            echo "警告: 测试结束后未找到模型文件"
        fi
        
    } > "$model_analysis"
    
    cat "$model_analysis"
}

# 生成测试报告
generate_report() {
    local report_file="${RESULTS_DIR}/test_report.md"
    
    log_info "生成测试报告..."
    
    {
        echo "# Fastbot 重用模型测试报告"
        echo ""
        echo "**生成时间:** $(date)"
        echo "**测试包名:** $PACKAGE_NAME"
        echo "**每轮测试时长:** $TEST_DURATION 分钟"
        echo "**节流时间:** $THROTTLE 毫秒"
        echo ""
        
        echo "## 测试概述"
        echo ""
        echo "本次测试进行了两轮60分钟的Fastbot测试："
        echo "- 第1轮：冷启动测试，无重用模型"
        echo "- 第2轮：使用第1轮生成的模型进行重用测试"
        echo ""
        
        echo "## 文件结构"
        echo ""
        echo "```"
        echo "$RESULTS_DIR/"
        echo "├── round1_test.log          # 第1轮测试完整日志"
        echo "├── round1_coverage.txt      # 第1轮活动覆盖率"
        echo "├── round1_analysis.txt      # 第1轮分析结果"
        echo "├── round2_test.log          # 第2轮测试完整日志"
        echo "├── round2_coverage.txt      # 第2轮活动覆盖率"
        echo "├── round2_analysis.txt      # 第2轮分析结果"
        echo "├── coverage_comparison.txt  # 两轮覆盖率比较"
        echo "├── model_analysis.txt       # 模型文件分析"
        echo "├── final_model.fbm          # 最终模型文件备份"
        echo "└── test_report.md           # 本报告"
        echo "```"
        echo ""
        
        echo "## 主要发现"
        echo ""
        echo "详细的分析结果请查看对应的分析文件。"
        echo ""
        
    } > "$report_file"
    
    log_success "测试报告已生成: $report_file"
}

# 主函数
main() {
    log_header "Fastbot 重用模型测试脚本"
    
    # 记录开始时间
    local script_start_time=$(date +%s)
    
    # 检查先决条件
    check_device
    check_app
    create_results_dir
    
    echo ""
    log_info "开始两轮重用模型测试..."
    echo ""
    
    # 第1轮测试 - 冷启动
    log_header "第1轮测试 - 冷启动 (无重用模型)"
    clean_old_model
    force_stop_app
    start_app
    
    if run_fastbot_test 1; then
        local round1_activities=$(analyze_coverage 1)
        log_success "第1轮测试完成，探索了 ${round1_activities} 个活动"
        
        # 检查是否生成了模型文件
        if check_model_exists; then
            local model_size=$(get_model_size)
            log_success "第1轮测试生成了模型文件，大小: ${model_size} 字节"
        else
            log_warning "第1轮测试未生成模型文件"
        fi
    else
        log_error "第1轮测试失败"
        exit 1
    fi
    
    echo ""
    sleep 5
    
    # 第2轮测试 - 重用模型
    log_header "第2轮测试 - 重用模型"
    force_stop_app
    start_app
    
    if run_fastbot_test 2; then
        local round2_activities=$(analyze_coverage 2)
        log_success "第2轮测试完成，探索了 ${round2_activities} 个活动"
    else
        log_error "第2轮测试失败"
        exit 1
    fi
    
    echo ""
    
    # 分析结果
    compare_coverage
    analyze_model_changes
    generate_report
    
    # 计算总耗时
    local script_end_time=$(date +%s)
    local total_duration=$((script_end_time - script_start_time))
    
    log_header "测试完成"
    log_success "总耗时: $((total_duration / 60)) 分钟 $((total_duration % 60)) 秒"
    log_success "所有结果已保存到: ${RESULTS_DIR}"
    
    echo ""
    log_info "主要结果文件:"
    echo "  - 测试报告: ${RESULTS_DIR}/test_report.md"
    echo "  - 覆盖率比较: ${RESULTS_DIR}/coverage_comparison.txt"
    echo "  - 第1轮日志: ${RESULTS_DIR}/round1_test.log"
    echo "  - 第2轮日志: ${RESULTS_DIR}/round2_test.log"
    echo ""
}

# 捕获中断信号
cleanup() {
    log_warning "测试被中断"
    force_stop_app 2>/dev/null || true
}

trap cleanup EXIT INT TERM

# 执行主函数
main "$@"
