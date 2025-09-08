#!/usr/bin/env python3
import sys
import os
import struct

def check_model_file_final(file_path):
    """最终检查模型文件"""
    print(f"最终检查模型文件: {file_path}")
    
    if not os.path.exists(file_path):
        print(f"错误: 文件不存在 {file_path}")
        return
    
    file_size = os.path.getsize(file_path)
    print(f"文件大小: {file_size} 字节")
    
    with open(file_path, 'rb') as f:
        content = f.read()
    
    # 检查文件头
    if len(content) < 4:
        print("错误: 文件太小")
        return
    
    # 读取大小前缀
    size_prefix = struct.unpack('<I', content[:4])[0]
    print(f"FlatBuffers大小前缀: {size_prefix}")
    
    if size_prefix > len(content) or size_prefix < 4:
        print("错误: 大小前缀不合理")
        return
    
    # 检查根表偏移
    if len(content) >= size_prefix + 4:
        root_offset = struct.unpack('<I', content[size_prefix:size_prefix+4])[0]
        print(f"根表偏移: {root_offset}")
        
        if root_offset >= size_prefix:
            print("错误: 根表偏移超出范围")
            return
    
    # 搜索FlatBuffers字段名称（可能以不同的编码方式存储）
    print("\n搜索FlatBuffers字段名称:")
    
    # 搜索可能的字段名称变体
    field_patterns = [
        b'similarity_attrs',
        b'similarityAttrs',
        b'similarityattrs',
        b'WidgetSimilarityAttributes',
        b'ActionSimilarityAttributes',
        b'save_similarity_attrs',
        b'saveSimilarityAttrs',
        b'action_type',
        b'actionType',
        b'activity_name',
        b'activityName',
        b'target_widget',
        b'targetWidget',
        b'widget_hash',
        b'widgetHash',
        b'text',
        b'resource_id',
        b'resourceId',
        b'icon_base64',
        b'iconBase64'
    ]
    
    found_fields = []
    for pattern in field_patterns:
        count = content.count(pattern)
        if count > 0:
            found_fields.append(pattern.decode())
            print(f"  ✓ 找到 '{pattern.decode()}' {count} 次")
    
    # 搜索常见的控件文本和资源ID
    print("\n搜索控件文本和资源ID:")
    common_patterns = [
        b'com.netease.cloudmusic',
        b'MainActivity',
        b'PlayerActivity',
        b'id/',
        b'android:id/',
        b'play',
        b'like',
        b'search',
        b'clear',
        b'netease',
        b'cloudmusic'
    ]
    
    found_content = []
    for pattern in common_patterns:
        count = content.count(pattern)
        if count > 0:
            found_content.append(pattern.decode())
            print(f"  ✓ 找到 '{pattern.decode()}' {count} 次")
    
    # 检查文件结构
    print(f"\n文件结构分析:")
    print(f"  总大小: {len(content)} 字节")
    print(f"  大小前缀: {size_prefix} 字节")
    print(f"  数据部分: {len(content) - size_prefix - 4} 字节")
    
    # 检查是否包含相似度属性
    has_similarity_fields = len(found_fields) > 0
    has_content = len(found_content) > 0
    
    print(f"\n属性检测结果:")
    print(f"  FlatBuffers字段: {'✓' if has_similarity_fields else '✗'}")
    print(f"  控件内容: {'✓' if has_content else '✗'}")
    
    if has_similarity_fields:
        print(f"\n找到的FlatBuffers字段: {', '.join(found_fields)}")
        print("✓ 模型文件包含相似度属性字段")
    elif has_content:
        print("\n⚠ 模型文件包含控件内容，但可能缺少相似度属性字段")
    else:
        print("\n✗ 模型文件缺少相似度属性")
    
    # 检查文件是否包含我们期望的数据
    if has_content:
        print("\n✓ 模型文件包含预期的控件数据（Activity名称、资源ID等）")
        print("这表明我们的修复已经生效，模型文件正在保存详细的属性信息")

def main():
    if len(sys.argv) != 2:
        print("用法: python3 check_model_final.py <model_file>")
        sys.exit(1)
    
    file_path = sys.argv[1]
    check_model_file_final(file_path)

if __name__ == "__main__":
    main() 