/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */
/**
 * @authors 
 */
#ifndef ActionSimilarity_CPP_
#define ActionSimilarity_CPP_

#include "ActionSimilarity.h"
#include <opencv2/opencv.hpp>
#include <onnxruntime/onnxruntime_cxx_api.h>
#include <iostream>
#include <vector>
#include "../utils.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>

namespace fastbotx {

// 初始化静态成员变量
Ort::Session* ActionSimilarity::bertSession = nullptr;
std::vector<const char*> ActionSimilarity::bertInputNames;
std::vector<const char*> ActionSimilarity::bertOutputNames;
std::vector<int64_t> ActionSimilarity::bertInputShape;

Ort::Session* ActionSimilarity::clipSession = nullptr;
std::vector<const char*> ActionSimilarity::clipInputNames;
std::vector<const char*> ActionSimilarity::clipOutputNames;
std::vector<int64_t> ActionSimilarity::clipInputShape;

std::unordered_map<std::string, int64_t> ActionSimilarity::vocabMap;

// 计算余弦相似度的辅助函数
static float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    float dot_product = 0.0f;
    float norm_a = 0.0f, norm_b = 0.0f;

    for (size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    norm_a = std::sqrt(norm_a);
    norm_b = std::sqrt(norm_b);

    return dot_product / (norm_a * norm_b);
}

// 添加calculateCosineSimilarity函数定义
static double calculateCosineSimilarity(const float* a, const float* b, size_t size) {
    double dot_product = 0.0;
    double norm_a = 0.0, norm_b = 0.0;

    for (size_t i = 0; i < size; ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    norm_a = std::sqrt(norm_a);
    norm_b = std::sqrt(norm_b);

    return dot_product / (norm_a * norm_b);
}

ActionSimilarity::ActionSimilarity() {
    initializeModels();
    initializeVocab();
}

ActionSimilarity::~ActionSimilarity() {
    if (bertSession) {
        delete bertSession;
        bertSession = nullptr;
    }
    if (clipSession) {
        delete clipSession;
        clipSession = nullptr;
    }
}

void ActionSimilarity::initializeModels() {
    try {
        BLOG("开始初始化模型");
        
        // 创建共享的环境和会话选项
        static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "fastbot-models");
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(1);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        sessionOptions.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
        
        // 初始化BERT模型
        if (!bertSession) {
            BLOG("正在初始化BERT模型");
#ifdef __ANDROID__
            std::string bertModelPath = "/data/local/tmp/bert-base-uncased.onnx";
#else
            std::string bertModelPath = "/Users/atmo/program/Fastbot_Android_副本/native/desc/reuse/models/bert-base-uncased.onnx";
#endif
            BLOG("BERT模型路径: %s", bertModelPath.c_str());
            
            try {
                // 检查文件是否存在
                std::ifstream file(bertModelPath);
                if (!file.good()) {
                    throw std::runtime_error("BERT模型文件不存在或无法访问: " + bertModelPath);
                }
                
                bertSession = new Ort::Session(env, bertModelPath.c_str(), sessionOptions);
                if (!bertSession) {
                    throw std::runtime_error("BERT模型会话创建失败");
                }
                
                BLOG("BERT模型加载成功");
                
                // 设置输入输出名称
                bertInputNames = {"input_ids", "attention_mask", "token_type_ids"};
                bertOutputNames = {"last_hidden_state"};
                bertInputShape = {1, 512};
            } catch (const std::exception& e) {
                BLOGE("BERT模型加载失败: %s", e.what());
                if (bertSession) {
                    delete bertSession;
                    bertSession = nullptr;
                }
                throw;
            }
        }
        
        // 初始化CLIP模型
        if (!clipSession) {
            BLOG("正在初始化CLIP模型");
#ifdef __ANDROID__
            std::string clipModelPath = "/data/local/tmp/clip_image_encoder.onnx";
#else
            std::string clipModelPath = "/Users/atmo/program/Fastbot_Android_副本/native/desc/reuse/models/clip_image_encoder.onnx";
#endif
            BLOG("CLIP模型路径: %s", clipModelPath.c_str());
            
            try {
                // 检查文件是否存在
                std::ifstream file(clipModelPath);
                if (!file.good()) {
                    throw std::runtime_error("CLIP模型文件不存在或无法访问: " + clipModelPath);
                }
                
                clipSession = new Ort::Session(env, clipModelPath.c_str(), sessionOptions);
                if (!clipSession) {
                    throw std::runtime_error("CLIP模型会话创建失败");
                }
                
                BLOG("CLIP模型加载成功");
                
                // 获取模型输入和输出信息
                // Ort::AllocatorWithDefaultOptions allocator;
                // auto input_name_ptr = clipSession->GetInputNameAllocated(0, allocator);
                // auto output_name_ptr = clipSession->GetOutputNameAllocated(0, allocator);
                // if (!input_name_ptr || !output_name_ptr) {
                //     throw std::runtime_error("无法获取CLIP模型的输入输出名称");
                // }
                // clipInputNames = {input_name_ptr.get()};
                // clipOutputNames = {output_name_ptr.get()};
                // clipInputShape = {1, 3, 224, 224};
                // 强制指定输入输出名，防止乱码
                static const char* clip_input_name = "image";
                static const char* clip_output_name = "image_features";
                clipInputNames = {clip_input_name};
                clipOutputNames = {clip_output_name};
                clipInputShape = {1, 3, 224, 224};
            } catch (const std::exception& e) {
                BLOGE("CLIP模型加载失败: %s", e.what());
                if (clipSession) {
                    delete clipSession;
                    clipSession = nullptr;
                }
                throw;
            }
        }
        
        BLOG("所有模型初始化完成");
    } catch (const std::exception& e) {
        BLOGE("模型初始化过程中发生错误: %s", e.what());
        throw;
    }
}

void ActionSimilarity::initializeVocab() {
    if (!vocabMap.empty()) return;

    // 添加特殊token
    vocabMap["[UNK]"] = 100;
    vocabMap["[CLS]"] = 101;
    vocabMap["[SEP]"] = 102;
    vocabMap["[PAD]"] = 0;

    // 添加Android UI相关的常见词汇
    std::vector<std::string> commonWords = {
        // 基础UI元素
        "button", "text", "image", "view", "layout", "activity", "fragment",
        "dialog", "menu", "list", "item", "header", "footer", "content",
        "title", "description", "icon", "background", "foreground", "click",
        "press", "tap", "swipe", "scroll", "input", "edit", "search",
        "submit", "cancel", "confirm", "back", "next", "previous", "close",
        "open", "save", "delete", "add", "remove", "update", "refresh",
        "loading", "error", "success", "warning", "info", "help", "settings",
        
        // 更多UI元素类型
        "checkbox", "radiobutton", "switch", "slider", "progressbar", "spinner",
        "dropdown", "combobox", "calendar", "datepicker", "timepicker", "ratingbar",
        "seekbar", "toggle", "chip", "badge", "tooltip", "snackbar", "toast",
        "notification", "alert", "popup", "modal", "drawer", "bottom_sheet",
        "tab", "pager", "carousel", "card", "chip_group", "floating_action_button",
        
        // 动作类型
        "click", "long_click", "double_click", "swipe", "scroll", "drag", "drop",
        "pinch", "zoom", "rotate", "shake", "shake", "shake", "shake", "shake",
        "type", "select", "deselect", "check", "uncheck", "expand", "collapse",
        "show", "hide", "focus", "blur", "submit", "reset", "clear", "copy",
        "paste", "cut", "undo", "redo", "refresh", "reload", "download", "upload",
        
        // 状态描述词
        "enabled", "disabled", "visible", "invisible", "gone", "selected", "unselected",
        "checked", "unchecked", "expanded", "collapsed", "focused", "blurred",
        "active", "inactive", "pressed", "released", "hovered", "loading", "loaded",
        "error", "success", "warning", "info", "debug", "primary", "secondary",
        "tertiary", "default", "custom", "required", "optional", "valid", "invalid",
        
        // 布局相关
        "container", "wrapper", "group", "panel", "section", "row", "column",
        "grid", "list", "table", "form", "card", "dialog", "popup", "modal",
        "drawer", "bottom_sheet", "tab", "pager", "carousel", "stack", "queue",
        "tree", "graph", "map", "chart", "calendar", "timeline", "gallery",
        
        // 属性描述词
        "width", "height", "size", "position", "margin", "padding", "border",
        "radius", "color", "background", "foreground", "opacity", "visibility",
        "z_index", "rotation", "scale", "translation", "animation", "transition",
        "duration", "delay", "easing", "curve", "path", "gradient", "shadow",
        "blur", "brightness", "contrast", "saturation", "hue", "alpha", "beta"
    };

    // 添加Android包名和类名相关的词汇
    std::vector<std::string> androidWords = {
        // 基础Android组件
        "android", "widget", "view", "activity", "fragment", "dialog",
        "recycler", "list", "grid", "card", "material", "design",
        "support", "v7", "v4", "androidx", "appcompat", "constraint",
        "coordinator", "navigation", "bottom", "top", "drawer", "tab",
        "pager", "swipe", "refresh", "layout", "linear", "relative",
        "frame", "constraint", "scroll", "horizontal", "vertical",
        
        // 更多Android组件
        "appbar", "toolbar", "actionbar", "statusbar", "navigationbar",
        "bottomsheet", "snackbar", "toast", "notification", "alert",
        "popup", "modal", "drawer", "bottom_sheet", "tab", "pager",
        "carousel", "card", "chip", "badge", "tooltip", "snackbar",
        "toast", "notification", "alert", "popup", "modal", "drawer",
        
        // Android特有组件
        "recyclerview", "viewpager", "viewpager2", "swiperefreshlayout",
        "coordinatorlayout", "constraintlayout", "linearlayout", "relativelayout",
        "framelayout", "gridlayout", "tablelayout", "absolutelayout",
        "webview", "mapview", "surfaceview", "textureview", "glsurfaceview",
        "videoview", "imageview", "imagebutton", "checkbox", "radiobutton",
        "switch", "togglebutton", "seekbar", "progressbar", "ratingbar",
        "spinner", "autocompletetextview", "multiautocompletetextview",
        
        // Android Material Design组件
        "materialbutton", "materialcardview", "materialtextview",
        "materialtextinputlayout", "materialtextinputedittext",
        "materialcheckbox", "materialradiobutton", "materialswitch",
        "materialslider", "materialprogressbar", "materialratingbar",
        "materialspinner", "materialdropdown", "materialcombobox",
        "materialcalendar", "materialdatepicker", "materialtimepicker",
        "materialchip", "materialbadge", "materialtooltip", "materialsnackbar",
        "materialtoast", "materialnotification", "materialalert", "materialpopup",
        "materialmodal", "materialdrawer", "materialbottomsheet", "materialtab",
        "materialpager", "materialcarousel"
    };

    // 添加常见UI元素ID前缀
    std::vector<std::string> idPrefixes = {
        // 基础前缀
        "btn", "txt", "img", "iv", "tv", "et", "rv", "lv", "sv", "cb",
        "rb", "sw", "pb", "sb", "tb", "fab", "card", "item", "header",
        "footer", "content", "container", "wrapper", "group", "panel",
        
        // 更多前缀
        "act", "frag", "dlg", "menu", "list", "grid", "card", "item",
        "header", "footer", "content", "container", "wrapper", "group",
        "panel", "section", "row", "col", "cell", "icon", "img", "pic",
        "thumb", "avatar", "logo", "banner", "cover", "background", "bg",
        "foreground", "fg", "overlay", "mask", "shadow", "border", "divider",
        "separator", "spacer", "gap", "margin", "padding", "container", "wrapper",
        
        // 功能相关前缀
        "action", "menu", "nav", "tab", "pager", "carousel", "slider",
        "progress", "loading", "error", "success", "warning", "info",
        "help", "settings", "config", "pref", "option", "choice", "select",
        "input", "form", "field", "label", "hint", "placeholder", "value",
        "default", "custom", "required", "optional", "valid", "invalid",
        
        // 状态相关前缀
        "enabled", "disabled", "visible", "invisible", "gone", "selected",
        "unselected", "checked", "unchecked", "expanded", "collapsed",
        "focused", "blurred", "active", "inactive", "pressed", "released",
        "hovered", "loading", "loaded", "error", "success", "warning",
        "info", "debug", "primary", "secondary", "tertiary", "default",
        "custom", "required", "optional", "valid", "invalid"
    };

    // 将所有词汇添加到词汇表
    int64_t tokenId = 103; // 从103开始，因为0-102是特殊token
    for (const auto& word : commonWords) {
        vocabMap[word] = tokenId++;
    }
    for (const auto& word : androidWords) {
        if (vocabMap.find(word) == vocabMap.end()) {
            vocabMap[word] = tokenId++;
        }
    }
    for (const auto& prefix : idPrefixes) {
        if (vocabMap.find(prefix) == vocabMap.end()) {
            vocabMap[prefix] = tokenId++;
        }
    }
}

std::vector<std::string> ActionSimilarity::tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    
    // 处理空文本
    if (text.empty()) {
        return tokens;
    }

