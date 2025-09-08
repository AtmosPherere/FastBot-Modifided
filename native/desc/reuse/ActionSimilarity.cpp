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
    // 检查向量是否为空
    if (a.empty() || b.empty()) {
        return 0.0f;
    }
    
    // 检查向量长度是否相同
    if (a.size() != b.size()) {
        return 0.0f;
    }
    
    float dot_product = 0.0f;
    float norm_a = 0.0f, norm_b = 0.0f;

    for (size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    norm_a = std::sqrt(norm_a);
    norm_b = std::sqrt(norm_b);

    // 检查是否为零向量
    if (norm_a == 0.0f || norm_b == 0.0f) {
        return 0.0f;
    }

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
            BLOG("检查BERT模型路径: %s", bertModelPath.c_str());
            
            // 检查文件是否存在
            std::ifstream bertFile(bertModelPath);
            if (!bertFile.good()) {
                BLOGE("BERT模型文件不存在或无法访问: %s", bertModelPath.c_str());
                // 尝试在其他位置查找
                bertModelPath = "/sdcard/bert-base-uncased.onnx";
                BLOG("尝试备用BERT模型路径: %s", bertModelPath.c_str());
                
                std::ifstream bertFile2(bertModelPath);
                if (!bertFile2.good()) {
                    BLOGE("备用BERT模型文件也不存在或无法访问");
                    throw std::runtime_error("找不到BERT模型文件");
                }
            }
#else
            std::string bertModelPath = "/Users/atmo/program/Fastbot_Android_副本/native/desc/reuse/models/bert-base-multilingual-cased.onnx";
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
            BLOG("检查CLIP模型路径: %s", clipModelPath.c_str());
            
            // 检查文件是否存在
            std::ifstream clipFile(clipModelPath);
            if (!clipFile.good()) {
                BLOGE("CLIP模型文件不存在或无法访问: %s", clipModelPath.c_str());
                // 尝试在其他位置查找
                clipModelPath = "/sdcard/clip_image_encoder.onnx";
                BLOG("尝试备用CLIP模型路径: %s", clipModelPath.c_str());
                
                std::ifstream clipFile2(clipModelPath);
                if (!clipFile2.good()) {
                    BLOGE("备用CLIP模型文件也不存在或无法访问");
                    throw std::runtime_error("找不到CLIP模型文件");
                }
            }
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

    BLOG("开始加载官方BERT词汇表");
    
    // 加载官方BERT词汇表
    std::string vocabPath = "/Users/atmo/program/Fastbot_Android_副本/vocab.txt";
    std::ifstream vocabFile(vocabPath);
    
    if (!vocabFile.is_open()) {
        BLOGE("无法打开词汇表文件: %s", vocabPath.c_str());
        // 回退到简单的词汇表
        vocabMap["[UNK]"] = UNK_TOKEN_ID;
        vocabMap["[CLS]"] = CLS_TOKEN_ID;
        vocabMap["[SEP]"] = SEP_TOKEN_ID;
        vocabMap["[PAD]"] = PAD_TOKEN_ID;
        return;
    }
    
    std::string line;
    int64_t lineNumber = 0;
    while (std::getline(vocabFile, line)) {
        lineNumber++;
        
        // 跳过空行
        if (line.empty()) continue;
        
        // 词汇表格式：每行一个token，行号就是ID
        vocabMap[line] = lineNumber - 1;  // 行号从1开始，ID从0开始
    }
    
    vocabFile.close();
    BLOG("成功加载词汇表，共%d个token", static_cast<int>(vocabMap.size()));
    
    // 验证特殊token
    if (vocabMap.find("[UNK]") != vocabMap.end()) {
        BLOG("UNK token ID: %lld", vocabMap["[UNK]"]);
    }
    if (vocabMap.find("[CLS]") != vocabMap.end()) {
        BLOG("CLS token ID: %lld", vocabMap["[CLS]"]);
    }
    if (vocabMap.find("[SEP]") != vocabMap.end()) {
        BLOG("SEP token ID: %lld", vocabMap["[SEP]"]);
    }
    if (vocabMap.find("[PAD]") != vocabMap.end()) {
        BLOG("PAD token ID: %lld", vocabMap["[PAD]"]);
    }
}

