#include "WidgetReusableAgent.h"
#include "../storage/WidgetReuseModel_generated.h"
#include "../desc/reuse/ActionSimilarity.h"
#include "Base.h"
#include "flatbuffers/flatbuffers.h"
#include "utils.hpp"
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <limits>
#include <cmath>
#include <thread>
#include <chrono>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <dirent.h>
#include <sys/stat.h>

namespace fastbotx {

// 在文件开头的命名空间内添加静态变量初始化
std::string WidgetReusableAgent::DefaultWidgetModelSavePath = "/sdcard/fastbot.widget.fbm";

WidgetReusableAgent::WidgetReusableAgent(const ModelPtr &model)
    : ModelReusableAgent(model) {
    // 初始化代码
    // 清空访问过的控件集合（新轮次开始）
    this->clearVisitedWidgets();

    // 自动检测并加载多平台模型
    BLOG("WidgetReusableAgent初始化，开始检测多平台模型...");
    try {
        autoLoadMultiPlatformModels("/sdcard", ""); // 构造函数时包名未知，使用空字符串
    } catch (const std::exception& e) {
        BLOGE("自动加载多平台模型失败: %s", e.what());
    }
}

WidgetReusableAgent::~WidgetReusableAgent() {
    BLOG("WidgetReusableAgent destructor called");
    BLOG("save widget reuse model in destruct");

    // 确保使用正确的保存路径
    std::string savePath;
    if (!this->_widgetModelSavePath.empty()) {
        savePath = this->_widgetModelSavePath;
    } else {
        savePath = DefaultWidgetModelSavePath;
    }

    BLOG("Saving widget reuse model to: %s", savePath.c_str());
    this->saveReuseModel(savePath);

    BLOG("Widget reuse model saved, clearing data");
    this->_widgetReuseModel.clear();
}

void WidgetReusableAgent::updateReuseModel() {
    if (this->_previousActions.empty())
        return;
    ActionPtr lastAction = this->_previousActions.back();
    ActivityNameActionPtr modelAction = std::dynamic_pointer_cast<ActivityNameAction>(lastAction);
    if (nullptr == modelAction || nullptr == this->_newState)
        return;
    auto hash = (uint64_t)modelAction->hash();

    {
        std::lock_guard<std::mutex> reuseGuard(this->_widgetReuseModelLock);
        // 直接获取/创建 action_hash 对应的 widget map
        auto &widgetMap = this->_widgetReuseModel[hash];

        // 保存action的属性
        ActionAttributes actionAttrs;
        actionAttrs.actionType = static_cast<int>(modelAction->getActionType());
        actionAttrs.activityName = modelAction->getActivity() ? *modelAction->getActivity() : "";
        
        auto targetWidget = modelAction->getTarget();
        if (targetWidget) {
            actionAttrs.targetWidgetText = targetWidget->getText();
            actionAttrs.targetWidgetResourceId = targetWidget->getResourceID();
            if (targetWidget->hasIcon()) {
                actionAttrs.targetWidgetIconBase64 = targetWidget->getIconBase64();
            }
        }
        this->_actionAttributes[hash] = actionAttrs;

        // 为当前状态的每个widget更新计数和属性
        for (const auto &widget : this->_newState->getWidgets()) {
            auto widgetHash = widget->hash();
            auto &widgetCountWithAttrs = widgetMap[widgetHash];
            
            int oldCount = widgetCountWithAttrs.count;
            int newCount = oldCount + 1;
            widgetCountWithAttrs.count = newCount;
            
            // 更新widget属性
            widgetCountWithAttrs.text = widget->getText();
            widgetCountWithAttrs.activityName = actionAttrs.activityName;
            widgetCountWithAttrs.resourceId = widget->getResourceID();
            if (widget->hasIcon()) {
                widgetCountWithAttrs.iconBase64 = widget->getIconBase64();
            }

            BDLOG("update reuse model: action_hash=%llu, widget_hash=%llu, old_count=%d, new_count=%d",
                  hash, widgetHash, oldCount, newCount);
        }
    }
}

void WidgetReusableAgent::updateStrategy() {
    // 首先调用父类的updateStrategy方法
    ModelReusableAgent::updateStrategy();

    // 然后更新当前轮次访问过的控件集合
    if (this->_newState) {
        this->updateVisitedWidgets(this->_newState);
    }
}

// action_hash_1: {
//         activity_1: {
//              widget_hash_1: count_1,
//              widget_hash_2: count_1,
//              widget_hash_3: count_1,
//              widget_hash_4: count_1,
//         },
//         activity_2: {
//              widget_hash_5: count_2,
//              widget_hash_6: count_2,
//              widget_hash_7: count_2,
//              widget_hash_8: count_2,
//         },
//     }
    // ...
// action_hash_1: {
//              widget_hash_1: count_1,
//              widget_hash_2: count_2,
//              widget_hash_3: count_3,
//              widget_hash_4: count_4,
//         },
// action_hash_2: {
//              widget_hash_5: count_5,
//              widget_hash_6: count_6,
//              widget_hash_7: count_7,
//              widget_hash_8: count_8,
//     }
// action_hash_1: {
//              widget_hash_1: count_1,
//              widget_hash_2: count_2,
//              widget_hash_3: count_3,
//              widget_hash_4: count_4,
//         },
// action_hash_2: {
//              widget_hash_5: count_5,
//              widget_hash_6: count_6,
//              widget_hash_7: count_7,
//              widget_hash_8: count_8,
//     }
// ActionInfo{
//         hash: 54321, 
//         targetText: "注册",
//         targetResourceId: "com.app:id/register_btn",
//         targetActivityName: "android.widget.Button",
//         targetIcon: "base64_icon_data"
//     } -> {
//         WidgetInfo{
//             hash: 33333,
//             text: "手机号",
//             resourceId: "com.app:id/phone",
//             ActivityName: "...",
//             Icon: "base64_icon_data"
//         }: 3,
        
//         WidgetInfo{
//             hash: 44444,
//             text: "验证码",
//             resourceId: "com.app:id/code",
//             ActivityName: "...", 
//             Icon: "base64_icon_data"
//         }: 3
//     }
double WidgetReusableAgent::probabilityOfVisitingNewWidgets(const ActivityStateActionPtr &action,
                                                          const stringPtrSet &visitedActivities) const {
    // 计算执行该动作后可能到达新控件的概率
    // 基于重用模型中记录的控件和当前轮次已访问的控件进行对比
    double value = 0.0;
    int total = 0;      // 该action在模型中记录的总执行次数
    int unvisited = 0;  // 执行该action后到达本轮未访问控件的次数

    uintptr_t actionHash = action->hash();
    BLOG("Computing widget probability for action hash=%llu", actionHash);

    // 查找action在widget重用模型中的记录
    auto actionMapIterator = this->_widgetReuseModel.find(actionHash);
    if (actionMapIterator != this->_widgetReuseModel.end()) {
        const auto &widgetMap = actionMapIterator->second;

        BLOG("Action %llu found in widget reuse model with %zu target widgets",
             actionHash, widgetMap.size());

        // 遍历该action能到达的所有widget
        for (const auto &widgetCountPair : widgetMap) {
            total += widgetCountPair.second.count;  // 累加总执行次数
            uint64_t widgetHash = widgetCountPair.first;

            // 检查该widget是否在当前轮次中已被访问过
            // 使用我们自己维护的 _visitedWidgets 集合
            bool isVisited = this->_visitedWidgets.find(widgetHash) != this->_visitedWidgets.end();

            BLOG("  Widget hash: %llu, count: %d, visited in current round: %s",
                 widgetHash, widgetCountPair.second.count, isVisited ? "yes" : "no");

            if (!isVisited) {
                unvisited += widgetCountPair.second.count;  // 累加未访问次数
            }
        }

        BLOG("Action %llu: total=%d, unvisited=%d", actionHash, total, unvisited);

        if (total > 0 && unvisited > 0) {
            value = static_cast<double>(unvisited) / total;
        }

        BLOG("Final widget probability for action %llu: %f", actionHash, value);
    } else {
        BLOG("Action %llu NOT found in widget reuse model", actionHash);
        // 如果action不在模型中，给一个很高的探索概率
        value = 1.0;
    }

    return value;
}

ActionPtr WidgetReusableAgent::selectUnperformedActionInReuseModel() const {
    float maxValue = -MAXFLOAT;
    ActionPtr nextAction = nullptr;

    BLOG("WidgetReusableAgent: Searching for unperformed actions in widget reuse model...");

    // 首先检查widget重用模型是否为空
    BLOG("Widget reuse model size: %zu", this->_widgetReuseModel.size());
    if (this->_widgetReuseModel.empty()) {
        BLOG("Widget reuse model is empty, cannot select action from reuse model");
        return nullptr;
    }

    // 检查targetActions()返回的列表
    auto targetActions = this->_newState->targetActions();
    BLOG("Target actions count: %zu", targetActions.size());

    if (targetActions.empty()) {
        BLOG("No target actions available");
        return nullptr;
    }

    // 检查有多少操作在widget重用模型中
    // int actionsInModel = 0;
    // for (const auto &action: targetActions) {
    //     if (this->_widgetReuseModel.find(action->hash()) != this->_widgetReuseModel.end()) {
    //         actionsInModel++;
    //         BLOG("Action hash=%llu found in widget reuse model", action->hash());
    //     } else {
    //         BLOG("Action hash=%llu NOT found in widget reuse model", action->hash());
    //     }
    // }
    // BLOG("Actions in widget reuse model: %d out of %zu target actions", actionsInModel, targetActions.size());

    for (const auto &action: this->_newState->targetActions()) {
        uintptr_t actionHash = action->hash();
        BLOG("Processing action hash=%llu, type=%s, visitedCount=%d",
             actionHash, actName[action->getActionType()].c_str(), action->getVisitedCount());

        if (action->getVisitedCount() > 0) {
            BLOG("Action %llu has been visited %d times, skipping", actionHash, action->getVisitedCount());
            continue;
        }

        // 检查本地模型
        bool inLocalModel = (this->_widgetReuseModel.find(actionHash) != this->_widgetReuseModel.end());
        ExternalActionMatch externalMatch;

        // 检查外部模型（先查询缓存）
        auto activityNameAction = std::dynamic_pointer_cast<ActivityNameAction>(action);
        if (activityNameAction) {
            BLOG("尝试在外部模型中查找相似action: hash=%llu, type=%s", 
                 actionHash, actName[action->getActionType()].c_str());
            // 先命中缓存
            bool cacheHit = false;
            {
                std::lock_guard<std::mutex> cacheLock(_externalActionMatchCacheLock);
                auto it = _externalActionMatchCache.find(actionHash);
                if (it != _externalActionMatchCache.end() && it->second.found && it->second.similarity >= 0.5) {
                    externalMatch = it->second;
                    cacheHit = true;
                    BLOG("外部action匹配在selectUnperformedActionInReuseModel命中缓存: platform=%s, similarity=%.3f",
                         externalMatch.platformId.c_str(), externalMatch.similarity);
                }
            }
            if (!cacheHit) {
                externalMatch = findSimilarActionInExternalModels(activityNameAction, 0.5);
            }
            
            if (externalMatch.found) {
                BLOG("成功在外部模型中找到相似action: platform=%s, similarity=%.3f",
                     externalMatch.platformId.c_str(), externalMatch.similarity);
            } else {
                BLOG("在外部模型中未找到相似action");
            }
        } else {
            BLOG("无法将action转换为ActivityNameAction类型，跳过外部模型检查");
        }

        if (inLocalModel || externalMatch.found) {
            auto modelPointer = this->_model.lock();
            if (modelPointer) {
                const GraphPtr &graphRef = modelPointer->getGraph();
                auto visitedActivities = graphRef->getVisitedActivities();

                double qualityValue = 0.0;

                if (inLocalModel) {
                    BLOG("Found action %llu in local widget reuse model", actionHash);
                    qualityValue = this->probabilityOfVisitingNewActivities(action, visitedActivities);
                } else if (externalMatch.found) {
                    BLOG("Found similar action in external model: platform=%s, similarity=%.3f",
                         externalMatch.platformId.c_str(), externalMatch.similarity);

                    // 使用外部模型数据计算概率
                    qualityValue = this->probabilityOfVisitingNewWidgetsFromExternalModel(
                        action, externalMatch);


                }

                BLOG("Calculated probability for action hash=%llu: %f", actionHash, qualityValue);

                if (qualityValue > 1e-4) {
                    // 添加随机因子
                    auto adjustedQualityValue = static_cast<float>(10.0 * qualityValue);
                    auto uniform = static_cast<float>(static_cast<float>(randomInt(0, 10)) / 10.0f);
                    if (uniform < std::numeric_limits<float>::min())
                        uniform = std::numeric_limits<float>::min();
                    adjustedQualityValue -= log(-log(uniform));

                    BLOG("After random factor, quality value for action hash=%llu: %f", actionHash, adjustedQualityValue);

                    if (adjustedQualityValue > maxValue) {
                        maxValue = adjustedQualityValue;
                        nextAction = action;
                        BLOG("New best action hash=%llu with quality value %f", actionHash, adjustedQualityValue);
                    }
                } else {
                    BLOG("Action %llu quality too low: %f (threshold: 1e-4)", actionHash, qualityValue);
                }
            } else {
                BLOG("Model pointer is null for action %llu", actionHash);
            }
        } else {
            BLOG("Action %llu NOT found in any model (local or external)", actionHash);
        }
    }

    if (nextAction != nullptr) {
        BLOG("WidgetReusableAgent: Selected action hash=%llu with max quality value %f", nextAction->hash(), maxValue);
    } else {
        BLOG("WidgetReusableAgent: No action selected from widget reuse model (maxValue=%f)", maxValue);
    }
    return nextAction;
}

ActionPtr WidgetReusableAgent::selectUnperformedActionNotInReuseModel() const {
    ActionPtr retAct = nullptr;
    std::vector<ActionPtr> actionsNotInModel;

    BLOG("WidgetReusableAgent: Searching for actions not in any model (local + external)...");

    for (const auto &action: this->_newState->getActions()) {
        if (!action->isModelAct() || action->getVisitedCount() > 0) {
            continue; // 跳过非模型action或已访问的action
        }

        // 检查是否在本地模型中
        bool inLocalModel = (this->_widgetReuseModel.find(action->hash()) != this->_widgetReuseModel.end());

        // 检查是否在外部模型中（使用相似度匹配）
        bool inExternalModel = false;
        auto activityNameAction = std::dynamic_pointer_cast<ActivityNameAction>(action);
        if (activityNameAction) {
            inExternalModel = isActionInAnyModel(activityNameAction, 0.5);
        }

        // 只有既不在本地模型也不在外部模型中的action才加入候选集
        if (!inLocalModel && !inExternalModel) {
            actionsNotInModel.emplace_back(action);
            BLOG("Action hash=%llu (type=%s) not in any model, adding to candidates",
                 action->hash(), actName[action->getActionType()].c_str());
        } else if (inExternalModel && !inLocalModel) {
            BLOG("Action hash=%llu found in external model, skipping", action->hash());
        }
    }

    BLOG("Found %zu actions not in widget reuse model", actionsNotInModel.size());

    // random by priority
    int totalWeight = 0;
    for (const auto &action: actionsNotInModel) {
        totalWeight += action->getPriority();
    }
    if (totalWeight <= 0) {
        BLOG("Total weights is 0 for actions not in widget reuse model");
        return nullptr;
    }

    int randI = randomInt(0, totalWeight);
    for (auto action: actionsNotInModel) {
        if (randI < action->getPriority()) {
            BLOG("WidgetReusableAgent: Selected action hash=%llu not in widget reuse model", action->hash());
            return action;
        }
        randI -= action->getPriority();
    }

    BLOG("WidgetReusableAgent: Failed to select action not in widget reuse model");
    return nullptr;
}

#define SarsaNStep 5
    double WidgetReusableAgent::computeRewardOfLatestAction() {//计算即时奖励
        double rewardValue = 0.0;
        if (nullptr != this->_newState) {
            this->computeAlphaValue();
            const GraphPtr &graphRef = this->_model.lock()->getGraph();
            auto visitedActivities = graphRef->getVisitedActivities(); // get the set of visited activities
            // get the last, or previous, action in the vector containing previous actions.
            ActivityStateActionPtr lastSelectedAction = std::dynamic_pointer_cast<ActivityStateAction>(
                    this->_previousActions.back());
            if (nullptr != lastSelectedAction) {
                // 首先检查本机模型
                uint64_t actionHash = lastSelectedAction->hash();
                bool foundInLocalModel = (this->_widgetReuseModel.find(actionHash) != this->_widgetReuseModel.end());

                if (foundInLocalModel) {
                    // 在本机模型中找到，使用原有逻辑
                    rewardValue = this->probabilityOfVisitingNewActivities(lastSelectedAction, visitedActivities);
                    BLOG("Action在本机模型中找到，奖励值=%.3f", rewardValue);
                } else {
                    // 本机模型中没找到，检查外部模型（先查缓存）
                    auto activityNameAction = std::dynamic_pointer_cast<ActivityNameAction>(lastSelectedAction);
                    if (activityNameAction) {
                        WidgetReusableAgent::ExternalActionMatch externalMatch;
                        bool cacheHit = false;
                        {
                            std::lock_guard<std::mutex> cacheLock(_externalActionMatchCacheLock);
                            auto it = _externalActionMatchCache.find(activityNameAction->hash());
                            if (it != _externalActionMatchCache.end() && it->second.found && it->second.similarity >= 0.5) {
                                externalMatch = it->second;
                                cacheHit = true;
                                BLOG("computeReward: 外部action匹配命中缓存: platform=%s, similarity=%.3f",
                                     externalMatch.platformId.c_str(), externalMatch.similarity);
                            }
                        }
                        if (!cacheHit) {
                            externalMatch = findSimilarActionInExternalModels(activityNameAction, 0.5);
                        }
                        if (externalMatch.found) {
                            // 在外部模型中找到相似action
                            rewardValue = this->probabilityOfVisitingNewWidgetsFromExternalModel(
                                activityNameAction, externalMatch);
                            
                            BLOG("Action在外部模型中找到相似匹配，平台=%s，相似度=%.3f",
                                 externalMatch.platformId.c_str(), externalMatch.similarity);
                        } else {
                            // 完全新的action，给予最高奖励
                            rewardValue = 1.0;
                            BLOG("Action是完全新的，给予最高奖励1.0");
                        }
                    } else {
                        // 无法转换为ActivityNameAction，按新action处理
                        rewardValue = 1.0;
                        BLOG("Action无法转换为ActivityNameAction，按新action处理，给予奖励1.0");
                    }
                }

                // If this is an action not in any model, this action is new and should definitely be used
                if (std::abs(rewardValue - 0.0) < 0.0001)
                    rewardValue = 1.0; // Set the expectation of this action to 1
                rewardValue = (rewardValue / sqrt(lastSelectedAction->getVisitedCount() + 1.0));
            }
            // 控件级期望值累加
            double widgetSum = 0.0;
            double denom = sqrt(this->_newState->getVisitedCount() + 1.0);
            for (const auto &widget : this->_newState->getWidgets()) {
                widgetSum += getStateActionExpectationValue(widget, visitedActivities) / denom;
            }
            rewardValue += widgetSum;
            BLOG("total visited " ACTIVITY_VC_STR " count is %zu", visitedActivities.size());
        }
        BDLOG("reuse-cov-opti action reward=%f", rewardValue);
        this->_rewardCache.emplace_back(rewardValue);
        // Make sure the length of reward cache is not over SarsaNStep
        if (this->_rewardCache.size() > SarsaNStep) {
            this->_rewardCache.erase(this->_rewardCache.begin());
        }
        return rewardValue;// + this->_newState->getTheta();
    }