    // 将文本转换为小写
    std::string lowerText = text;
    std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);

    // 分割驼峰命名
    std::string processedText;
    for (size_t i = 0; i < lowerText.size(); ++i) {
        if (i > 0 && isupper(lowerText[i]) && islower(lowerText[i-1])) {
            processedText += '_';
        }
        processedText += tolower(lowerText[i]);
    }

    // 分割下划线和点号
    std::regex pattern("[_.]");
    std::string result = std::regex_replace(processedText, pattern, " ");

    // 分割空格
    std::istringstream iss(result);
    std::string token;
    while (iss >> token) {
        // 移除特殊字符
        token = std::regex_replace(token, std::regex("[^a-z0-9]"), "");
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }

    return tokens;
}

std::vector<int64_t> ActionSimilarity::convertTokensToIds(const std::vector<std::string>& tokens) {
    std::vector<int64_t> ids;
    ids.reserve(tokens.size());

    for (const auto& token : tokens) {
        auto it = vocabMap.find(token);
        if (it != vocabMap.end()) {
            ids.push_back(it->second);
        } else {
            ids.push_back(100);
        }
    }

    return ids;
}

std::vector<int64_t> ActionSimilarity::preprocessText(const std::string& text) {
    // 添加[CLS]标记
    std::vector<int64_t> ids = {101};
    
    // 分词并转换为ID
    auto tokens = tokenize(text);
    auto tokenIds = convertTokensToIds(tokens);
    ids.insert(ids.end(), tokenIds.begin(), tokenIds.end());
    
    // 添加[SEP]标记
    ids.push_back(102);
    
    // 填充到最大长度
    while (ids.size() < bertInputShape[1]) {
        ids.push_back(0);
    }
    
    // 截断到最大长度
    if (ids.size() > bertInputShape[1]) {
        ids.resize(bertInputShape[1]);
        ids[bertInputShape[1] - 1] = 102;
    }
    
    return ids;
}