std::vector<std::string> ActionSimilarity::tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    
    // 处理空文本
    if (text.empty()) {
        return tokens;
    }

    BLOG("开始分词: '%s'", text.c_str());

    // 使用WordPiece分词（简化版本）
    // 1. 首先按空格和标点符号分割
    std::vector<std::string> words;
    std::string currentWord;
    
    for (char c : text) {
        if (std::isspace(c) || c == '.' || c == '_' || c == ':' || c == '/' || c == '\\') {
            if (!currentWord.empty()) {
                words.push_back(currentWord);
                currentWord.clear();
            }
        } else {
            currentWord += c;
        }
    }
    if (!currentWord.empty()) {
        words.push_back(currentWord);
    }

    // 2. 对每个词进行WordPiece分词
    for (const std::string& word : words) {
        if (word.empty()) continue;
        
        // 检查是否在词汇表中
        if (vocabMap.find(word) != vocabMap.end()) {
            tokens.push_back(word);
            BLOG("找到完整词: '%s'", word.c_str());
        } else {
            // 进行WordPiece分词
            std::vector<std::string> subTokens = wordPieceTokenize(word);
            tokens.insert(tokens.end(), subTokens.begin(), subTokens.end());
        }
    }

    BLOG("分词结果: [%s]", [&tokens]() {
        std::string result;
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i > 0) result += ", ";
            result += "'" + tokens[i] + "'";
        }
        return result;
    }().c_str());

    return tokens;
}

// 辅助函数：WordPiece分词
std::vector<std::string> ActionSimilarity::wordPieceTokenize(const std::string& word) {
    std::vector<std::string> tokens;
    
    // 如果词太短，直接返回
    if (word.length() <= 2) {
        tokens.push_back(word);
        return tokens;
    }
    
    // 尝试从最长子串开始匹配
    for (size_t len = word.length(); len > 0; --len) {
        for (size_t start = 0; start <= word.length() - len; ++start) {
            std::string subword = word.substr(start, len);
            
            // 检查是否在词汇表中
            if (vocabMap.find(subword) != vocabMap.end()) {
                tokens.push_back(subword);
                
                // 递归处理剩余部分
                if (start > 0) {
                    std::string left = word.substr(0, start);
                    auto leftTokens = wordPieceTokenize(left);
                    tokens.insert(tokens.begin(), leftTokens.begin(), leftTokens.end());
                }
                
                if (start + len < word.length()) {
                    std::string right = word.substr(start + len);
                    auto rightTokens = wordPieceTokenize(right);
                    tokens.insert(tokens.end(), rightTokens.begin(), rightTokens.end());
                }
                
                return tokens;
            }
        }
    }
    
    // 如果没有找到匹配，返回原词
    tokens.push_back(word);
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
            // 使用词汇表中的UNK token ID，如果找不到则使用默认值
            auto unkIt = vocabMap.find("[UNK]");
            if (unkIt != vocabMap.end()) {
                ids.push_back(unkIt->second);
            } else {
                ids.push_back(UNK_TOKEN_ID);
            }
        }
    }

    return ids;
}

