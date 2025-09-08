#!/bin/bash

# Fastbot 重用模型测试脚本（六轮版）
# 第1轮：冷启动，无重用模型；第2-6轮：重用模型
# 每轮60分钟（可配置），结束后输出每轮覆盖、相邻轮覆盖比较与总体报告
# 作者: AI Assistant
# 日期: 2025-08-19

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
ROUNDS=(1 2 3 4 5 6)

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
        return 0
    else
        return 1
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

# 运行 Fastbot 测试（传入轮次）
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
    local test_cmd="export LD_LIBRARY_PATH=/data/local/tmp:$LD_LIBRARY_PATH && CLASSPATH=/sdcard/monkeyq.jar:/sdcard/framework.jar:/sdcard/fastbot-thirdpart.jar exec app_process /system/bin com.android.commands.monkey.Monkey -p ${PACKAGE_NAME} --agent reuseq --running-minutes ${TEST_DURATION} --throttle ${THROTTLE} -v -v"

    log_info "执行测试命令..."
    echo "命令: $test_cmd" >> "$log_file"
    echo "" >> "$log_file"

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

# 分析活动覆盖率（输出覆盖数量，并写入 roundX_coverage.txt 与 roundX_analysis.txt）
analyze_coverage() {
    local round=$1
    local log_file="${RESULTS_DIR}/round${round}_test.log"
    local coverage_file="${RESULTS_DIR}/round${round}_coverage.txt"

    log_info "分析第 ${round} 轮活动覆盖率..."

    if [ -f "$log_file" ]; then
        if grep -q "Explored app activities:" "$log_file"; then
            local start_line=$(grep -n "Explored app activities:" "$log_file" | tail -1 | cut -d: -f1)
            if [ -n "$start_line" ]; then
                grep -A 200 "Explored app activities:" "$log_file" | \
                grep "^.*\][[:space:]]*[0-9][0-9]*[[:space:]].*com\." | \
                sed 's/^.*\][[:space:]]*[0-9][0-9]*[[:space:]]*//' | \
                grep -v "^$" | sort | uniq > "$coverage_file"
            else
                : > "$coverage_file"
            fi
        else
            grep "current activity is" "$log_file" | sed 's/.*current activity is //' | sort | uniq > "$coverage_file" 2>/dev/null || : > "$coverage_file"
        fi

        local activity_count=$(wc -l < "$coverage_file" 2>/dev/null || echo "0")
        log_success "第 ${round} 轮共探索了 ${activity_count} 个不同的活动"

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

# 比较两个轮次的覆盖（生成文件到 RESULTS_DIR）
compare_two_rounds() {
    local r1=$1
    local r2=$2
    local out_file="${RESULTS_DIR}/coverage_comparison_${r1}_vs_${r2}.txt"
    local round1_coverage="${RESULTS_DIR}/round${r1}_coverage.txt"
    local round2_coverage="${RESULTS_DIR}/round${r2}_coverage.txt"

    if [ -f "$round1_coverage" ] && [ -f "$round2_coverage" ]; then
        {
            echo "========================================="
            echo "活动覆盖率比较分析"
            echo "生成时间: $(date)"
            echo "========================================="
            echo ""

            local count1=$(wc -l < "$round1_coverage")
            local count2=$(wc -l < "$round2_coverage")
            echo "第${r1}轮测试探索活动数:       $count1"
            echo "第${r2}轮测试探索活动数:       $count2"
            echo ""

            local common_activities=$(comm -12 <(sort "$round1_coverage") <(sort "$round2_coverage") | wc -l)
            local round1_only=$(comm -23 <(sort "$round1_coverage") <(sort "$round2_coverage") | wc -l)
            local round2_only=$(comm -13 <(sort "$round1_coverage") <(sort "$round2_coverage") | wc -l)

            echo "两轮都探索的活动数:       $common_activities"
            echo "仅第${r1}轮探索的活动数:       $round1_only"
            echo "仅第${r2}轮探索的活动数:       $round2_only"
            echo ""

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
                echo "第${r2}轮新探索的活动:"
                comm -13 <(sort "$round1_coverage") <(sort "$round2_coverage")
                echo ""
            fi
            if [ "$round1_only" -gt 0 ]; then
                echo "第${r1}轮独有的活动:"
                comm -23 <(sort "$round1_coverage") <(sort "$round2_coverage")
                echo ""
            fi
        } > "$out_file"
        log_success "已生成覆盖比较: $out_file"
    else
        log_warning "无法比较第${r1}与第${r2}轮，覆盖文件缺失"
    fi
}

# 分析模型文件变化（记录最终大小 + 备份）
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

# 生成测试报告（六轮）
generate_report() {
    local report_file="${RESULTS_DIR}/test_report.md"
    log_info "生成测试报告..."
    {
        echo "# Fastbot 重用模型测试报告（六轮）"
        echo ""
        echo "**生成时间:** $(date)"
        echo "**测试包名:** $PACKAGE_NAME"
        echo "**每轮测试时长:** $TEST_DURATION 分钟"
        echo "**节流时间:** $THROTTLE 毫秒"
        echo ""
        echo "## 测试概述"
        echo ""
        echo "本次测试进行了六轮 $TEST_DURATION 分钟的 Fastbot 测试："
        echo "- 第1轮：冷启动测试，无重用模型"
        echo "- 第2-6轮：使用已生成的模型进行重用测试"
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
        echo "├── round3_test.log          # 第3轮测试完整日志"
        echo "├── round3_coverage.txt      # 第3轮活动覆盖率"
        echo "├── round3_analysis.txt      # 第3轮分析结果"
        echo "├── round4_test.log          # 第4轮测试完整日志"
        echo "├── round4_coverage.txt      # 第4轮活动覆盖率"
        echo "├── round4_analysis.txt      # 第4轮分析结果"
        echo "├── round5_test.log          # 第5轮测试完整日志"
        echo "├── round5_coverage.txt      # 第5轮活动覆盖率"
        echo "├── round5_analysis.txt      # 第5轮分析结果"
        echo "├── round6_test.log          # 第6轮测试完整日志"
        echo "├── round6_coverage.txt      # 第6轮活动覆盖率"
        echo "├── round6_analysis.txt      # 第6轮分析结果"
        echo "├── coverage_comparison_1_vs_2.txt  # 相邻轮比较样例"
        echo "├── ...（其余相邻对比文件）"
        echo "├── model_analysis.txt       # 模型文件分析"
        echo "├── final_model.fbm          # 最终模型文件备份"
        echo "└── test_report.md           # 本报告"
        echo "```"
        echo ""
        echo "## 主要发现"
        echo ""
        echo "- 详细的分析结果请查看各轮 *coverage* 与相邻 *comparison* 文件。"
        echo ""
    } > "$report_file"
    log_success "测试报告已生成: $report_file"
}

# 主函数
main() {
    log_header "Fastbot 重用模型测试脚本（六轮）"

    local script_start_time=$(date +%s)

    check_device
    check_app
    create_results_dir

    echo ""
    log_info "开始六轮重用模型测试..."
    echo ""

    # 第1轮：冷启动，清理模型
    log_header "第1轮测试 - 冷启动 (无重用模型)"
    clean_old_model
    force_stop_app
    start_app
    if run_fastbot_test 1; then
        local r1_activities=$(analyze_coverage 1)
        log_success "第1轮测试完成，探索了 ${r1_activities} 个活动"
        if check_model_exists; then
            local model_size_r1=$(get_model_size)
            log_success "第1轮测试生成了模型文件，大小: ${model_size_r1} 字节"
        else
            log_warning "第1轮测试未生成模型文件"
        fi
    else
        log_error "第1轮测试失败"
        exit 1
    fi

    echo ""
    sleep 5

    # 第2-6轮：重用模型
    for round in 2 3 4 5 6; do
        log_header "第${round}轮测试 - 重用模型"
        force_stop_app
        start_app
        if run_fastbot_test ${round}; then
            local acts=$(analyze_coverage ${round})
            log_success "第${round}轮测试完成，探索了 ${acts} 个活动"
        else
            log_error "第${round}轮测试失败"
            exit 1
        fi
        echo ""
        sleep 3
    done

    # 相邻轮比较（1vs2, 2vs3, ..., 5vs6）
    log_header "比较相邻轮次的活动覆盖率"
    compare_two_rounds 1 2
    compare_two_rounds 2 3
    compare_two_rounds 3 4
    compare_two_rounds 4 5
    compare_two_rounds 5 6

    # 模型分析与报告
    analyze_model_changes
    generate_report

    # 总耗时
    local script_end_time=$(date +%s)
    local total_duration=$((script_end_time - script_start_time))

    log_header "测试完成"
    log_success "总耗时: $((total_duration / 60)) 分钟 $((total_duration % 60)) 秒"
    log_success "所有结果已保存到: ${RESULTS_DIR}"
    echo ""
    log_info "主要结果文件:"
    echo "  - 测试报告: ${RESULTS_DIR}/test_report.md"
    echo "  - 覆盖率比较: ${RESULTS_DIR}/coverage_comparison_*.txt"
    echo "  - 各轮日志: ${RESULTS_DIR}/round*_test.log"
}

# 捕获中断信号
cleanup() {
    log_warning "测试被中断"
    force_stop_app 2>/dev/null || true
}

trap cleanup EXIT INT TERM

# 执行主函数
main "$@" 