std::vector<float> ActionSimilarity::getBertEmbedding(const std::string& text) {
    if (!bertSession) {
        BLOGE("BERT模型未初始化");
        // 尝试重新初始化模型
        try {
            initializeModels();
            if (!bertSession) {
                BLOGE("BERT模型重新初始化失败");
                return std::vector<float>();
            }
        } catch (const std::exception& e) {
            BLOGE("BERT模型重新初始化异常: %s", e.what());
            return std::vector<float>();
        }
    }

    try {
        // 预处理文本
        std::vector<int64_t> inputIds = preprocessText(text);
        std::vector<int64_t> attentionMask(bertInputShape[1], 1);
        std::vector<int64_t> tokenTypeIds(bertInputShape[1], 0);

        // 创建输入tensor
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<Ort::Value> inputTensors;
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(memoryInfo, inputIds.data(), inputIds.size(), bertInputShape.data(), bertInputShape.size()));
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(memoryInfo, attentionMask.data(), attentionMask.size(), bertInputShape.data(), bertInputShape.size()));
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(memoryInfo, tokenTypeIds.data(), tokenTypeIds.size(), bertInputShape.data(), bertInputShape.size()));

        // 运行模型
        auto outputTensors = bertSession->Run(Ort::RunOptions{nullptr}, bertInputNames.data(), inputTensors.data(), inputTensors.size(), bertOutputNames.data(), bertOutputNames.size());

        // 获取输出向量
        float* outputData = outputTensors[0].GetTensorMutableData<float>();
        size_t outputSize = outputTensors[0].GetTensorTypeAndShapeInfo().GetElementCount();
        
        // 对输出向量进行平均池化得到句子向量
        std::vector<float> embedding(outputSize / bertInputShape[1], 0.0f);
        for (size_t i = 0; i < outputSize / bertInputShape[1]; ++i) {
            for (size_t j = 0; j < bertInputShape[1]; ++j) {
                embedding[i] += outputData[j * (outputSize / bertInputShape[1]) + i];
            }
            embedding[i] /= bertInputShape[1];
        }

        return embedding;
    } catch (const std::exception& e) {
        std::cerr << "获取BERT嵌入向量失败: " << e.what() << std::endl;
        return std::vector<float>();
    }
}

