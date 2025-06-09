/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */
/**
 * @authors 
 */
#ifndef ActionSimilarity_H_
#define ActionSimilarity_H_

#include "ActivityNameAction.h"
#include <memory>
#include <string>
#include <onnxruntime/onnxruntime_cxx_api.h>
#include <vector>
#include <unordered_map>

namespace fastbotx {

class ActionSimilarity {
public:
    ActionSimilarity();
    ~ActionSimilarity();

    // 计算两个action的相似度
    static double calculateSimilarity(const ActivityNameActionPtr& action1, const ActivityNameActionPtr& action2);
    
    // 判断两个action是否相似（相似度超过阈值）
    static bool isSimilar(const ActivityNameActionPtr& action1, const ActivityNameActionPtr& action2, double threshold = 0.8);

private:
    // 计算文本相似度
    static double calculateTextSimilarity(const std::string& text1, const std::string& text2);
    
    // 计算资源ID相似度
    static double calculateResourceIdSimilarity(const std::string& id1, const std::string& id2);
    
    // 计算活动名称相似度
    static double calculateActivitySimilarity(const std::string& activity1, const std::string& activity2);
    
    // 计算图标相似度（使用CLIP模型）
    static double calculateIconSimilarity(const WidgetIconPtr& icon1, const WidgetIconPtr& icon2);

    // BERT模型相关
    static Ort::Session* bertSession;
    static std::vector<const char*> bertInputNames;
    static std::vector<const char*> bertOutputNames;
    static std::vector<int64_t> bertInputShape;
    
    // CLIP模型相关
    static Ort::Session* clipSession;
    static std::vector<const char*> clipInputNames;
    static std::vector<const char*> clipOutputNames;
    static std::vector<int64_t> clipInputShape;
    
    // 初始化模型
    static void initializeModels();
    
    // 使用BERT模型获取文本的嵌入向量
    static std::vector<float> getBertEmbedding(const std::string& text);

    // 文本预处理相关
    static std::unordered_map<std::string, int64_t> vocabMap;  // 词汇表映射
    static const int64_t UNK_TOKEN_ID = 100;  // 未知token的ID
    static const int64_t CLS_TOKEN_ID = 101;  // 分类token的ID
    static const int64_t SEP_TOKEN_ID = 102;  // 分隔token的ID
    static const int64_t PAD_TOKEN_ID = 0;    // 填充token的ID

    // 初始化词汇表
    static void initializeVocab();
    
    // 对文本进行分词
    static std::vector<std::string> tokenize(const std::string& text);
    
    // 将token转换为ID
    static std::vector<int64_t> convertTokensToIds(const std::vector<std::string>& tokens);
    
    // 预处理文本（分词并转换为ID）
    static std::vector<int64_t> preprocessText(const std::string& text);
};

} // namespace fastbotx

#endif // ActionSimilarity_H_