    //遍历 state->getActions()，筛选 action->getTarget() == widget 的 action，再进行后续处理。
    double WidgetReusableAgent::getStateActionExpectationValue(const WidgetPtr &widget, const stringPtrSet &visitedActivities) const {
        double value = 0.0;
        if (!this->_newState) return value;
        const auto& actions = this->_newState->getActions();
        for (const auto& action : actions) {
            if (action->getTarget() == widget) {
                uintptr_t actionHash = action->hash();

                // 首先检查本机模型
                bool foundInLocalModel = (this->_widgetReuseModel.find(actionHash) != this->_widgetReuseModel.end());

                if (foundInLocalModel) {
                    // 在本机模型中找到，根据访问次数给予奖励
                    if (action->getVisitedCount() >= 1) {
                        value += 0.5;
                    }
                } else {
                    // 本机模型中没找到，检查外部模型（先查缓存）
                    auto activityNameAction = std::dynamic_pointer_cast<ActivityNameAction>(action);
                    if (activityNameAction) {
                        WidgetReusableAgent::ExternalActionMatch externalMatch;
                        bool cacheHit = false;
                        {
                            std::lock_guard<std::mutex> cacheLock(_externalActionMatchCacheLock);
                            auto it = _externalActionMatchCache.find(activityNameAction->hash());
                            if (it != _externalActionMatchCache.end() && it->second.found && it->second.similarity >= 0.5) {
                                externalMatch = it->second;
                                cacheHit = true;
                                BLOG("getStateExpectation: 外部action匹配命中缓存: platform=%s, similarity=%.3f",
                                     externalMatch.platformId.c_str(), externalMatch.similarity);
                            }
                        }
                        if (!cacheHit) {
                            externalMatch = findSimilarActionInExternalModels(activityNameAction, 0.5);
                        }
                        if (externalMatch.found) {
                            // 在外部模型中找到相似action，给予中等奖励
                            value += 0.7;
                            BLOG("Action在外部模型中找到相似匹配，相似度=%.3f，给予奖励0.7", externalMatch.similarity);
                        } else {
                            // 完全新的action，给予最高奖励
                            value += 1.0;
                            BLOG("Action是完全新的，给予最高奖励1.0");
                        }
                    } else {
                        // 无法转换为ActivityNameAction，按新action处理
                        value += 1.0;
                    }
                }
                // 期望到达未访问activity的概率
                value += probabilityOfVisitingNewActivities(action, visitedActivities);
            }
        }
        return value;
    }
#ifdef __ANDROID__
#define STORAGE_PREFIX "/sdcard/fastbot_"
#else
#define STORAGE_PREFIX ""
#endif