double ActionSimilarity::calculateTextSimilarity(const std::string& text1, const std::string& text2) {
    // 如果两个文本都为空，认为它们相似
    if (text1.empty() && text2.empty()) {
        return 1.0;
    }
    
    // 如果只有一个为空，认为它们不相似
    if (text1.empty() || text2.empty()) {
        return 0.0;
    }
    
    // 使用BERT模型计算相似度
    auto embedding1 = getBertEmbedding(text1);
    auto embedding2 = getBertEmbedding(text2);
    
    if (embedding1.empty() || embedding2.empty()) {
        // 如果BERT模型失败，回退到编辑距离
        return calculateTextSimilarity(text1, text2);
    }
    
    return cosine_similarity(embedding1, embedding2);
}

double ActionSimilarity::calculateResourceIdSimilarity(const std::string& id1, const std::string& id2) {
    // 如果两个ID都为空，认为它们相似
    if (id1.empty() && id2.empty()) {
        return 1.0;
    }
    
    // 如果只有一个为空，认为它们不相似
    if (id1.empty() || id2.empty()) {
        return 0.0;
    }
    
    // 使用BERT模型计算相似度
    auto embedding1 = getBertEmbedding(id1);
    auto embedding2 = getBertEmbedding(id2);
    
    if (embedding1.empty() || embedding2.empty()) {
        // 如果BERT模型失败，回退到字符串比较
        if (id1 == id2) {
            return 1.0;
        }
        if (id1.find(id2) != std::string::npos || id2.find(id1) != std::string::npos) {
            return 0.8;
        }
        return 0.0;
    }
    
    return cosine_similarity(embedding1, embedding2);
}

