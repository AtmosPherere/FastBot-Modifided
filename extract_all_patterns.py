#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
提取模型文件中的所有模式信息
"""

import sys
import os
import re
import json

def extract_all_patterns(model_path):
    """提取模型文件中的所有模式"""
    print(f'=== 提取模型文件中的所有模式: {model_path} ===')
    
    # 读取模型文件
    with open(model_path, 'rb') as f:
        data = f.read()
    
    print(f'文件大小: {len(data)} 字节')
    print()
    
    # 查找所有字符串模式
    all_strings = []
    current_string = ""
    
    for byte in data:
        if 32 <= byte <= 126:  # 可打印ASCII字符
            current_string += chr(byte)
        else:
            if len(current_string) > 2:
                all_strings.append(current_string)
            current_string = ""
    
    # 过滤出有意义的字符串
    meaningful_strings = []
    for s in all_strings:
        if (len(s) > 3 and 
            (s.startswith('com.') or 
             s.startswith('iot:') or 
             s.startswith('nexuslauncher:') or
             s.startswith('android.') or
             'Activity' in s or
             'id/' in s or
             s.isalpha())):
            meaningful_strings.append(s)
    
    # 分类字符串
    activities = []
    resource_ids = []
    other_strings = []
    
    for s in meaningful_strings:
        if s.startswith('com.') and 'Activity' in s:
            activities.append(s)
        elif ':id/' in s:
            resource_ids.append(s)
        else:
            other_strings.append(s)
    
    # 查找中文字符
    chinese_pattern = rb'[\xe4-\xe9][\x80-\xbf][\x80-\xbf]+'
    chinese_texts = re.findall(chinese_pattern, data)
    chinese_strings = []
    for text in set(chinese_texts):
        try:
            chinese_strings.append(text.decode('utf-8'))
        except:
            pass
    
    # 输出结果
    print(f'发现的Activity模式 ({len(activities)}个):')
    for i, activity in enumerate(sorted(set(activities))):
        print(f'  [{i}] "{activity}"')
    
    print(f'\n发现的资源ID模式 ({len(resource_ids)}个):')
    for i, res_id in enumerate(sorted(set(resource_ids))):
        print(f'  [{i}] "{res_id}"')
    
    print(f'\n发现的中文文本 ({len(chinese_strings)}个):')
    for i, text in enumerate(sorted(set(chinese_strings))):
        print(f'  [{i}] "{text}"')
    
    print(f'\n其他有意义的字符串 ({len(other_strings)}个):')
    for i, s in enumerate(sorted(set(other_strings))[:50]):  # 限制显示数量
        print(f'  [{i}] "{s}"')
    
    if len(other_strings) > 50:
        print(f'  ... 还有 {len(other_strings) - 50} 个')
    
    # 保存到JSON文件
    result = {
        'file_path': model_path,
        'file_size': len(data),
        'activities': sorted(set(activities)),
        'resource_ids': sorted(set(resource_ids)),
        'chinese_texts': sorted(set(chinese_strings)),
        'other_strings': sorted(set(other_strings))
    }
    
    output_file = model_path.replace('.fbm', '_all_patterns.json')
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
    
    print(f'\n所有模式已保存到: {output_file}')

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('用法: python3 extract_all_patterns.py <模型文件路径>')
        sys.exit(1)
    
    model_path = sys.argv[1]
    if not os.path.exists(model_path):
        print(f'错误: 模型文件不存在: {model_path}')
        sys.exit(1)
    
    extract_all_patterns(model_path)
