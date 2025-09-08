/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */
/**
 * @authors 
 */
#ifndef WidgetIcon_H_
#define WidgetIcon_H_

#include <opencv2/opencv.hpp>
#include <memory>
#include <string>

namespace fastbotx {

class WidgetIcon {
public:
    WidgetIcon();
    explicit WidgetIcon(const std::string& base64Icon);
    ~WidgetIcon() = default;

    // 从Base64字符串加载图标
    bool loadFromBase64(const std::string& base64Icon);
    
    static std::vector<float> mat_to_tensor(const cv::Mat& image);

    // 获取图标
    cv::Mat getIcon() const;

    // 检查图标是否为空
    bool isEmpty() const;

    // 获取原始的base64字符串
    std::string getBase64String() const;

private:
    cv::Mat _icon;
    bool _isValid;
    std::string _base64String;  // 保存原始的base64字符串
    
    // 从Base64解码为图像
    static cv::Mat base64ToMat(const std::string& base64String);
    static std::vector<uchar> decodeBase64(const std::string& base64String);
    // 预处理图标尺寸
    cv::Mat preprocess_image(const cv::Mat& image);
};

using WidgetIconPtr = std::shared_ptr<WidgetIcon>;

} // namespace fastbotx

#endif // WidgetIcon_H_