#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
模型分析工具使用示例
演示如何使用analyze_model_actions.py分析模型文件
"""

import os
import sys
from pathlib import Path

def find_model_files():
    """查找可用的模型文件"""
    possible_paths = [
        "/sdcard/fastbot_com.netease.cloudmusic.fbm",
        "/sdcard/fastbot_com.netease.cloudmusic.tablet.fbm",
        "fastbot_com.netease.cloudmusic.fbm",
        "fastbot_com.netease.cloudmusic.tablet.fbm",
    ]
    
    # 查找test_results目录中的模型文件
    test_results_dirs = [d for d in os.listdir('.') if d.startswith('test_results_')]
    for test_dir in test_results_dirs:
        possible_paths.extend([
            f"{test_dir}/fastbot_com.netease.cloudmusic.fbm",
            f"{test_dir}/fastbot_com.netease.cloudmusic.tablet.fbm",
        ])
    
    found_files = []
    for path in possible_paths:
        if os.path.exists(path):
            found_files.append(path)
    
    return found_files

def run_analysis_examples():
    """运行分析示例"""
    print("=== 模型分析工具使用示例 ===\n")
    
    # 查找模型文件
    model_files = find_model_files()
    
    if not model_files:
        print("未找到任何模型文件")
        print("请确保以下位置之一存在模型文件:")
        print("  - /sdcard/fastbot_com.netease.cloudmusic.fbm")
        print("  - fastbot_com.netease.cloudmusic.fbm")
        print("  - test_results_*/fastbot_com.netease.cloudmusic.fbm")
        return
    
    print(f"找到 {len(model_files)} 个模型文件:")
    for i, file in enumerate(model_files):
        print(f"  {i+1}. {file}")
    
    # 选择第一个模型文件进行分析
    model_file = model_files[0]
    print(f"\n使用模型文件: {model_file}")
    
    # 示例1: 基本分析
    print("\n--- 示例1: 基本分析 ---")
    cmd = f"python3 analyze_model_actions.py '{model_file}' --summary-only"
    print(f"命令: {cmd}")
    os.system(cmd)
    
    # 示例2: 详细分析
    print("\n--- 示例2: 详细分析 (前5个actions) ---")
    cmd = f"python3 analyze_model_actions.py '{model_file}' --limit 5"
    print(f"命令: {cmd}")
    os.system(cmd)
    
    # 示例3: 导出到JSON
    print("\n--- 示例3: 导出到JSON文件 ---")
    output_file = "model_analysis_example.json"
    cmd = f"python3 analyze_model_actions.py '{model_file}' --output {output_file}"
    print(f"命令: {cmd}")
    os.system(cmd)
    
    if os.path.exists(output_file):
        file_size = os.path.getsize(output_file)
        print(f"JSON文件已生成: {output_file} ({file_size} 字节)")
        
        # 显示JSON文件的前几行
        print("\nJSON文件内容预览:")
        with open(output_file, 'r', encoding='utf-8') as f:
            lines = f.readlines()
            for i, line in enumerate(lines[:10]):
                print(f"  {i+1:2d}: {line.rstrip()}")
            if len(lines) > 10:
                print(f"  ... 还有 {len(lines) - 10} 行")

def show_usage_tips():
    """显示使用技巧"""
    print("\n=== 使用技巧 ===")
    print("1. 基本使用:")
    print("   python3 analyze_model_actions.py model.fbm")
    
    print("\n2. 只显示摘要:")
    print("   python3 analyze_model_actions.py model.fbm --summary-only")
    
    print("\n3. 显示更多详细信息:")
    print("   python3 analyze_model_actions.py model.fbm --limit 20")
    
    print("\n4. 导出到JSON文件:")
    print("   python3 analyze_model_actions.py model.fbm --output analysis.json")
    
    print("\n5. 批量分析多个文件:")
    print("   for file in *.fbm; do")
    print("     python3 analyze_model_actions.py \"$file\" --summary-only")
    print("   done")
    
    print("\n6. 查找特定类型的actions:")
    print("   python3 analyze_model_actions.py model.fbm --output analysis.json")
    print("   grep -A 5 -B 5 'action_type.*1' analysis.json")

if __name__ == '__main__':
    try:
        run_analysis_examples()
        show_usage_tips()
    except KeyboardInterrupt:
        print("\n\n用户中断操作")
    except Exception as e:
        print(f"\n错误: {e}")
        sys.exit(1)
