/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */
/**
 * @authors 
 */
#ifndef WidgetIcon_CPP_
#define WidgetIcon_CPP_

#include "WidgetIcon.h"
#include <opencv2/opencv.hpp>
#include <base64.h>
#include <string>
#include <vector>
#include <stdexcept>
#include "../utils.hpp"

namespace fastbotx {

WidgetIcon::WidgetIcon() : _isValid(false) {
}

WidgetIcon::WidgetIcon(const std::string& base64Icon) : _isValid(false) {
    loadFromBase64(base64Icon);
}

bool WidgetIcon::loadFromBase64(const std::string& base64Icon) {
    if (base64Icon.empty()) {
        BLOGE("Base64 icon string is empty");
        _isValid = false;
        return false;
    }
    
    try {
        BLOG("开始加载Base64图标数据，长度: %zu", base64Icon.length());
        //BLOG("Base64字符串内容: %s", base64Icon.c_str());
        _icon = base64ToMat(base64Icon);
        _isValid = !_icon.empty();
        if (_isValid) {
            BLOG("Base64解码成功，图像尺寸: %dx%d", _icon.cols, _icon.rows);
            _icon = preprocess_image(_icon);
            BLOG("图像预处理完成，新尺寸: %dx%d", _icon.cols, _icon.rows);
        } else {
            BLOGE("Base64解码失败，生成的图像为空");
        }
        return _isValid;
    } catch (const std::exception& e) {
        BLOGE("加载图标失败: %s", e.what());
        _isValid = false;
        return false;
    }
}

cv::Mat WidgetIcon::preprocess_image(const cv::Mat& image) {
    if (image.empty()) {
        BLOGE("预处理失败: 输入图像为空");
        throw std::runtime_error("无法加载图像: 图像为空");
    }

    BLOG("开始预处理图像，原始尺寸: %dx%d", image.cols, image.rows);
    
    // 创建一个新的 Mat 对象，存储调整大小后的图像
    cv::Mat resized_image;
    cv::resize(image, resized_image, cv::Size(224, 224));
    BLOG("图像调整大小完成: %dx%d", resized_image.cols, resized_image.rows);

    // 转换为浮点类型并归一化
    resized_image.convertTo(resized_image, CV_32F, 1.0 / 255.0);
    BLOG("图像归一化完成");

    // 转换为 RGB 格式
    cv::Mat rgb_image;
    cv::cvtColor(resized_image, rgb_image, cv::COLOR_BGR2RGB);
    BLOG("图像转换为RGB格式完成");

    // 转换为 CHW 格式（通道分离和合并）
    std::vector<cv::Mat> channels(3);
    cv::split(rgb_image, channels);
    cv::Mat chw_image;
    cv::merge(channels, chw_image);
    BLOG("图像转换为CHW格式完成");

    return chw_image;
}

std::vector<float> WidgetIcon::mat_to_tensor(const cv::Mat& image) {
    std::vector<float> tensor_values;
    tensor_values.assign((float*)image.datastart, (float*)image.dataend);
    return tensor_values;
}

cv::Mat WidgetIcon::getIcon() const {
    return _icon;
}

bool WidgetIcon::isEmpty() const {
    return !_isValid || _icon.empty();
}

cv::Mat WidgetIcon::base64ToMat(const std::string& base64String) {
    BLOG("开始解码Base64字符串");
    
    // 解码 Base64 字符串为二进制数据
    std::vector<uchar> decodedData = decodeBase64(base64String);
    BLOG("Base64解码完成，数据大小: %zu bytes", decodedData.size());

    // 检查解码是否成功
    if (decodedData.empty()) {
        BLOGE("Base64解码失败，数据为空");
        throw std::runtime_error("Failed to decode Base64 string.");
    }

    // 将二进制数据转换为 OpenCV Mat
    cv::Mat img = cv::imdecode(decodedData, cv::IMREAD_COLOR);
    if (img.empty()) {
        BLOGE("图像解码失败");
        throw std::runtime_error("Failed to decode image from binary data.");
    }

    BLOG("图像解码成功，尺寸: %dx%d", img.cols, img.rows);
    return img;
}

std::vector<uchar> WidgetIcon::decodeBase64(const std::string& base64String) {
    // 检查并去除常见的base64前缀
    std::string::size_type pos = base64String.find(",");
    std::string pureBase64;
    if (base64String.substr(0, 5) == "data:") {
        // 形如 data:image/png;base64,xxxx
        if (pos != std::string::npos) {
            pureBase64 = base64String.substr(pos + 1);
            //BLOG("检测到data URI前缀，已去除。处理后的Base64字符串: %s", pureBase64.c_str());
            BLOG("检测到data URI前缀，已去除。处理后的Base64字符串长度: %zu", pureBase64.length());
        } else {
            pureBase64 = base64String;
            //BLOG("检测到data URI前缀但未找到分隔符，使用原始字符串: %s", pureBase64.c_str());
            BLOG("检测到data URI前缀但未找到分隔符，使用原始字符串长度: %zu", pureBase64.length());
        }
    } else {
        pureBase64 = base64String;
        //BLOG("未检测到data URI前缀，使用原始字符串: %s", pureBase64.c_str());
        BLOG("未检测到data URI前缀，使用原始字符串长度: %zu", pureBase64.length());
    }

    // 清理所有空白字符
    pureBase64.erase(
        std::remove_if(pureBase64.begin(), pureBase64.end(),
            [](unsigned char x){ return std::isspace(x); }),
        pureBase64.end()
    );
    BLOG("去除空白字符后Base64长度: %zu", pureBase64.length());

    // 自动补齐填充符号
    while (pureBase64.length() % 4 != 0) {
        pureBase64 += '=';
    }
    BLOG("补齐填充后Base64长度: %zu", pureBase64.length());

    // 使用 base64_decode 得到字符串
    std::string decodedStr = base64_decode(pureBase64);
    BLOG("Base64解码后的字符串长度: %zu", decodedStr.length());
    BLOG("Base64解码后的字符串内容: %s", decodedStr.c_str());

    // 将 string 转换为 vector<unsigned char>
    return std::vector<uchar>(decodedStr.begin(), decodedStr.end());
}

} // namespace fastbotx

#endif // WidgetIcon_CPP_