double ActionSimilarity::calculateActivitySimilarity(const std::string& activity1, const std::string& activity2) {
    // 如果两个活动名称都为空，认为它们相似
    if (activity1.empty() && activity2.empty()) {
        return 1.0;
    }
    
    // 如果只有一个为空，认为它们不相似
    if (activity1.empty() || activity2.empty()) {
        return 0.0;
    }
    
    // 使用BERT模型计算相似度
    auto embedding1 = getBertEmbedding(activity1);
    auto embedding2 = getBertEmbedding(activity2);
    
    if (embedding1.empty() || embedding2.empty()) {
        // 如果BERT模型失败，回退到精确匹配
        return (activity1 == activity2) ? 1.0 : 0.0;
    }
    
    return cosine_similarity(embedding1, embedding2);
}

double ActionSimilarity::calculateSimilarity(const ActivityNameActionPtr& action1, const ActivityNameActionPtr& action2) {
    if (!action1 || !action2) {
        return 0.0;
    }
    
    // 如果动作类型不同，直接返回0
    if (action1->getActionType() != action2->getActionType()) {
        return 0.0;
    }
    
    // 获取widget
    const auto& widget1 = action1->getTarget();
    const auto& widget2 = action2->getTarget();
    
    // 如果一个有widget而另一个没有，直接返回0
    if ((widget1 == nullptr && widget2 != nullptr) || (widget1 != nullptr && widget2 == nullptr)) {
        return 0.0;
    }
    
    // 如果都没有widget（比如BACK操作），只比较activity
    if (widget1 == nullptr && widget2 == nullptr) {
        return calculateActivitySimilarity(*action1->getActivity(), *action2->getActivity());
    }
    
    // 计算各个属性的相似度
    double textSim = calculateTextSimilarity(widget1->getText(), widget2->getText());
    BLOG("text相似度: %f", textSim);
    double resourceIdSim = calculateResourceIdSimilarity(widget1->getResourceID(), widget2->getResourceID());
    BLOG("resourceId相似度: %f", resourceIdSim);
    double activitySim = calculateActivitySimilarity(*action1->getActivity(), *action2->getActivity());
    BLOG("activity相似度: %f", activitySim);
    
    // 计算图标相似度
    double iconSim = 0.0;
    if (widget1->hasIcon() && widget2->hasIcon()) {
        iconSim = calculateIconSimilarity(widget1->getIcon(), widget2->getIcon());//这里获取的是WidgetIconPtr类型，需要转换为cv::Mat类型
        BLOG("icon相似度: %f", iconSim);
    }
    
    // 加权平均计算最终相似度
    // 权重可以根据实际需求调整
    double weightText = 0.35;
    double weightResourceId = 0.2;
    double weightActivity = 0.1;
    double weightIcon = 0.35;
    
    // 如果没有图标，调整其他权重
    if (!widget1->hasIcon() || !widget2->hasIcon()) {
        weightText = 0.4;
        weightResourceId = 0.2;
        weightActivity = 0.4;
        weightIcon = 0.0;
    }
    
    double similarity = weightText * textSim + 
                        weightResourceId * resourceIdSim + 
                        weightActivity * activitySim + 
                        weightIcon * iconSim;
    BLOG("最终相似度: %f", similarity);
    return similarity;
}

