#!/bin/bash

echo "上传BERT和CLIP模型到设备..."
echo "这可能需要几分钟时间，请耐心等待..."

# 上传BERT模型
echo "上传BERT模型..."
adb push native/desc/reuse/models/bert-base-uncased.onnx /sdcard/

# 上传CLIP模型
echo "上传CLIP模型..."
adb push native/desc/reuse/models/clip_image_encoder.onnx /sdcard/

# 验证文件是否上传成功
echo "验证文件是否上传成功..."
adb shell ls -l /sdcard/bert-base-uncased.onnx /sdcard/clip_image_encoder.onnx

echo "模型上传完成，现在可以运行测试了。"
