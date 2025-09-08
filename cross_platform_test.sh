#!/bin/bash

# Fastbot 跨平台Widget复用功能测试脚本
# 验证手机端是否能正确加载和使用平板端的模型数据
# 作者: AI Assistant
# 日期: 2025-07-24

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

# 配置参数
PACKAGE_NAME="com.netease.cloudmusic"  # 手机版网易云音乐包名
TEST_DURATION=60   # 分钟 (稍长一些以观察跨平台效果)
THROTTLE=1000      # 毫秒 (稍快一些)
RESULTS_DIR="cross_platform_test_$(date +%Y%m%d_%H%M%S)"
LOCAL_MODEL_FILE="/sdcard/fastbot_${PACKAGE_NAME}.fbm"
TABLET_MODEL_FILE="/sdcard/fastbot_${PACKAGE_NAME}.tablet.fbm"

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

log_highlight() {
    echo -e "${MAGENTA}🔍 $1${NC}"
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

# 检查平板模型文件
check_tablet_model() {
    log_info "检查平板模型文件..."
    if adb shell "test -f ${TABLET_MODEL_FILE}" 2>/dev/null; then
        local size=$(adb shell "stat -c%s ${TABLET_MODEL_FILE} 2>/dev/null" | tr -d '\r\n')
        log_success "✅ 平板模型文件存在，大小: $size 字节"
        return 0
    else
        log_error "❌ 平板模型文件不存在: ${TABLET_MODEL_FILE}"
        log_error "请先上传平板模型文件到手机设备"
        exit 1
    fi
}

# 清理本机模型
clean_local_model() {
    log_info "清理本机模型文件..."
    adb shell "rm -f ${LOCAL_MODEL_FILE}" 2>/dev/null || true
    log_success "本机模型文件已清理"
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

# 运行跨平台测试
run_cross_platform_test() {
    local log_file="${RESULTS_DIR}/cross_platform_test.log"
    local logcat_file="${RESULTS_DIR}/cross_platform_logcat.log"
    
    log_header "跨平台复用测试 (${TEST_DURATION} 分钟)"
    
    local test_cmd="export LD_LIBRARY_PATH=/data/local/tmp:\$LD_LIBRARY_PATH && CLASSPATH=/sdcard/monkeyq.jar:/sdcard/framework.jar:/sdcard/fastbot-thirdpart.jar exec app_process /system/bin com.android.commands.monkey.Monkey -p ${PACKAGE_NAME} --agent reuseq --running-minutes ${TEST_DURATION} --throttle ${THROTTLE} -v -v"
    
    echo "开始时间: $(date)" > "$log_file"
    echo "测试命令: $test_cmd" >> "$log_file"
    echo "=========================================" >> "$log_file"
    
    # 启动logcat监控
    log_info "启动logcat监控..."
    adb logcat -c  # 清空logcat缓冲区
    adb logcat > "$logcat_file" &
    local logcat_pid=$!
    
    # 运行测试
    log_info "开始跨平台复用测试..."
    if adb shell "$test_cmd" >> "$log_file" 2>&1; then
        echo "结束时间: $(date)" >> "$log_file"
        log_success "跨平台测试完成"
        
        # 停止logcat监控
        kill $logcat_pid 2>/dev/null || true
        sleep 2
        
        return 0
    else
        log_error "跨平台测试失败"
        kill $logcat_pid 2>/dev/null || true
        return 1
    fi
}

# 分析跨平台复用效果
analyze_cross_platform_effect() {
    local test_log="${RESULTS_DIR}/cross_platform_test.log"
    local logcat_log="${RESULTS_DIR}/cross_platform_logcat.log"
    local analysis_file="${RESULTS_DIR}/cross_platform_analysis.txt"
    
    log_header "跨平台复用效果分析"
    
    echo "跨平台复用功能分析报告" > "$analysis_file"
    echo "生成时间: $(date)" >> "$analysis_file"
    echo "=========================================" >> "$analysis_file"
    
    # 1. 检查多平台模型自动检测
    log_highlight "1. 检查多平台模型自动检测..."
    if grep -q "自动检测多平台模型" "$logcat_log" 2>/dev/null; then
        log_success "✅ 发现多平台模型自动检测日志"
        echo "✅ 多平台模型自动检测: 正常" >> "$analysis_file"
        
        # 提取检测详情
        grep "自动检测多平台模型\|发现外部平台模型\|成功加载外部平台模型\|多平台模型检测完成" "$logcat_log" 2>/dev/null | head -10 >> "$analysis_file"
    else
        log_warning "⚠️  未发现多平台模型自动检测日志"
        echo "⚠️  多平台模型自动检测: 未发现相关日志" >> "$analysis_file"
    fi
    
    # 2. 检查外部模型加载
    log_highlight "2. 检查外部模型加载..."
    if grep -q "tablet" "$logcat_log" 2>/dev/null; then
        log_success "✅ 发现平板模型相关日志"
        echo "✅ 平板模型加载: 正常" >> "$analysis_file"
        
        # 提取平板模型相关日志
        echo "" >> "$analysis_file"
        echo "平板模型相关日志:" >> "$analysis_file"
        grep -i "tablet\|外部模型\|外部平台" "$logcat_log" 2>/dev/null | head -5 >> "$analysis_file"
    else
        log_warning "⚠️  未发现平板模型相关日志"
        echo "⚠️  平板模型加载: 未发现相关日志" >> "$analysis_file"
    fi
    
    # 3. 检查相似度匹配
    log_highlight "3. 检查相似度匹配..."
    if grep -q "相似度\|similarity" "$logcat_log" 2>/dev/null; then
        log_success "✅ 发现相似度匹配日志"
        echo "✅ 相似度匹配: 正常" >> "$analysis_file"
        
        # 提取相似度匹配日志
        echo "" >> "$analysis_file"
        echo "相似度匹配日志:" >> "$analysis_file"
        grep -i "相似度\|similarity" "$logcat_log" 2>/dev/null | head -5 >> "$analysis_file"
    else
        log_warning "⚠️  未发现相似度匹配日志"
        echo "⚠️  相似度匹配: 未发现相关日志" >> "$analysis_file"
    fi
    
    # 4. 检查跨平台action选择
    log_highlight "4. 检查跨平台action选择..."
    if grep -q "外部模型中找到\|external.*match" "$logcat_log" 2>/dev/null; then
        log_success "✅ 发现跨平台action选择日志"
        echo "✅ 跨平台action选择: 正常" >> "$analysis_file"
        
        # 提取跨平台选择日志
        echo "" >> "$analysis_file"
        echo "跨平台action选择日志:" >> "$analysis_file"
        grep -i "外部模型中找到\|external.*match" "$logcat_log" 2>/dev/null | head -5 >> "$analysis_file"
    else
        log_warning "⚠️  未发现跨平台action选择日志"
        echo "⚠️  跨平台action选择: 未发现相关日志" >> "$analysis_file"
    fi
    
    # 5. 检查奖励计算调整
    log_highlight "5. 检查奖励计算调整..."
    if grep -q "调整后奖励\|奖励.*调整" "$logcat_log" 2>/dev/null; then
        log_success "✅ 发现奖励调整日志"
        echo "✅ 奖励计算调整: 正常" >> "$analysis_file"
        
        # 提取奖励调整日志
        echo "" >> "$analysis_file"
        echo "奖励调整日志:" >> "$analysis_file"
        grep -i "调整后奖励\|奖励.*调整" "$logcat_log" 2>/dev/null | head -5 >> "$analysis_file"
    else
        log_warning "⚠️  未发现奖励调整日志"
        echo "⚠️  奖励计算调整: 未发现相关日志" >> "$analysis_file"
    fi
    
    # 6. 统计活动覆盖情况
    log_highlight "6. 统计活动覆盖情况..."
    if grep -q "Explored app activities:" "$test_log"; then
        local activity_count=$(grep -A 20 "Explored app activities:" "$test_log" | grep "^.*[0-9].*com\." | wc -l)
        log_success "✅ 探索了 $activity_count 个活动"
        echo "✅ 活动覆盖: $activity_count 个活动" >> "$analysis_file"
        
        # 提取活动列表
        echo "" >> "$analysis_file"
        echo "探索的活动列表:" >> "$analysis_file"
        grep -A 20 "Explored app activities:" "$test_log" | grep "^.*[0-9].*com\." | sed 's/^.*[0-9][[:space:]]*/  - /' >> "$analysis_file"
    else
        log_warning "⚠️  未找到活动覆盖统计"
        echo "⚠️  活动覆盖: 未找到统计信息" >> "$analysis_file"
    fi
    
    # 7. 检查模型文件状态
    log_highlight "7. 检查模型文件状态..."
    if adb shell "test -f ${LOCAL_MODEL_FILE}" 2>/dev/null; then
        local size=$(adb shell "stat -c%s ${LOCAL_MODEL_FILE} 2>/dev/null" | tr -d '\r\n')
        log_success "✅ 生成了本机模型文件，大小: $size 字节"
        echo "✅ 本机模型生成: $size 字节" >> "$analysis_file"
    else
        log_warning "⚠️  未生成本机模型文件"
        echo "⚠️  本机模型生成: 未生成" >> "$analysis_file"
    fi
    
    echo "" >> "$analysis_file"
    echo "=========================================" >> "$analysis_file"
    echo "分析完成时间: $(date)" >> "$analysis_file"
    
    # 显示分析结果
    echo ""
    log_info "详细分析结果已保存到: $analysis_file"
    echo ""
    cat "$analysis_file"
}

# 主函数
main() {
    log_header "Fastbot 跨平台Widget复用功能测试"
    
    check_device
    create_results_dir
    check_tablet_model
    
    # 保留本机模型，测试重用模型功能
    # clean_local_model  # 注释掉清理本机模型，保留现有模型进行重用测试
    force_stop_app
    start_app
    
    # 运行跨平台测试
    if run_cross_platform_test; then
        log_success "跨平台测试执行完成"
    else
        log_error "跨平台测试执行失败"
        exit 1
    fi
    
    # 分析跨平台复用效果
    analyze_cross_platform_effect
    
    log_header "测试完成"
    log_success "结果保存在: ${RESULTS_DIR}"
    echo ""
    echo "主要文件:"
    echo "  - 测试日志: ${RESULTS_DIR}/cross_platform_test.log"
    echo "  - 系统日志: ${RESULTS_DIR}/cross_platform_logcat.log"
    echo "  - 分析报告: ${RESULTS_DIR}/cross_platform_analysis.txt"
    echo ""
    log_info "如果跨平台复用功能正常，你应该看到:"
    echo "  ✅ 多平台模型自动检测"
    echo "  ✅ 平板模型加载成功"
    echo "  ✅ 相似度匹配计算"
    echo "  ✅ 跨平台action选择"
    echo "  ✅ 奖励计算调整"
}

# 清理函数
cleanup() {
    force_stop_app 2>/dev/null || true
    # 杀死可能残留的logcat进程
    pkill -f "adb logcat" 2>/dev/null || true
}

trap cleanup EXIT INT TERM

main "$@"