bool ActionSimilarity::isSimilar(const ActivityNameActionPtr& action1, const ActivityNameActionPtr& action2, double threshold) {
    return calculateSimilarity(action1, action2) >= threshold;
}

double ActionSimilarity::calculateIconSimilarity(const WidgetIconPtr& icon1, const WidgetIconPtr& icon2) {
    if (!icon1 || !icon2 || icon1->isEmpty() || icon2->isEmpty()) {
        BLOGE("图标数据无效，无法计算相似度");
        return 0.0;
    }

    try {
        BLOG("开始计算图标相似度");
        
        // 确保模型已初始化
        if (!clipSession) {
            BLOG("CLIP模型未初始化，开始初始化");
            initializeModels();
        }

        // 获取图标数据
        cv::Mat img1 = icon1->getIcon();
        cv::Mat img2 = icon2->getIcon();
        BLOG("获取到两个图标的图像数据，尺寸分别为: %dx%d 和 %dx%d", 
              img1.cols, img1.rows, img2.cols, img2.rows);

        // 转换为张量
        std::vector<float> tensor1 = WidgetIcon::mat_to_tensor(img1);
        std::vector<float> tensor2 = WidgetIcon::mat_to_tensor(img2);
        BLOG("图像数据转换为张量完成，大小分别为: %zu 和 %zu", 
              tensor1.size(), tensor2.size());

        // 创建输入张量
        std::vector<Ort::Value> inputTensors1;
        std::vector<Ort::Value> inputTensors2;
        
        // 设置输入形状
        std::vector<int64_t> inputShape = {1, 3, 224, 224};
        
        // 创建内存分配器
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);

        // 创建输入张量
        inputTensors1.push_back(Ort::Value::CreateTensor<float>(
            memoryInfo, tensor1.data(), tensor1.size(),
            inputShape.data(), inputShape.size()));
            
        inputTensors2.push_back(Ort::Value::CreateTensor<float>(
            memoryInfo, tensor2.data(), tensor2.size(),
            inputShape.data(), inputShape.size()));
            
        BLOG("创建输入张量完成");

        // 运行模型
        auto output1 = clipSession->Run(
            Ort::RunOptions{nullptr}, 
            clipInputNames.data(), 
            inputTensors1.data(), 
            1, 
            clipOutputNames.data(), 
            1);
            
        auto output2 = clipSession->Run(
            Ort::RunOptions{nullptr}, 
            clipInputNames.data(), 
            inputTensors2.data(), 
            1, 
            clipOutputNames.data(), 
            1);
            
        BLOG("模型推理完成");

        // 获取输出数据
        float* outputData1 = output1[0].GetTensorMutableData<float>();
        float* outputData2 = output2[0].GetTensorMutableData<float>();
        
        // 计算余弦相似度
        double similarity = calculateCosineSimilarity(
            outputData1, outputData2, output1[0].GetTensorTypeAndShapeInfo().GetElementCount());
            
        return similarity;
    } catch (const std::exception& e) {
        BLOGE("计算图标相似度时发生错误: %s", e.what());
        return 0.0;
    }
}

} // namespace fastbotx

#endif // ActionSimilarity_CPP_