std::vector<int64_t> ActionSimilarity::preprocessText(const std::string& text) {
    // 获取特殊token的ID
    int64_t clsId = CLS_TOKEN_ID;
    int64_t sepId = SEP_TOKEN_ID;
    int64_t padId = PAD_TOKEN_ID;
    
    // 如果词汇表已加载，使用词汇表中的ID
    if (!vocabMap.empty()) {
        auto clsIt = vocabMap.find("[CLS]");
        if (clsIt != vocabMap.end()) clsId = clsIt->second;
        
        auto sepIt = vocabMap.find("[SEP]");
        if (sepIt != vocabMap.end()) sepId = sepIt->second;
        
        auto padIt = vocabMap.find("[PAD]");
        if (padIt != vocabMap.end()) padId = padIt->second;
    }
    
    // 添加[CLS]标记
    std::vector<int64_t> ids = {clsId};
    
    // 分词并转换为ID
    auto tokens = tokenize(text);
    auto tokenIds = convertTokensToIds(tokens);
    ids.insert(ids.end(), tokenIds.begin(), tokenIds.end());
    
    // 添加[SEP]标记
    ids.push_back(sepId);
    
    // 填充到最大长度
    while (ids.size() < bertInputShape[1]) {
        ids.push_back(padId);
    }
    
    // 截断到最大长度
    if (ids.size() > bertInputShape[1]) {
        ids.resize(bertInputShape[1]);
        ids[bertInputShape[1] - 1] = sepId;
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
    BLOG("计算文本相似度: '%s' vs '%s'", text1.c_str(), text2.c_str());
    
    // 如果两个文本都为空，认为它们相似
    if (text1.empty() && text2.empty()) {
        BLOG("两个文本都为空，相似度为1.0");
        return 1.0;
    }
    
    // 如果只有一个为空，认为它们不相似
    if (text1.empty() || text2.empty()) {
        BLOG("一个文本为空，另一个不为空，相似度为0.0");
        return 0.0;
    }
    
    // 调试：显示分词结果
    auto tokens1 = tokenize(text1);
    auto tokens2 = tokenize(text2);
    BLOG("分词结果1: [%s]", [&tokens1]() {
        std::string result;
        for (size_t i = 0; i < tokens1.size(); ++i) {
            if (i > 0) result += ", ";
            result += "'" + tokens1[i] + "'";
        }
        return result;
    }().c_str());
    BLOG("分词结果2: [%s]", [&tokens2]() {
        std::string result;
        for (size_t i = 0; i < tokens2.size(); ++i) {
            if (i > 0) result += ", ";
            result += "'" + tokens2[i] + "'";
        }
        return result;
    }().c_str());
    
    // 尝试使用BERT模型计算相似度
    try {
        BLOG("尝试使用BERT模型计算文本相似度");
        if (!bertSession) {
            BLOG("BERT模型未初始化，尝试初始化");
            try {
                initializeModels();
            } catch (const std::exception& e) {
                BLOGE("BERT模型初始化失败: %s", e.what());
                throw;
            }
        }
        
        if (!bertSession) {
            BLOGE("BERT模型仍然未初始化，使用备用方法");
            throw std::runtime_error("BERT模型未初始化");
        }
        
    auto embedding1 = getBertEmbedding(text1);
    auto embedding2 = getBertEmbedding(text2);
    
    if (embedding1.empty() || embedding2.empty()) {
            BLOGE("获取BERT嵌入向量失败，使用备用方法");
            throw std::runtime_error("获取BERT嵌入向量失败");
        }
        
        double similarity = cosine_similarity(embedding1, embedding2);
        BLOG("BERT模型计算文本相似度结果: %f", similarity);
        return similarity;
    } catch (const std::exception& e) {
        BLOGE("使用BERT模型计算文本相似度失败: %s，使用备用方法", e.what());
        
        // 使用备用方法：简单字符串比较
        if (text1 == text2) {
            BLOG("文本完全匹配，备用相似度为1.0");
            return 1.0;
        } else if (text1.find(text2) != std::string::npos || text2.find(text1) != std::string::npos) {
            BLOG("文本部分匹配（包含关系），备用相似度为0.8");
            return 0.8;
        } else {
            // 计算编辑距离相似度
            size_t maxLen = std::max(text1.length(), text2.length());
            if (maxLen == 0) return 1.0; // 两个都是空字符串
            
            // 简单的字符级别相似度：相同字符的比例
            size_t sameChars = 0;
            size_t minLen = std::min(text1.length(), text2.length());
            
            for (size_t i = 0; i < minLen; ++i) {
                if (text1[i] == text2[i]) {
                    sameChars++;
                }
            }
            
            double similarity = static_cast<double>(sameChars) / maxLen;
            BLOG("文本不匹配，使用字符级别相似度计算，备用相似度为%f", similarity);
            return similarity;
        }
    }
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

// double ActionSimilarity::calculateSimilarity(const ActivityNameActionPtr& action1, const ActivityNameActionPtr& action2) {
//     if (!action1 || !action2) {
//         return 0.0;
//     }
    
//     // 如果动作类型不同，直接返回0
//     if (action1->getActionType() != action2->getActionType()) {
//         return 0.0;
//     }
    
//     // 获取widget
//     const auto& widget1 = action1->getTarget();
//     const auto& widget2 = action2->getTarget();
    
//     // 如果一个有widget而另一个没有，直接返回0
//     if ((widget1 == nullptr && widget2 != nullptr) || (widget1 != nullptr && widget2 == nullptr)) {
//         return 0.0;
//     }
    
//     // 如果都没有widget（比如BACK操作），只比较activity
//     if (widget1 == nullptr && widget2 == nullptr) {
//         return calculateActivitySimilarity(*action1->getActivity(), *action2->getActivity());
//     }
    
//     // 计算各个属性的相似度
//     double textSim = calculateTextSimilarity(widget1->getText(), widget2->getText());
//     BLOG("text相似度: %f", textSim);
//     double resourceIdSim = calculateResourceIdSimilarity(widget1->getResourceID(), widget2->getResourceID());
//     BLOG("resourceId相似度: %f", resourceIdSim);
//     double activitySim = calculateActivitySimilarity(*action1->getActivity(), *action2->getActivity());
//     BLOG("activity相似度: %f", activitySim);
    
//     // 计算图标相似度
//     double iconSim = 0.0;
//     if (widget1->hasIcon() && widget2->hasIcon()) {
//         iconSim = calculateIconSimilarity(widget1->getIcon(), widget2->getIcon());//这里获取的是WidgetIconPtr类型，需要转换为cv::Mat类型
//         BLOG("icon相似度: %f", iconSim);
//     }
    
//     // 加权平均计算最终相似度
//     // 权重可以根据实际需求调整
//     double weightText = 0.35;
//     double weightResourceId = 0.2;
//     double weightActivity = 0.1;
//     double weightIcon = 0.35;
    
//     // 如果没有图标，调整其他权重
//     if (!widget1->hasIcon() || !widget2->hasIcon()) {
//         weightText = 0.4;
//         weightResourceId = 0.2;
//         weightActivity = 0.4;
//         weightIcon = 0.0;
//     }
    
//     double similarity = weightText * textSim + 
//                         weightResourceId * resourceIdSim + 
//                         weightActivity * activitySim + 
//                         weightIcon * iconSim;
//     BLOG("最终相似度: %f", similarity);
//     return similarity;
// }

// bool ActionSimilarity::isSimilar(const ActivityNameActionPtr& action1, const ActivityNameActionPtr& action2, double threshold) {
//     return calculateSimilarity(action1, action2) >= threshold;
// }

// 基于属性的相似度计算（支持序列化数据）
double ActionSimilarity::calculateSimilarity(
    const std::string& text1, const std::string& activityName1, const std::string& resourceId1, const std::string& iconBase64_1,
    const std::string& text2, const std::string& activityName2, const std::string& resourceId2, const std::string& iconBase64_2) {

    BLOG("开始基于属性计算相似度");
    
    try {
    // 计算各个属性的相似度
        double textSim = 0.0;
        try {
            textSim = calculateTextSimilarity(text1, text2);
            BLOG("text相似度: %f ('%s' vs '%s')", textSim, text1.c_str(), text2.c_str());
        } catch (const std::exception& e) {
            BLOGE("计算text相似度时发生错误: %s", e.what());
            // 如果BERT模型不可用，使用简单的字符串比较
            if (text1 == text2) {
                textSim = 1.0;
            } else if (text1.find(text2) != std::string::npos || text2.find(text1) != std::string::npos) {
                textSim = 0.8;
            } else {
                textSim = 0.0;
            }
            BLOG("使用备用方法计算text相似度: %f", textSim);
        }

        double resourceIdSim = 0.0;
        try {
            resourceIdSim = calculateResourceIdSimilarity(resourceId1, resourceId2);
            BLOG("resourceId相似度: %f ('%s' vs '%s')", resourceIdSim, resourceId1.c_str(), resourceId2.c_str());
        } catch (const std::exception& e) {
            BLOGE("计算resourceId相似度时发生错误: %s", e.what());
            // 如果BERT模型不可用，使用简单的字符串比较
            if (resourceId1 == resourceId2) {
                resourceIdSim = 1.0;
            } else if (resourceId1.find(resourceId2) != std::string::npos || resourceId2.find(resourceId1) != std::string::npos) {
                resourceIdSim = 0.8;
            } else {
                resourceIdSim = 0.0;
            }
            BLOG("使用备用方法计算resourceId相似度: %f", resourceIdSim);
        }

        double activitySim = 0.0;
        try {
            activitySim = calculateActivitySimilarity(activityName1, activityName2);
            BLOG("activity相似度: %f ('%s' vs '%s')", activitySim, activityName1.c_str(), activityName2.c_str());
        } catch (const std::exception& e) {
            BLOGE("计算activity相似度时发生错误: %s", e.what());
            // 如果BERT模型不可用，使用简单的字符串比较
            if (activityName1 == activityName2) {
                activitySim = 1.0;
            } else {
                activitySim = 0.0;
            }
            BLOG("使用备用方法计算activity相似度: %f", activitySim);
        }

    // 计算图标相似度（直接使用base64字符串，适用于外部模型匹配）
    double iconSim = 0.0;
    if (!iconBase64_1.empty() && !iconBase64_2.empty()) {
            try {
        iconSim = calculateIconSimilarity(iconBase64_1, iconBase64_2);
        BLOG("icon相似度: %f", iconSim);
            } catch (const std::exception& e) {
                BLOGE("计算图标相似度时发生错误: %s", e.what());
            }
        } else {
            BLOG("跳过图标相似度计算，至少一个图标数据为空");
    }

    // 加权平均计算最终相似度
    double weightText = 0.35;
    double weightResourceId = 0.2;
    double weightActivity = 0.1;
    double weightIcon = 0.35;

    // 如果没有图标，调整其他权重
    if (iconBase64_1.empty() || iconBase64_2.empty()) {
        weightText = 0.4;
        weightResourceId = 0.2;
        weightActivity = 0.4;
        weightIcon = 0.0;
            BLOG("无图标数据，调整权重: text=%.2f, resourceId=%.2f, activity=%.2f", 
                 weightText, weightResourceId, weightActivity);
        } else {
            BLOG("有图标数据，使用标准权重: text=%.2f, resourceId=%.2f, activity=%.2f, icon=%.2f", 
                 weightText, weightResourceId, weightActivity, weightIcon);
    }

    double similarity = weightText * textSim +
                        weightResourceId * resourceIdSim +
                        weightActivity * activitySim +
                        weightIcon * iconSim;

        BLOG("最终相似度: %f = %.2f*%.3f + %.2f*%.3f + %.2f*%.3f + %.2f*%.3f", 
             similarity, weightText, textSim, weightResourceId, resourceIdSim, 
             weightActivity, activitySim, weightIcon, iconSim);
    return similarity;
    } catch (const std::exception& e) {
        BLOGE("计算相似度过程中发生异常: %s", e.what());
        return 0.0;
    }
}

// 混合相似度计算：当前对象 vs 外部模型数据（用于外部模型匹配）
double ActionSimilarity::calculateSimilarity(const WidgetPtr& currentWidget, const std::string& currentActivityName,
                                             const std::string& externalText, const std::string& externalActivityName,
                                             const std::string& externalResourceId, const std::string& externalIconBase64) {
    if (!currentWidget) {
        return 0.0;
    }

    // 提取当前widget的属性
    std::string currentText = currentWidget->getText();
    std::string currentResourceId = currentWidget->getResourceID();
    std::string currentIconBase64;

    if (currentWidget->hasIcon()) {
        currentIconBase64 = currentWidget->getIconBase64();
    }

    // 调用基于属性的相似度计算
    return calculateSimilarity(currentText, currentActivityName, currentResourceId, currentIconBase64,
                              externalText, externalActivityName, externalResourceId, externalIconBase64);
}

// 混合相似度计算：当前action对象 vs 外部模型数据（用于外部模型匹配）
double ActionSimilarity::calculateSimilarity(const ActivityNameActionPtr& currentAction,
                                             const std::string& externalText, const std::string& externalActivityName,
                                             const std::string& externalResourceId, const std::string& externalIconBase64) {
    BLOG("开始计算相似度: currentAction vs 外部模型数据");
    
    if (!currentAction) {
        BLOGE("calculateSimilarity: currentAction为空");
        return 0.0;
    }

    auto targetWidget = currentAction->getTarget();
    if (!targetWidget) {
        BLOGE("calculateSimilarity: currentAction的targetWidget为空");
        return 0.0;
    }

    // 提取当前action的属性
    std::string currentText = targetWidget->getText();
    std::string currentActivityName = currentAction->getActivity() ? *currentAction->getActivity() : "";
    std::string currentResourceId = targetWidget->getResourceID();
    std::string currentIconBase64;

    BLOG("计算相似度 - 当前Action: text='%s', activity='%s', resourceId='%s'", 
         currentText.c_str(), currentActivityName.c_str(), currentResourceId.c_str());
    BLOG("计算相似度 - 外部数据: text='%s', activity='%s', resourceId='%s'", 
         externalText.c_str(), externalActivityName.c_str(), externalResourceId.c_str());

    if (targetWidget->hasIcon()) {
        currentIconBase64 = targetWidget->getIconBase64();
        BLOG("当前Action有图标数据，长度: %zu", currentIconBase64.length());
    } else {
        BLOG("当前Action没有图标数据");
    }

    if (!externalIconBase64.empty()) {
        BLOG("外部数据有图标数据，长度: %zu", externalIconBase64.length());
    } else {
        BLOG("外部数据没有图标数据");
    }

    try {
    // 调用基于属性的相似度计算
        double similarity = calculateSimilarity(currentText, currentActivityName, currentResourceId, currentIconBase64,
                              externalText, externalActivityName, externalResourceId, externalIconBase64);
        
        BLOG("计算相似度结果: %.3f", similarity);
        return similarity;
    } catch (const std::exception& e) {
        BLOGE("计算相似度过程中发生异常: %s，使用备用方法", e.what());
        
        // 使用备用方法：简单字符串比较
        double textSim = 0.0;
        if (currentText == externalText) {
            textSim = 1.0;
        } else if (!currentText.empty() && !externalText.empty() && 
                  (currentText.find(externalText) != std::string::npos || externalText.find(currentText) != std::string::npos)) {
            textSim = 0.8;
        }
        
        double resourceIdSim = 0.0;
        if (currentResourceId == externalResourceId) {
            resourceIdSim = 1.0;
        } else if (!currentResourceId.empty() && !externalResourceId.empty() &&
                  (currentResourceId.find(externalResourceId) != std::string::npos || externalResourceId.find(currentResourceId) != std::string::npos)) {
            resourceIdSim = 0.8;
        }
        
        double activitySim = 0.0;
        if (currentActivityName == externalActivityName) {
            activitySim = 1.0;
        }
        
        // 加权平均计算最终相似度
        double weightText = 0.4;
        double weightResourceId = 0.4;
        double weightActivity = 0.2;
        
        double similarity = weightText * textSim + 
                          weightResourceId * resourceIdSim + 
                          weightActivity * activitySim;
                          
        BLOG("备用相似度计算: textSim=%.3f, resourceIdSim=%.3f, activitySim=%.3f, 最终相似度=%.3f",
             textSim, resourceIdSim, activitySim, similarity);
             
        return similarity;
    }
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

// 基于base64字符串的图标相似度计算（用于外部模型匹配）
double ActionSimilarity::calculateIconSimilarity(const std::string& iconBase64_1, const std::string& iconBase64_2) {
    if (iconBase64_1.empty() || iconBase64_2.empty()) {
        BLOG("base64图标数据为空，无法计算相似度");
        return 0.0;
    }

    try {
        BLOG("开始计算base64图标相似度");

        // 创建WidgetIcon对象
        auto icon1 = std::make_shared<WidgetIcon>(iconBase64_1);
        auto icon2 = std::make_shared<WidgetIcon>(iconBase64_2);

        if (!icon1 || !icon2 || icon1->isEmpty() || icon2->isEmpty()) {
            BLOGE("无法从base64创建有效的WidgetIcon对象");
            return 0.0;
        }

        // 调用现有的WidgetIconPtr版本的方法
        return calculateIconSimilarity(icon1, icon2);

    } catch (const std::exception& e) {
        BLOGE("计算base64图标相似度时发生错误: %s", e.what());
        return 0.0;
    }
}

} // namespace fastbotx

#endif // ActionSimilarity_CPP_