    /// According to the given package name, deserialize
    /// the serialized model file with the ReuseModel.fbs
    /// by FlatBuffers
    /// \param packageName The package name of the tested application
    void WidgetReusableAgent::saveReuseModel(const std::string &modelFilepath) {
        flatbuffers::FlatBufferBuilder builder;
        std::vector<flatbuffers::Offset<fastbotx::ReuseEntry>> reuseEntryVector;

        {
            std::lock_guard<std::mutex> reuseGuard(this->_widgetReuseModelLock);

            // 检查模型是否为空
            if (this->_widgetReuseModel.empty()) {
                BLOG("Widget reuse model is empty, skipping save");
                return;
            }

            BLOG("Saving widget reuse model with %zu actions (with similarity attributes)", this->_widgetReuseModel.size());
            int actionsWithAttrs = 0;

            for (const auto &actionIterator : this->_widgetReuseModel) {
                uint64_t actionHash = actionIterator.first;
                const WidgetCountMapWithAttrs &widgetMap = actionIterator.second;

                // 获取action的属性
                const ActionAttributes* actionAttrs = nullptr;
                auto actionAttrsIt = this->_actionAttributes.find(actionHash);
                if (actionAttrsIt != this->_actionAttributes.end()) {
                    actionAttrs = &actionAttrsIt->second;
                }

                // 创建action的相似度属性
                flatbuffers::Offset<fastbotx::ActionSimilarityAttributes> actionAttrsOffset;
                bool hasActionAttrs = false;
                
                if (actionAttrs) {
                        // 创建target widget的相似度属性
                        auto targetWidgetAttrs = fastbotx::CreateWidgetSimilarityAttributes(builder,
                        builder.CreateString(actionAttrs->targetWidgetText),
                        builder.CreateString(actionAttrs->activityName),
                        builder.CreateString(actionAttrs->targetWidgetResourceId),
                        builder.CreateString(actionAttrs->targetWidgetIconBase64));

                        // 创建action的相似度属性
                    actionAttrsOffset = fastbotx::CreateActionSimilarityAttributes(builder,
                        actionAttrs->actionType,
                        builder.CreateString(actionAttrs->activityName),
                            targetWidgetAttrs);
                    
                    hasActionAttrs = true;
                    actionsWithAttrs++;
                    
                    BLOG("保存action属性: hash=%llu, type=%d, text='%s', resourceId='%s'",
                         actionHash, actionAttrs->actionType, 
                         actionAttrs->targetWidgetText.c_str(), actionAttrs->targetWidgetResourceId.c_str());
                }

                // 创建widget count向量，包含相似度属性
                std::vector<flatbuffers::Offset<fastbotx::WidgetCount>> widgetVector;
                for (const auto &widgetIterator : widgetMap) {
                    uint64_t widgetHash = widgetIterator.first;
                    const WidgetCountWithAttributes &widgetCountWithAttrs = widgetIterator.second;

                    // 直接使用内存中的widget属性数据
                    auto widgetAttrs = fastbotx::CreateWidgetSimilarityAttributes(builder,
                        builder.CreateString(widgetCountWithAttrs.text),
                        builder.CreateString(widgetCountWithAttrs.activityName),
                        builder.CreateString(widgetCountWithAttrs.resourceId),
                        builder.CreateString(widgetCountWithAttrs.iconBase64));

                    auto widgetCount = fastbotx::CreateWidgetCount(builder, widgetHash, widgetCountWithAttrs.count, widgetAttrs);
                    widgetVector.push_back(widgetCount);
                }

                // 创建ActivityWidgetMap
                auto activityWidgetMap = CreateActivityWidgetMap(
                    builder,
                    builder.CreateString(""), // 空的activity名称
                    builder.CreateVector(widgetVector)
                );

                std::vector<flatbuffers::Offset<fastbotx::ActivityWidgetMap>> activityVector;
                activityVector.push_back(activityWidgetMap);

                // 创建ReuseEntry，包含相似度属性
                auto reuseEntry = CreateReuseEntry(
                    builder,
                    actionHash,
                    builder.CreateVector(activityVector),
                    actionAttrsOffset
                );
                reuseEntryVector.push_back(reuseEntry);
            }
            
            BLOG("保存模型: 总共 %zu 个actions, 其中 %d 个包含属性", this->_widgetReuseModel.size(), actionsWithAttrs);
        }
        
        // 创建包含相似度属性的模型
        auto widgetReuseModel = fastbotx::CreateWidgetReuseModel(
            builder,
            builder.CreateVector(reuseEntryVector),
            builder.CreateString("current_platform"), // 平台信息
            true // 保存了相似度属性
        );
        builder.Finish(widgetReuseModel);
        
        std::string outputFilePath = modelFilepath;
        if (outputFilePath.empty()) {
            outputFilePath = this->_widgetDefaultModelSavePath;
        }
        BLOG("save widget reuse model to path: %s", outputFilePath.c_str());
        std::ofstream outputFile(outputFilePath, std::ios::binary);
        outputFile.write((char *)builder.GetBufferPointer(), static_cast<int>(builder.GetSize()));
        outputFile.close();
    }

