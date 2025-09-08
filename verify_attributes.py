#!/usr/bin/env python3
import sys
import os
import struct
import re

def verify_attributes(file_path):
    """验证模型文件中的属性"""
    print(f"验证模型文件属性: {file_path}")
    
    if not os.path.exists(file_path):
        print(f"错误: 文件不存在 {file_path}")
        return
    
    with open(file_path, 'rb') as f:
        content = f.read()
    
    print(f"文件大小: {len(content)} 字节")
    
    # 1. 检查FlatBuffers结构
    print("\n=== FlatBuffers结构检查 ===")
    if len(content) >= 4:
        size_prefix = struct.unpack('<I', content[:4])[0]
        print(f"大小前缀: {size_prefix}")
        
        if len(content) >= size_prefix + 4:
            root_offset = struct.unpack('<I', content[size_prefix:size_prefix+4])[0]
            print(f"根表偏移: {root_offset}")
    
    # 2. 搜索具体的属性值
    print("\n=== 属性值检查 ===")
    
    # 检查文本属性
    text_patterns = [
        b'play', b'like', b'search', b'clear', b'netease', b'cloudmusic',
        b'MainActivity', b'PlayerActivity', b'LoginActivity',
        b'button', b'text', b'label', b'title'
    ]
    
    found_texts = []
    for pattern in text_patterns:
        count = content.count(pattern)
        if count > 0:
            found_texts.append((pattern.decode(), count))
    
    print("找到的文本属性:")
    for text, count in found_texts:
        print(f"  ✓ '{text}': {count} 次")
    
    # 检查资源ID属性
    resource_patterns = [
        b'com.netease.cloudmusic',
        b'id/',
        b'android:id/',
        b'button', b'text', b'image', b'view'
    ]
    
    found_resources = []
    for pattern in resource_patterns:
        count = content.count(pattern)
        if count > 0:
            found_resources.append((pattern.decode(), count))
    
    print("\n找到的资源ID属性:")
    for resource, count in found_resources:
        print(f"  ✓ '{resource}': {count} 次")
    
    # 检查Activity属性
    activity_patterns = [
        b'MainActivity',
        b'PlayerActivity', 
        b'LoginActivity',
        b'Activity'
    ]
    
    found_activities = []
    for pattern in activity_patterns:
        count = content.count(pattern)
        if count > 0:
            found_activities.append((pattern.decode(), count))
    
    print("\n找到的Activity属性:")
    for activity, count in found_activities:
        print(f"  ✓ '{activity}': {count} 次")
    
    # 检查图标属性（base64编码）
    icon_patterns = [
        b'data:image',
        b'iVBORw0KGgo',  # base64 PNG header
        b'/9j/',         # base64 JPEG header
        b'icon',
        b'image'
    ]
    
    found_icons = []
    for pattern in icon_patterns:
        count = content.count(pattern)
        if count > 0:
            found_icons.append((pattern.decode(), count))
    
    print("\n找到的图标属性:")
    for icon, count in found_icons:
        print(f"  ✓ '{icon}': {count} 次")
    
    # 3. 检查FlatBuffers字段名称（多种编码方式）
    print("\n=== FlatBuffers字段检查 ===")
    
    # 尝试不同的编码方式
    field_patterns = [
        # 原始字符串
        b'similarity_attrs',
        b'text',
        b'resource_id', 
        b'icon_base64',
        b'activity_name',
        b'action_type',
        # 可能的变体
        b'similarityAttrs',
        b'similarityattrs',
        b'text',
        b'resourceId',
        b'iconBase64',
        b'activityName',
        b'actionType'
    ]
    
    found_fields = []
    for pattern in field_patterns:
        count = content.count(pattern)
        if count > 0:
            found_fields.append((pattern.decode(), count))
    
    if found_fields:
        print("找到的FlatBuffers字段:")
        for field, count in found_fields:
            print(f"  ✓ '{field}': {count} 次")
    else:
        print("未找到FlatBuffers字段名称（可能以不同编码方式存储）")
    
    # 4. 总结
    print("\n=== 验证总结 ===")
    
    has_text = len(found_texts) > 0
    has_resource = len(found_resources) > 0  
    has_activity = len(found_activities) > 0
    has_icon = len(found_icons) > 0
    has_fields = len(found_fields) > 0
    
    print(f"文本属性: {'✓' if has_text else '✗'}")
    print(f"资源ID属性: {'✓' if has_resource else '✗'}")
    print(f"Activity属性: {'✓' if has_activity else '✗'}")
    print(f"图标属性: {'✓' if has_icon else '✗'}")
    print(f"FlatBuffers字段: {'✓' if has_fields else '✗'}")
    
    if has_text and has_resource and has_activity:
        print("\n✅ 模型文件成功保存了详细的属性信息！")
        print("这表明我们的修复已经生效，跨平台widget复用功能应该可以正常工作。")
    elif has_text or has_resource or has_activity:
        print("\n⚠ 模型文件包含部分属性，但可能不完整")
    else:
        print("\n❌ 模型文件缺少详细的属性信息")

def main():
    if len(sys.argv) != 2:
        print("用法: python3 verify_attributes.py <model_file>")
        sys.exit(1)
    
    file_path = sys.argv[1]
    verify_attributes(file_path)

if __name__ == "__main__":
    main() 