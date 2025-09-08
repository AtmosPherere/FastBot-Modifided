/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */
/**
 * @authors Jianqiang Guo, Yuhui Su
 */
#include "fastbot_native.h"
#include <nlohmann/json.hpp> // JSON parsing library
#include "Model.h"
#include "ModelReusableAgent.h"
#include "WidgetReusableAgent.h"
#include "utils.hpp"

#ifdef __cplusplus
extern "C" {
#endif

static fastbotx::ModelPtr _fastbot_model = nullptr;

// 添加全局变量用于存储图标信息
std::unordered_map<std::string, std::map<std::string, std::string>> g_activityIconsMap;
std::mutex g_iconsMutex; // 用于线程安全访问

//getAction
jstring JNICALL Java_com_bytedance_fastbot_AiClient_b0bhkadf(JNIEnv *env, jobject, jstring activity,
                                                             jstring xmlDescOfGuiTree) {
    if (nullptr == _fastbot_model) {
        _fastbot_model = fastbotx::Model::create();
    }
    const char *xmlDescriptionCString = env->GetStringUTFChars(xmlDescOfGuiTree, nullptr);
    const char *activityCString = env->GetStringUTFChars(activity, nullptr);
    std::string xmlString = std::string(xmlDescriptionCString);
    std::string activityString = std::string(activityCString);
    std::string operationString = _fastbot_model->getOperate(xmlString, activityString);
    LOGD("do action opt is : %s", operationString.c_str());
    env->ReleaseStringUTFChars(xmlDescOfGuiTree, xmlDescriptionCString);
    env->ReleaseStringUTFChars(activity, activityCString);
    return env->NewStringUTF(operationString.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_bytedance_fastbot_AiClient_setWidgetIcons(JNIEnv *env, jclass clazz, jstring activity_name, jstring serialized_icons) {
    const char *activityNameChars = env->GetStringUTFChars(activity_name, nullptr);
    const char *serializedIconsChars = env->GetStringUTFChars(serialized_icons, nullptr);
    
    std::string activityName(activityNameChars);
    std::string serializedIcons(serializedIconsChars);
    
    // 解析JSON字符串
    std::map<std::string, std::string> iconMap;
    try {
        nlohmann::json jsonObject = nlohmann::json::parse(serializedIcons);
        
        // 遍历JSON对象中的所有键值对
        for (auto it = jsonObject.begin(); it != jsonObject.end(); ++it) {
            iconMap[it.key()] = it.value();
        }
        
        // 存储图标信息到全局变量
        {
            std::lock_guard<std::mutex> lock(g_iconsMutex);
            g_activityIconsMap[activityName] = iconMap;
        }
        
        LOGD("Stored %zu widget icons for activity: %s", iconMap.size(), activityName.c_str());
    } catch (const std::exception& e) {
        BLOGE("Failed to parse widget icons JSON: %s", e.what());
    }
    
    // 释放Java字符串
    env->ReleaseStringUTFChars(activity_name, activityNameChars);
    env->ReleaseStringUTFChars(serialized_icons, serializedIconsChars);
}

// for single device, just addAgent as empty device //InitAgent
void JNICALL Java_com_bytedance_fastbot_AiClient_fgdsaf5d(JNIEnv *env, jobject, jint agentType,
                                                          jstring packageName, jint deviceType) {
    if (nullptr == _fastbot_model) {
        _fastbot_model = fastbotx::Model::create();
    }
    auto algorithmType = (fastbotx::AlgorithmType) agentType;
    auto agentPointer = _fastbot_model->addAgent("", algorithmType,
                                                 (fastbotx::DeviceType) deviceType);
    const char *packageNameCString = "";
    if (env)
        packageNameCString = env->GetStringUTFChars(packageName, nullptr);
    _fastbot_model->setPackageName(std::string(packageNameCString));

    BLOG("init agent with type %d, %s,  %d", agentType, packageNameCString, deviceType);
    if (algorithmType == fastbotx::AlgorithmType::Reuse) {
        // 所有的agent都应该是WidgetReusableAgent
        auto widgetReuseAgentPtr = std::dynamic_pointer_cast<fastbotx::WidgetReusableAgent>(agentPointer);
        if (widgetReuseAgentPtr) {
            BLOG("Loading widget reuse model for WidgetReusableAgent");
            widgetReuseAgentPtr->loadReuseModel(std::string(packageNameCString));
        } else {
            BLOGE("Failed to cast agent to WidgetReusableAgent!");
        }

        if (env)
            env->ReleaseStringUTFChars(packageName, packageNameCString);
    }
}

// load ResMapping
void JNICALL
Java_com_bytedance_fastbot_AiClient_jdasdbil(JNIEnv *env, jobject, jstring resMappingFilepath) {
    if (nullptr == _fastbot_model) {
        _fastbot_model = fastbotx::Model::create();
    }
    const char *resourceMappingPath = env->GetStringUTFChars(resMappingFilepath, nullptr);
    auto preference = _fastbot_model->getPreference();
    if (preference) {
        preference->loadMixResMapping(std::string(resourceMappingPath));
    }
    env->ReleaseStringUTFChars(resMappingFilepath, resourceMappingPath);
}

// to check if a point is in black widget area
jboolean JNICALL
Java_com_bytedance_fastbot_AiClient_nkksdhdk(JNIEnv *env, jobject, jstring activity, jfloat pointX,
                                             jfloat pointY) {
    bool isShield = false;
    if (nullptr == _fastbot_model) {
        BLOGE("%s", "model null, check point failed!");
        return isShield;
    }
    const char *activityStr = env->GetStringUTFChars(activity, nullptr);
    auto preference = _fastbot_model->getPreference();
    if (preference) {
        isShield = preference->checkPointIsInBlackRects(std::string(activityStr),
                                                        static_cast<int>(pointX),
                                                        static_cast<int>(pointY));
    }
    env->ReleaseStringUTFChars(activity, activityStr);
    return isShield;
}

jstring JNICALL Java_com_bytedance_fastbot_AiClient_getNativeVersion(JNIEnv *env, jclass clazz) {
    return env->NewStringUTF(FASTBOT_VERSION);
}

// 添加清理方法，用于显式销毁模型和保存数据
void JNICALL Java_com_bytedance_fastbot_AiClient_cleanup(JNIEnv *env, jobject) {
    BLOG("Cleanup called - destroying fastbot model and saving data");

    if (_fastbot_model != nullptr) {
        // 在销毁模型之前，强制保存模型数据
        BLOG("Force saving model before destruction...");
        // 使用空字符串作为设备ID（与初始化时一致）
        auto agent = _fastbot_model->getAgent("");
        if (agent != nullptr) {
            // 尝试转换为 WidgetReusableAgent 并强制保存
            auto widgetAgent = std::dynamic_pointer_cast<fastbotx::WidgetReusableAgent>(agent);
            if (widgetAgent != nullptr) {
                BLOG("Calling forceSaveReuseModel...");
                widgetAgent->forceSaveReuseModel();
                BLOG("Force save completed");
            } else {
                BLOG("Agent is not WidgetReusableAgent, using regular save");
                auto reuseAgent = std::dynamic_pointer_cast<fastbotx::ModelReusableAgent>(agent);
                if (reuseAgent != nullptr) {
                    // 使用父类的保存方法
                    reuseAgent->saveReuseModel("");
                }
            }
        }

        // 模型销毁时会自动调用所有 agent 的析构函数，从而保存模型数据
        BLOG("Destroying fastbot model...");
        _fastbot_model.reset();
        _fastbot_model = nullptr;
        BLOG("Fastbot model destroyed");
    } else {
        BLOG("Fastbot model is already null");
    }

    // 清理全局图标数据
    {
        std::lock_guard<std::mutex> lock(g_iconsMutex);
        g_activityIconsMap.clear();
        BLOG("Cleared global activity icons map");
    }
}

#ifdef __cplusplus
}
#endif