    void WidgetReusableAgent::forceSaveReuseModel() {
        BLOG("Force saving widget reuse model...");
        std::string savePath;
        if (!this->_widgetModelSavePath.empty()) {
            savePath = this->_widgetModelSavePath;
        } else {
            savePath = this->_widgetDefaultModelSavePath;
        }

        // 直接调用saveReuseModel方法，因为它已经使用统一的数据结构
        saveReuseModel(savePath);
        BLOG("Force save completed");
    }

    void WidgetReusableAgent::loadReuseModel(const std::string &packageName) {
        std::string modelFilePath = std::string(STORAGE_PREFIX) + packageName + ".fbm"; // widget binary model

        this->_widgetModelSavePath = modelFilePath;

        // 关键修复：同时设置父类的 _modelSavePath，确保后台保存线程能正常工作
        this->_modelSavePath = modelFilePath;

        if (!this->_widgetModelSavePath.empty()) {
            // 使用相同的后缀，避免保存和加载路径不一致
            this->_widgetDefaultModelSavePath = std::string(STORAGE_PREFIX) + packageName + ".fbm";
            this->_defaultModelSavePath = std::string(STORAGE_PREFIX) + packageName + ".fbm";  // 同时设置父类的默认路径
        } else {
            // 如果包名为空，使用默认路径
            this->_widgetModelSavePath = DefaultWidgetModelSavePath;
            this->_widgetDefaultModelSavePath = DefaultWidgetModelSavePath;
            this->_modelSavePath = DefaultWidgetModelSavePath;  // 同时设置父类路径
            this->_defaultModelSavePath = DefaultWidgetModelSavePath;
        }

        BLOG("begin load widget reuse model: %s", this->_widgetModelSavePath.c_str());
        BLOG("parent class _modelSavePath set to: %s", this->_modelSavePath.c_str());
        
        std::ifstream modelFile(modelFilePath, std::ios::binary | std::ios::in);
        if (modelFile.fail()) {
            BLOG("read widget reuse model file %s failed, check if file exists!", modelFilePath.c_str());

            // 即使本机模型加载失败，也要尝试加载外部模型
            BLOG("本机模型不存在，但仍然检测多平台模型...");
            try {
                autoLoadMultiPlatformModels("/sdcard", packageName);
            } catch (const std::exception& e) {
                BLOGE("自动加载多平台模型失败: %s", e.what());
            }
            return;
        }
        
        std::filebuf *fileBuffer = modelFile.rdbuf();
        std::size_t filesize = fileBuffer->pubseekoff(0, modelFile.end, modelFile.in);
        fileBuffer->pubseekpos(0, modelFile.in);
        char *modelFileData = new char[filesize];
        fileBuffer->sgetn(modelFileData, static_cast<int>(filesize));
        
        auto widgetReuseFBModel = GetWidgetReuseModel(modelFileData);
        
        {
            std::lock_guard<std::mutex> reuseGuard(this->_widgetReuseModelLock);
            this->_widgetReuseModel.clear();
            this->_widgetReuseQValue.clear();
        }
        
        auto modelDataPtr = widgetReuseFBModel->model();
        if (!modelDataPtr) {
            BLOG("%s", "widget reuse model data is null");
            return;
        }
        
        for (int entryIndex = 0; entryIndex < modelDataPtr->size(); entryIndex++) {
            auto reuseEntry = modelDataPtr->Get(entryIndex);
            uint64_t actionHash = reuseEntry->action();
            const auto* activities = reuseEntry->activities();

            // 在新的数据结构中，我们直接创建widget map，忽略activity层级
            WidgetCountMap widgetMap;

            // 遍历所有activity（通常只有一个空的activity）
            for (int activityIndex = 0; activityIndex < activities->size(); activityIndex++) {
                const ::fastbotx::ActivityWidgetMap* activityEntry = activities->Get(activityIndex);
                const flatbuffers::Vector<flatbuffers::Offset<::fastbotx::WidgetCount>>* widgets = activityEntry->widgets();

                // 将所有widget合并到同一个map中
                for (int widgetIndex = 0; widgetIndex < widgets->size(); widgetIndex++) {
                    const ::fastbotx::WidgetCount* widgetEntry = widgets->Get(widgetIndex);
                    uint64_t widgetHash = widgetEntry->widget_hash();
                    int count = widgetEntry->count();

                    // 如果widget已存在，取较大的count值（合并不同activity的数据）
                    if (widgetMap.find(widgetHash) != widgetMap.end()) {
                        widgetMap[widgetHash] = std::max(widgetMap[widgetHash], count);
                    } else {
                        widgetMap[widgetHash] = count;
                    }
                }
            }

            if (!widgetMap.empty()) {
                std::lock_guard<std::mutex> reuseGuard(this->_widgetReuseModelLock);
                // 将WidgetCountMap转换为WidgetCountMapWithAttrs
                WidgetCountMapWithAttrs widgetMapWithAttrs;
                for (const auto& widgetPair : widgetMap) {
                    WidgetCountWithAttributes widgetCountWithAttrs;
                    widgetCountWithAttrs.count = widgetPair.second;
                    // 其他属性保持默认值，因为旧模型中没有这些信息
                    widgetMapWithAttrs[widgetPair.first] = widgetCountWithAttrs;
                }
                this->_widgetReuseModel[actionHash] = widgetMapWithAttrs;
            }
        }
        
        BLOG("loaded widget reuse model contains actions: %zu", this->_widgetReuseModel.size());
        delete[] modelFileData;

        // 加载本地模型后，自动检测并加载多平台模型
        BLOG("本地模型加载完成，开始检测多平台模型...");
        // 清理旧的外部匹配缓存，避免使用过期索引
        {
            std::lock_guard<std::mutex> cacheLock(_externalActionMatchCacheLock);
            _externalActionMatchCache.clear();
        }
        {
            std::lock_guard<std::mutex> idxLock(_externalWidgetIndexLock);
            _externalWidgetVisitedIndex.clear();
        }
        try {
            autoLoadMultiPlatformModels("/sdcard", packageName);
        } catch (const std::exception& e) {
            BLOGE("自动加载多平台模型失败: %s", e.what());
        }
    }

