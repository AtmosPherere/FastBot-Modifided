#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
详细模型分析工具 - 输出所有Action信息
"""

import sys
import os
import flatbuffers
from flatbuffers import Builder

# 添加FlatBuffers生成文件路径
sys.path.append('/Users/atmo/program/Fastbot_Android_副本/native/storage')

def analyze_model_detailed(model_path):
    """详细分析模型文件"""
    print(f'=== 详细分析模型文件: {model_path} ===')
    
    # 读取模型文件
    with open(model_path, 'rb') as f:
        data = f.read()
    
    print(f'文件大小: {len(data)} 字节')
    print()
    
    try:
        import WidgetReuseModel_generated as wrm
        
        # 解析模型
        widget_reuse_model = wrm.GetRootAsWidgetReuseModel(data, 0)
        if not widget_reuse_model or not widget_reuse_model.Model():
            print('模型数据为空')
            return
        
        model_data = widget_reuse_model.Model()
        actions_count = model_data.Length()
        print(f'总Actions数量: {actions_count}')
        print()
        
        for i in range(actions_count):
            reuse_entry = model_data.Get(i)
            action_hash = reuse_entry.Action()
            
            print(f'--- Action {i+1} ---')
            print(f'Action Hash: 0x{action_hash:x}')
            
            # 获取相似度属性
            similarity_attrs = reuse_entry.SimilarityAttrs()
            if similarity_attrs:
                action_type = similarity_attrs.ActionType()
                activity_name = similarity_attrs.ActivityName().decode('utf-8') if similarity_attrs.ActivityName() else ''
                
                target_widget = similarity_attrs.TargetWidget()
                if target_widget:
                    widget_text = target_widget.Text().decode('utf-8') if target_widget.Text() else ''
                    widget_resource_id = target_widget.ResourceId().decode('utf-8') if target_widget.ResourceId() else ''
                    widget_icon_size = len(target_widget.IconBase64()) if target_widget.IconBase64() else 0
                else:
                    widget_text = ''
                    widget_resource_id = ''
                    widget_icon_size = 0
                
                print(f'外部模型action属性[{i}]: type={action_type}, text="{widget_text}", resourceId="{widget_resource_id}", activity="{activity_name}"')
                if widget_icon_size > 0:
                    print(f'  图标大小: {widget_icon_size} 字节')
            else:
                print(f'外部模型action属性[{i}]: 无相似度属性')
            
            # 获取widgets信息
            activities = reuse_entry.Activities()
            if activities:
                total_widgets = 0
                for j in range(activities.Length()):
                    activity_widget_map = activities.Get(j)
                    widgets = activity_widget_map.Widgets()
                    if widgets:
                        total_widgets += widgets.Length()
                        
                        # 显示所有widget的详细信息
                        print(f'  关联Widgets (共{widgets.Length()}个):')
                        for k in range(widgets.Length()):
                            widget_count = widgets.Get(k)
                            widget_hash = widget_count.WidgetHash()
                            count = widget_count.Count()
                            
                            widget_similarity_attrs = widget_count.SimilarityAttrs()
                            if widget_similarity_attrs:
                                widget_text = widget_similarity_attrs.Text().decode('utf-8') if widget_similarity_attrs.Text() else ''
                                widget_activity = widget_similarity_attrs.ActivityName().decode('utf-8') if widget_similarity_attrs.ActivityName() else ''
                                widget_resource_id = widget_similarity_attrs.ResourceId().decode('utf-8') if widget_similarity_attrs.ResourceId() else ''
                                widget_icon_size = len(widget_similarity_attrs.IconBase64()) if widget_similarity_attrs.IconBase64() else 0
                                
                                print(f'    Widget {k+1}: hash=0x{widget_hash:x}, count={count}')
                                print(f'      text="{widget_text}", resourceId="{widget_resource_id}", activity="{widget_activity}"')
                                if widget_icon_size > 0:
                                    print(f'      图标大小: {widget_icon_size} 字节')
                            else:
                                print(f'    Widget {k+1}: hash=0x{widget_hash:x}, count={count} (无相似度属性)')
                
                print(f'  总关联Widgets数量: {total_widgets}')
            else:
                print('  无关联Widgets')
            
            print()
            
    except ImportError as e:
        print(f'无法导入FlatBuffers生成类: {e}')
        print('使用基础分析模式...')
        
        # 基础分析
        import re
        
        # 查找字符串模式
        text_patterns = []
        resource_id_patterns = []
        activity_patterns = []
        
        # 查找包名模式
        package_pattern = rb'com\.[a-zA-Z0-9_.]+'
        packages = re.findall(package_pattern, data)
        for pkg in set(packages):
            try:
                activity_patterns.append(pkg.decode('utf-8'))
            except:
                pass
        
        # 查找资源ID模式
        resource_pattern = rb'[a-zA-Z0-9_]+:id/[a-zA-Z0-9_]+'
        resources = re.findall(resource_pattern, data)
        for res in set(resources):
            try:
                resource_id_patterns.append(res.decode('utf-8'))
            except:
                pass
        
        # 查找可能的文本内容
        text_pattern = rb'[\xe4-\xe9][\x80-\xbf][\x80-\xbf]+'
        texts = re.findall(text_pattern, data)
        for text in set(texts):
            try:
                text_patterns.append(text.decode('utf-8'))
            except:
                pass
        
        print(f'发现的文本模式 ({len(text_patterns)}个):')
        for i, text in enumerate(text_patterns):
            print(f'  [{i}] "{text}"')
        
        print(f'发现的资源ID模式 ({len(resource_id_patterns)}个):')
        for i, res_id in enumerate(resource_id_patterns):
            print(f'  [{i}] "{res_id}"')
        
        print(f'发现的Activity模式 ({len(activity_patterns)}个):')
        for i, activity in enumerate(activity_patterns):
            print(f'  [{i}] "{activity}"')
            
    except Exception as e:
        print(f'解析失败: {e}')

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('用法: python3 detailed_model_analysis.py <模型文件路径>')
        sys.exit(1)
    
    model_path = sys.argv[1]
    if not os.path.exists(model_path):
        print(f'错误: 模型文件不存在: {model_path}')
        sys.exit(1)
    
    analyze_model_detailed(model_path)