    // 为了保持与基类接口的兼容性，保留原方法名但调用新的实现
    double WidgetReusableAgent::probabilityOfVisitingNewActivities(const ActivityStateActionPtr &action,
                                                                  const stringPtrSet &visitedActivities) const {
        // 在细粒度模型中，我们实际计算的是访问新控件的概率
        // 这里调用新的方法来保持语义一致性
        return this->probabilityOfVisitingNewWidgets(action, visitedActivities);
    }

    // 更新当前轮次访问过的控件集合
    void WidgetReusableAgent::updateVisitedWidgets(const StatePtr& state) {
        if (!state) return;

        const auto& widgets = state->getWidgets();
        for (const auto& widget : widgets) {
            if (widget) {
                uint64_t widgetHash = widget->hash();
                this->_visitedWidgets.insert(widgetHash);
                BLOG("Added widget hash %llu to visited widgets set", widgetHash);
            }
        }
        BLOG("Total visited widgets in current round: %zu", this->_visitedWidgets.size());
    }

    // 清空当前轮次访问过的控件集合（新轮次开始时调用）
    void WidgetReusableAgent::clearVisitedWidgets() {
        BLOG("Clearing visited widgets set (had %zu widgets)", this->_visitedWidgets.size());
        this->_visitedWidgets.clear();
    }

    // ========== 多平台复用功能实现 ==========

    void WidgetReusableAgent::autoLoadMultiPlatformModels(const std::string& baseDir, const std::string& packageNameParam) {
        BLOG("自动检测多平台模型，目录: %s", baseDir.c_str());

        std::string packageName;
        std::string currentPlatform = "phone"; // 默认平台

        // 优先使用传入的包名参数
        if (!packageNameParam.empty()) {
            packageName = packageNameParam;
            BLOG("使用传入的包名: %s", packageName.c_str());
        } else {
            // 从本地模型路径中提取包名
            std::string currentModelPath = _widgetModelSavePath.empty() ?
                                         _widgetDefaultModelSavePath : _widgetModelSavePath;

            size_t fbmPos = currentModelPath.find(".fbm");
            if (fbmPos != std::string::npos) {
                std::string baseName = currentModelPath.substr(0, fbmPos);
                size_t fastbotPos = baseName.find("fastbot_");
                if (fastbotPos != std::string::npos) {
                    std::string nameWithPlatform = baseName.substr(fastbotPos + 8); // 跳过 "fastbot_"

                    // 查找最后一个点，分离包名和平台
                    size_t lastDotPos = nameWithPlatform.find_last_of('.');
                    if (lastDotPos != std::string::npos) {
                        packageName = nameWithPlatform.substr(0, lastDotPos);
                        currentPlatform = nameWithPlatform.substr(lastDotPos + 1);
                    } else {
                        packageName = nameWithPlatform;
                        currentPlatform = "phone"; // 默认平台
                    }
                }
            }
        }

        if (packageName.empty()) {
            BLOG("无法提取包名，跳过多平台模型检测");
            return;
        }

        BLOG("当前包名: %s, 当前平台: %s", packageName.c_str(), currentPlatform.c_str());

        // 清空现有的外部平台模型（重新加载）
        {
            std::lock_guard<std::mutex> lock(_externalModelsLock);
            if (!_externalPlatformModels.empty()) {
                BLOG("清空现有的 %zu 个外部平台模型", _externalPlatformModels.size());
                _externalPlatformModels.clear();
            }
        }
        // 同步清空与外部模型关联的缓存与索引
        {
            std::lock_guard<std::mutex> cacheLock(_externalActionMatchCacheLock);
            _externalActionMatchCache.clear();
        }
        {
            std::lock_guard<std::mutex> idxLock(_externalWidgetIndexLock);
            _externalWidgetVisitedIndex.clear();
        }

        // 搜索其他平台的模型文件
        std::vector<std::string> platformSuffixes = {"tablet", "tv", "car", "watch", "phone"};
        int foundModels = 0;

        for (const auto& platform : platformSuffixes) {
            if (platform == currentPlatform) {
                BLOG("跳过当前平台: %s", platform.c_str());
                continue; // 跳过当前平台
            }

            std::string modelPath = baseDir + "/fastbot_" + packageName + "." + platform + ".fbm";
            BLOG("检查外部平台模型: %s", modelPath.c_str());

            // 检查文件是否存在
            std::ifstream file(modelPath);
            if (file.good()) {
                file.close();
                BLOG("发现外部平台模型: %s", modelPath.c_str());
                foundModels++;

                if (addExternalPlatformModel(modelPath, platform)) {
                    BLOG("成功加载外部平台模型: %s", platform.c_str());
                } else {
                    BLOGE("加载外部平台模型失败: %s", platform.c_str());
                }
            } else {
                BLOG("未找到平台 %s 的模型文件", platform.c_str());
            }
        }

        if (foundModels == 0) {
            BLOG("未找到任何外部平台模型");
        }

        // 检查是否成功加载了模型
        {
            std::lock_guard<std::mutex> lock(_externalModelsLock);
            if (_externalPlatformModels.empty()) {
                BLOG("未成功加载任何外部平台模型");
            } else {
                BLOG("多平台模型检测完成，共加载 %zu 个外部模型:", _externalPlatformModels.size());
                
                for (size_t i = 0; i < _externalPlatformModels.size(); i++) {
                    const auto& platform = _externalPlatformModels[i];
                    BLOG("  %zu) 平台: %s, Actions: %zu, 属性: %zu", 
                         i+1, platform.platformId.c_str(), 
                         platform.reuseModel.size(), platform.actionAttributes.size());
                }
            }
        }
    }

    bool WidgetReusableAgent::addExternalPlatformModel(const std::string& modelPath, const std::string& platformInfo) {
        BLOG("加载外部平台模型: %s (平台: %s)", modelPath.c_str(), platformInfo.c_str());

        try {
            std::ifstream modelFile(modelPath, std::ios::binary);
            if (!modelFile.is_open()) {
                BLOGE("无法打开模型文件: %s", modelPath.c_str());
                return false;
            }

            // 读取文件内容
            std::filebuf* fileBuffer = modelFile.rdbuf();
            std::size_t filesize = fileBuffer->pubseekoff(0, modelFile.end, modelFile.in);
            fileBuffer->pubseekpos(0, modelFile.in);

            auto modelFileData = std::make_unique<char[]>(filesize);
            fileBuffer->sgetn(modelFileData.get(), static_cast<int>(filesize));

            // 解析FlatBuffers数据
            auto widgetReuseFBModel = GetWidgetReuseModel(modelFileData.get());
            if (!widgetReuseFBModel || !widgetReuseFBModel->model()) {
                BLOGE("解析模型文件失败: %s", modelPath.c_str());
                return false;
            }

            // 检查模型中是否有相似度属性
            bool hasSimilarityAttrs = false;
            // 尝试读取第一个条目的相似度属性来判断
            if (widgetReuseFBModel->model()->size() > 0) {
                auto firstEntry = widgetReuseFBModel->model()->Get(0);
                if (firstEntry->similarity_attrs() != nullptr) {
                    hasSimilarityAttrs = true;
                }
            }
            
            BLOG("模型文件: %s, 是否包含相似度属性: %s", 
                 modelPath.c_str(), hasSimilarityAttrs ? "是" : "否");

            // 创建外部平台数据
            ExternalPlatformData platformData;
            platformData.platformId = platformInfo;
            platformData.modelPath = modelPath;

            // 加载基本复用数据
            auto modelDataPtr = widgetReuseFBModel->model();
            int actionWithAttrsCount = 0;

            for (int actionIndex = 0; actionIndex < modelDataPtr->size(); actionIndex++) {
                const auto* reuseEntry = modelDataPtr->Get(actionIndex);
                uint64_t actionHash = reuseEntry->action();

                std::map<uint64_t, int> widgetMap;

                // 处理activities
                auto activities = reuseEntry->activities();
                if (activities) {
                    for (int activityIndex = 0; activityIndex < activities->size(); activityIndex++) {
                        const auto* activityWidgetMap = activities->Get(activityIndex);
                        auto widgets = activityWidgetMap->widgets();

                        if (widgets) {
                            for (int widgetIndex = 0; widgetIndex < widgets->size(); widgetIndex++) {
                                const auto* widgetEntry = widgets->Get(widgetIndex);
                                uint64_t widgetHash = widgetEntry->widget_hash();
                                int count = widgetEntry->count();

                                // 合并不同activity的widget计数
                                if (widgetMap.find(widgetHash) != widgetMap.end()) {
                                    widgetMap[widgetHash] = std::max(widgetMap[widgetHash], count);
                                } else {
                                    widgetMap[widgetHash] = count;
                                }

                                // 加载widget的相似度属性
                                if (widgetEntry->similarity_attrs()) {
                                    const auto* widgetAttrs = widgetEntry->similarity_attrs();

                                    ExternalPlatformData::WidgetAttributes attrs;
                                    attrs.widgetHash = widgetHash;
                                    attrs.widgetText = widgetAttrs->text() ? widgetAttrs->text()->str() : "";
                                    attrs.activityName = widgetAttrs->activity_name() ? widgetAttrs->activity_name()->str() : "";
                                    attrs.widgetResourceId = widgetAttrs->resource_id() ? widgetAttrs->resource_id()->str() : "";

                                    // 读取图标数据（base64格式）
                                    if (widgetAttrs->icon_base64()) {
                                        attrs.widgetIconBase64 = widgetAttrs->icon_base64()->str();
                                    }

                                    platformData.widgetAttributes[widgetHash] = attrs;
                                }
                            }
                        }
                    }
                }

                if (!widgetMap.empty()) {
                    // 将WidgetCountMap转换为WidgetCountMapWithAttrs
                    WidgetCountMapWithAttrs widgetMapWithAttrs;
                    for (const auto& widgetPair : widgetMap) {
                        WidgetCountWithAttributes widgetCountWithAttrs;
                        widgetCountWithAttrs.count = widgetPair.second;
                        // 其他属性保持默认值，因为外部模型可能没有这些信息
                        widgetMapWithAttrs[widgetPair.first] = widgetCountWithAttrs;
                    }
                    platformData.reuseModel[actionHash] = widgetMapWithAttrs;
                }

                // 如果有相似度属性，也加载它们
                if (reuseEntry->similarity_attrs()) {
                    ExternalPlatformData::ActionAttributes attrs;
                    attrs.actionHash = actionHash;

                    const auto* actionAttrs = reuseEntry->similarity_attrs();
                    attrs.actionType = actionAttrs->action_type();
                    attrs.activityName = actionAttrs->activity_name() ? actionAttrs->activity_name()->str() : "";

                    if (actionAttrs->target_widget()) {
                        const auto* widgetAttrs = actionAttrs->target_widget();
                        attrs.widgetText = widgetAttrs->text() ? widgetAttrs->text()->str() : "";
                        attrs.widgetResourceId = widgetAttrs->resource_id() ? widgetAttrs->resource_id()->str() : "";

                        // 读取图标数据（base64格式）
                        if (widgetAttrs->icon_base64()) {
                            attrs.widgetIconBase64 = widgetAttrs->icon_base64()->str();
                        }
                        
                        BLOG("加载action属性: hash=%llu, type=%d, text='%s', resourceId='%s', iconSize=%zu",
                             attrs.actionHash, attrs.actionType, attrs.widgetText.c_str(), 
                             attrs.widgetResourceId.c_str(), attrs.widgetIconBase64.size());
                    }

                    platformData.actionAttributes.push_back(attrs);
                    actionWithAttrsCount++;
                }
            }

            // 如果模型中没有相似度属性，但我们需要它们进行跨平台匹配，则手动创建
            if (platformData.actionAttributes.empty() && !platformData.reuseModel.empty()) {
                BLOG("模型中没有相似度属性，正在手动创建...");
                BLOG("外部模型包含 %zu 个actions，但没有任何action属性", platformData.reuseModel.size());
                
                // 遍历所有action
                for (const auto& actionPair : platformData.reuseModel) {
                    ExternalPlatformData::ActionAttributes attrs;
                    attrs.actionHash = actionPair.first;
                    
                    // 默认值
                    attrs.actionType = 1; // 默认为CLICK类型
                    attrs.widgetText = "";  // 默认空字符串，而不是"未知控件"
                    attrs.widgetResourceId = "";
                    attrs.activityName = "";
                    
                    // 尝试从actionHash中提取信息
                    // 某些哈希值可能包含类型信息
                    if ((attrs.actionHash & 0xFF) == 11) {
                        attrs.actionType = 11; // SCROLL_TOP_DOWN
                    } else if ((attrs.actionHash & 0xFF) == 12) {
                        attrs.actionType = 12; // SCROLL_BOTTOM_UP
                    }
                    
                    BLOG("处理外部模型action: hash=%llu, type=%d, widgetCount=%zu", 
                         attrs.actionHash, attrs.actionType, actionPair.second.size());
                    
                    // 尝试从本地模型中查找相同hash的action，获取更多信息
                    if (this->_newState) {
                        for (const auto& action : this->_newState->getActions()) {
                            if (action && action->hash() == attrs.actionHash) {
                                // 找到了相同hash的action，获取它的属性
                                auto activityNameAction = std::dynamic_pointer_cast<ActivityNameAction>(action);
                                if (activityNameAction) {
                                    attrs.actionType = static_cast<int>(activityNameAction->getActionType());
                                    attrs.activityName = activityNameAction->getActivity() ? *activityNameAction->getActivity() : "";
                                    
                                    auto targetWidget = activityNameAction->getTarget();
                                    if (targetWidget) {
                                        attrs.widgetText = targetWidget->getText();
                                        attrs.widgetResourceId = targetWidget->getResourceID();
                                        
                                        if (targetWidget->hasIcon()) {
                                            attrs.widgetIconBase64 = targetWidget->getIconBase64();
                                        }
                                    }
                                    
                                    BLOG("从本地模型找到匹配action: hash=%llu, type=%d, text='%s', resourceId='%s'",
                                         attrs.actionHash, attrs.actionType, attrs.widgetText.c_str(), attrs.widgetResourceId.c_str());
                                }
                                break;
                            }
                        }
                    }
                    
                    // 尝试从widgetMap中提取更多信息
                    const auto& widgetMap = actionPair.second;
                    if (!widgetMap.empty()) {
                        // 查找出现频率最高的widget
                        uint64_t mostFrequentWidgetHash = 0;
                        int maxCount = 0;
                        for (const auto& widgetPair : widgetMap) {
                            if (widgetPair.second.count > maxCount) {
                                maxCount = widgetPair.second.count;
                                mostFrequentWidgetHash = widgetPair.first;
                            }
                        }
                        
                        BLOG("外部模型action %llu 最频繁的widget: hash=%llu, count=%d", 
                             attrs.actionHash, mostFrequentWidgetHash, maxCount);
                        
                        // 尝试从本地模型中查找这个widget
                        if (this->_newState && mostFrequentWidgetHash != 0) {
                            for (const auto& widget : this->_newState->getWidgets()) {
                                if (widget && widget->hash() == mostFrequentWidgetHash) {
                                    // 使用这个widget的属性补充action属性
                                    if (attrs.widgetText.empty()) {
                                        attrs.widgetText = widget->getText();
                                    }
                                    if (attrs.widgetResourceId.empty()) {
                                        attrs.widgetResourceId = widget->getResourceID();
                                    }
                                    // 不覆盖activity，因为widget没有activity信息
                                    
                                    BLOG("从本地模型找到匹配widget: hash=%llu, text='%s', resourceId='%s'",
                                         mostFrequentWidgetHash, widget->getText().c_str(), widget->getResourceID().c_str());
                                    break;
                                }
                            }
                        }
                    }
                    
                    // 如果仍然没有有效的属性，使用更有意义的默认值
                    if (attrs.widgetText.empty()) {
                        // 尝试从actionHash生成一些信息
                        std::stringstream ss;
                        ss << "Action_" << attrs.actionHash % 1000;
                        attrs.widgetText = ss.str();
                    }
                    
                    if (attrs.widgetResourceId.empty()) {
                        std::stringstream ss;
                        ss << "id_" << attrs.actionHash % 1000;
                        attrs.widgetResourceId = ss.str();
                    }
                    
                    if (attrs.activityName.empty()) {
                        attrs.activityName = "com.netease.cloudmusic.activity.MainActivity"; // 使用常见的主活动名称
                    }
                    
                    BLOG("手动创建action属性: hash=%llu, type=%d, text='%s', resourceId='%s'",
                         attrs.actionHash, attrs.actionType, attrs.widgetText.c_str(), attrs.widgetResourceId.c_str());
                    
                    platformData.actionAttributes.push_back(attrs);
                }
                
                BLOG("手动创建了 %zu 个action属性记录", platformData.actionAttributes.size());
            }

            // 添加到外部模型列表
            {
                std::lock_guard<std::mutex> lock(_externalModelsLock);
                _externalPlatformModels.push_back(platformData);
            }

            BLOG("成功加载外部平台模型: %s, %zu个actions, %d个带属性的actions, %zu个属性",
                 platformInfo.c_str(), platformData.reuseModel.size(), 
                 actionWithAttrsCount, platformData.actionAttributes.size());
             
            // 打印前5个action属性的详细信息，帮助调试
            int debugCount = 0;
            for (const auto& attrs : platformData.actionAttributes) {
                if (debugCount < 5) {
                    BLOG("外部模型action属性[%d]: type=%d, text='%s', resourceId='%s', activity='%s'", 
                         debugCount, attrs.actionType, attrs.widgetText.c_str(), 
                         attrs.widgetResourceId.c_str(), attrs.activityName.c_str());
                    debugCount++;
                }
            }

            return true;

        } catch (const std::exception& e) {
            BLOGE("加载外部平台模型失败: %s", e.what());
            return false;
        }
    }

    bool WidgetReusableAgent::isActionInAnyModel(const ActivityNameActionPtr& action, double similarityThreshold) const {
        if (!action) {
            return false;
        }

        uint64_t actionHash = action->hash();

        // 先检查缓存命中（仅缓存成功匹配的结果）
        {
            std::lock_guard<std::mutex> cacheLock(_externalActionMatchCacheLock);
            auto it = _externalActionMatchCache.find(actionHash);
            if (it != _externalActionMatchCache.end()) {
                const auto& cached = it->second;
                if (cached.found && cached.similarity >= similarityThreshold) {
                    BLOG("外部action匹配命中缓存: platform=%s, similarity=%.3f, actionHash=%llu",
                         cached.platformId.c_str(), cached.similarity, cached.actionHash);
                    return true;
                }
            }
        }

        // // 首先检查本地模型
        // {
        //     std::lock_guard<std::mutex> reuseGuard(this->_widgetReuseModelLock);
        //     if (this->_widgetReuseModel.find(actionHash) != this->_widgetReuseModel.end()) {
        //         return true;
        //     }
        // }

        // 检查外部模型
        auto match = findSimilarActionInExternalModels(action, similarityThreshold);
        return match.found;
    }

    bool WidgetReusableAgent::isWidgetVisitedWithSimilarity(const WidgetPtr& widget, double similarityThreshold) const {
        if (!widget) {
            return false;
        }

        uint64_t widgetHash = widget->hash();

        // 本机模型复用只需要精确匹配（hash匹配）
        return _visitedWidgets.find(widgetHash) != _visitedWidgets.end();
    }

    WidgetReusableAgent::ExternalActionMatch WidgetReusableAgent::findSimilarActionInExternalModels(
        const ActivityNameActionPtr& action, double similarityThreshold) const {

        ExternalActionMatch result;
        result.found = false;
        result.similarity = 0.0;

        if (!action) {
            BLOG("findSimilarActionInExternalModels: action is null");
            return result;
        }

        // 检查action是否有target widget
        auto targetWidget = action->getTarget();
        if (!targetWidget) {
            BLOG("findSimilarActionInExternalModels: action没有target widget");
            return result;
        }

        int currentActionType = static_cast<int>(action->getActionType());
        std::string currentText = targetWidget->getText();
        std::string currentResourceId = targetWidget->getResourceID();
        std::string currentActivityName = action->getActivity() ? *action->getActivity() : "";

        BLOG("开始查找相似action: type=%s(%d), text='%s', resourceId='%s', activity='%s'", 
             actName[action->getActionType()].c_str(), currentActionType,
             currentText.c_str(),
             currentResourceId.c_str(),
             currentActivityName.c_str());

        // 检查外部模型数量
        {
            std::lock_guard<std::mutex> lock(_externalModelsLock);
            BLOG("当前已加载 %zu 个外部平台模型", _externalPlatformModels.size());
            
            if (_externalPlatformModels.empty()) {
                BLOG("没有加载任何外部平台模型，跳过相似度匹配");
                return result;
            }
        }


        BLOG("使用相似度阈值: %.2f（按入参保持不变）", similarityThreshold);

        // 遍历所有外部模型
        try {
            std::lock_guard<std::mutex> lock(_externalModelsLock);
            int matchingTypeCount = 0;
            int totalActionCount = 0;
            bool strictTypeMatching = false;  // 设置为false，不要求严格类型匹配

            for (const auto& platformData : _externalPlatformModels) {
                BLOG("检查平台 %s 的模型，包含 %zu 个action属性", 
                     platformData.platformId.c_str(), platformData.actionAttributes.size());
                 
                if (platformData.actionAttributes.empty()) {
                    BLOG("平台 %s 没有action属性数据，跳过", platformData.platformId.c_str());
                    continue;
                }
                
                // 打印前5个action属性的详细信息，帮助调试
                int debugCount = 0;
                for (const auto& attrs : platformData.actionAttributes) {
                    if (debugCount < 5) {
                        BLOG("外部模型action属性[%d]: type=%d, text='%s', resourceId='%s', activity='%s'", 
                             debugCount, attrs.actionType, attrs.widgetText.c_str(), 
                             attrs.widgetResourceId.c_str(), attrs.activityName.c_str());
                        debugCount++;
                    }
                }
                 
                for (const auto& attrs : platformData.actionAttributes) {
                    totalActionCount++;
                    
                    // 动作类型匹配检查（可选）
                    if (strictTypeMatching && attrs.actionType != currentActionType) {
                        continue;
                    }

                    matchingTypeCount++;
                    
                    BLOG("比较action: 当前=[type=%d, text='%s', resourceId='%s', activity='%s'] vs 外部=[type=%d, text='%s', resourceId='%s', activity='%s']", 
                         currentActionType, currentText.c_str(), currentResourceId.c_str(), currentActivityName.c_str(),
                         attrs.actionType, attrs.widgetText.c_str(), attrs.widgetResourceId.c_str(), attrs.activityName.c_str());

                    // 如果外部模型的属性都是空的，直接跳过
                    if (attrs.widgetText.empty() && attrs.widgetResourceId.empty() && attrs.activityName.empty()) {
                        BLOG("外部模型属性都是空的，跳过相似度计算");
                        continue;
                    }

                    try {
                    // 使用混合相似度计算：当前action对象 vs 外部模型数据
                        BLOG("调用calculateSimilarity计算相似度...");
                    double similarity = ActionSimilarity::calculateSimilarity(
                        action,  // 当前action对象
                        attrs.widgetText, attrs.activityName, attrs.widgetResourceId, attrs.widgetIconBase64);

                        BLOG("计算相似度: 当前='%s' vs 外部='%s', 相似度=%.3f", 
                             currentText.c_str(), attrs.widgetText.c_str(), similarity);

                        if (similarity >= similarityThreshold) {
                            result.found = true;
                            result.similarity = similarity;
                            result.platformId = platformData.platformId;
                            result.actionHash = attrs.actionHash;

                            // 获取对应的widget计数
                            auto it = platformData.reuseModel.find(attrs.actionHash);
                            if (it != platformData.reuseModel.end()) {
                                const auto& widgetMap = it->second;
                                for (const auto& widgetPair : widgetMap) {
                                    result.widgetCounts[widgetPair.first] = widgetPair.second.count;
                                }
                            }

                            BLOG("匹配成功（提前返回）: platform=%s, similarity=%.3f, actionHash=%llu, 阈值=%.2f",
                                 result.platformId.c_str(), result.similarity, result.actionHash, similarityThreshold);
                            // 写入缓存
                            {
                                std::lock_guard<std::mutex> cacheLock(_externalActionMatchCacheLock);
                                _externalActionMatchCache[result.actionHash] = result;
                            }
                            return result; // 提前返回，提升性能
                        } else {
                            BLOG("相似度 %.3f 低于阈值 %.2f，不匹配", similarity, similarityThreshold);
                        }
                    } catch (const std::exception& e) {
                        BLOGE("计算相似度时发生异常: %s", e.what());
                    }
                }
                
                BLOG("检查完成: 总action数=%d, 类型匹配数=%d", totalActionCount, matchingTypeCount);
            }
        } catch (const std::exception& e) {
            BLOGE("查找相似action时发生异常: %s", e.what());
        }

        if (result.found) {
            BLOG("在外部模型中找到相似action: 平台=%s, 相似度=%.3f, widgetCounts=%zu",
                 result.platformId.c_str(), result.similarity, result.widgetCounts.size());
        } else {
            BLOG("在所有外部模型中均未找到相似action");
        }

        return result;
    }



    double WidgetReusableAgent::probabilityOfVisitingNewWidgetsFromExternalModel(
        const ActivityStateActionPtr& action,
        const ExternalActionMatch& externalMatch) const {

        if (!externalMatch.found || externalMatch.widgetCounts.empty()) {
            return 0.0;
        }

        BLOG("计算外部模型中action的widget访问概率: platform=%s, widgets=%zu",
             externalMatch.platformId.c_str(), externalMatch.widgetCounts.size());

        int totalWidgets = 0;
        int unvisitedWidgets = 0;

        // 遍历外部模型中的widget计数
        for (const auto& widgetEntry : externalMatch.widgetCounts) {
            uint64_t widgetHash = widgetEntry.first;
            int count = widgetEntry.second;

            totalWidgets += count;

            // 检查这个widget是否已经被访问过
            // 由于是外部模型的widget，需要使用相似度匹配
            bool isVisited = false;

            // 首先使用索引与精确匹配
            if (_visitedWidgets.find(widgetHash) != _visitedWidgets.end()) {
                isVisited = true;
            } else {
                // 快速索引：platformId + externalWidgetHash 对应的本地已判相似widget集合
                bool indexedHit = false;
                {
                    std::lock_guard<std::mutex> idxLock(_externalWidgetIndexLock);
                    auto pit = _externalWidgetVisitedIndex.find(externalMatch.platformId);
                    if (pit != _externalWidgetVisitedIndex.end()) {
                        auto wit = pit->second.find(widgetHash);
                        if (wit != pit->second.end()) {
                            for (const auto& localHash : wit->second) {
                                if (_visitedWidgets.find(localHash) != _visitedWidgets.end()) {
                                    indexedHit = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (indexedHit) {
                    BLOG("外部widget命中相似索引，视为已访问");
                    isVisited = true;
                } else {
                // 如果没有精确匹配或索引命中，使用相似度匹配
                // 查找外部模型中该widget的属性
                auto externalWidgetAttr = this->findExternalWidgetAttributes(widgetHash, externalMatch.platformId);
                if (externalWidgetAttr) {
                    // 遍历当前状态中已访问的widgets，进行相似度比较
                    if (this->_newState) {
                        for (const auto& currentWidget : this->_newState->getWidgets()) {
                            if (currentWidget && _visitedWidgets.find(currentWidget->hash()) != _visitedWidgets.end()) {
                                // 这个widget已经被访问过，检查与外部widget的相似度
                                // 使用混合相似度计算：当前对象 vs 外部模型数据
                                double similarity = ActionSimilarity::calculateSimilarity(
                                    currentWidget, "",  // 当前widget对象和activity
                                    externalWidgetAttr->widgetText, externalWidgetAttr->activityName,
                                    externalWidgetAttr->widgetResourceId, externalWidgetAttr->widgetIconBase64);

                                if (similarity >= 0.5) {
                                    BLOG("外部widget与已访问widget相似度匹配: 相似度=%.3f", similarity);
                                    // 写入索引缓存
                                    {
                                        std::lock_guard<std::mutex> idxLock(_externalWidgetIndexLock);
                                        _externalWidgetVisitedIndex[externalMatch.platformId][widgetHash].insert(currentWidget->hash());
                                    }
                                    isVisited = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                }
            }

            if (!isVisited) {
                unvisitedWidgets += count;
            }
        }

        if (totalWidgets == 0) {
            return 0.0;
        }

        double probability = static_cast<double>(unvisitedWidgets) / totalWidgets;

        BLOG("外部模型widget概率计算: 总widgets=%d, 未访问=%d, 概率=%.3f",
             totalWidgets, unvisitedWidgets, probability);

        return probability;
    }

    const WidgetReusableAgent::ExternalPlatformData::WidgetAttributes* WidgetReusableAgent::findExternalWidgetAttributes(
        uint64_t widgetHash, const std::string& platformId) const {

        std::lock_guard<std::mutex> lock(_externalModelsLock);

        for (const auto& platformData : _externalPlatformModels) {
            if (platformData.platformId == platformId) {
                auto it = platformData.widgetAttributes.find(widgetHash);
                if (it != platformData.widgetAttributes.end()) {
                    return &(it->second);
                }
                break;
            }
        }

        return nullptr;
    }

    ActionPtr WidgetReusableAgent::selectActionByQValue() {
        ActionPtr returnAction = nullptr;
        float maxQ = -MAXFLOAT;

        auto modelPointer = this->_model.lock();
        if (!modelPointer) {
            BLOGE("Model pointer is null in selectActionByQValue");
            return nullptr;
        }

        const GraphPtr &graphRef = modelPointer->getGraph();
        auto visitedActivities = graphRef->getVisitedActivities();

        BLOG("WidgetReusableAgent: selectActionByQValue with multi-platform support");

        for (auto action: this->_newState->getActions()) {
            double qv = 0.0;
            uintptr_t actionHash = action->hash();

            // 如果action未被访问过，优先返回
            // if (action->getVisitedCount() <= 0) {
            //     bool inLocalModel = (this->_widgetReuseModel.find(actionHash) != this->_widgetReuseModel.end());
            //     ExternalActionMatch externalMatch;

            //     auto activityNameAction = std::dynamic_pointer_cast<ActivityNameAction>(action);
            //     if (activityNameAction) {
            //         externalMatch = findSimilarActionInExternalModels(activityNameAction, 0.8);
            //     }

            //     if (inLocalModel || externalMatch.found) {
            //         // 计算概率
            //         if (inLocalModel) {
            //             qv += this->probabilityOfVisitingNewActivities(action, visitedActivities);
            //         } else if (externalMatch.found) {
            //             qv += this->probabilityOfVisitingNewWidgetsFromExternalModel(
            //                 action, externalMatch);

            //         }
            //     } else {
            //         BLOG("qvalue pick return an unvisited action not in any model: %s", action->toString().c_str());
            //         return action;
            //     }
            // }

            // 添加Q值
            qv += getQValue(action);
            qv /= 0.1; // entropyAlpha

            // 添加随机因子
            auto uniform = static_cast<float>(static_cast<float>(randomInt(0, 10)) / 10.0f);
            if (uniform < std::numeric_limits<float>::min())
                uniform = std::numeric_limits<float>::min();
            qv -= log(-log(uniform));

            if (qv > maxQ) {
                maxQ = qv;
                returnAction = action;
            }
        }

        if (returnAction) {
            BLOG("WidgetReusableAgent: selectActionByQValue selected action hash=%llu with Q=%.3f",
                 returnAction->hash(), maxQ);
        } else {
            BLOG("WidgetReusableAgent: selectActionByQValue found no suitable action");
        }

        return returnAction;
    }

} // namespace fastbotx