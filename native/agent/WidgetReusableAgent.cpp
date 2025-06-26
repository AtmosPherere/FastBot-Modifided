#include "WidgetReusableAgent.h"
#include "../storage/WidgetReuseModel_generated.h"
#include "Base.h"
#include "flatbuffers/flatbuffers.h"
#include <fstream>
#include <memory>
#include <string>
#include <utility>

namespace fastbotx {

WidgetReusableAgent::WidgetReusableAgent(const ModelPtr &model)
    : ModelReusableAgent(model) {
    // 初始化代码
}

WidgetReusableAgent::~WidgetReusableAgent() {
    BLOG("save widget reuse model in destruct");
    this->saveReuseModel(this->_widgetModelSavePath);
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
    stringPtr activity = this->_newState->getActivityString();
    if (activity == nullptr)
        return;
    {
        std::lock_guard<std::mutex> reuseGuard(this->_widgetReuseModelLock);
        // 1. 获取/创建 action_hash 对应的 activity map
        auto &activityMap = this->_widgetReuseModel[hash];
        // 2. 获取/创建 activity 对应的 widget map
        auto &widgetMap = activityMap[activity];
        // 3. 计数+1（activity的计数）
        int newCount = 1;//如果widgetMap为空，则newCount为1
        if (!widgetMap.empty()) {
            newCount = widgetMap.begin()->second + 1;
            BDLOG("update reuse model: action_hash=%llu, activity=%s, old_count=%d, new_count=%d", hash, activity->c_str(), widgetMap.begin()->second, newCount);
        } else {
            BDLOG("init reuse model: action_hash=%llu, activity=%s, new_count=%d", hash, activity->c_str(), newCount);
        }
        for (const auto &widget : this->_newState->getWidgets()) {
            auto widgetHash = widget->hash();
            widgetMap[widgetHash] = newCount;
        }
    }
}

// action_hash_1: {
    //     activity_1: {
    //          widget_hash_1: count_1,
    //          widget_hash_2: count_1,
    //          widget_hash_3: count_1,
    //          widget_hash_4: count_1,
    //     },
    //     activity_2: {
    //          widget_hash_5: count_2,
    //          widget_hash_6: count_2,
    //          widget_hash_7: count_2,
    //          widget_hash_8: count_2,
    //     },
    // }
    // ...
double WidgetReusableAgent::probabilityOfVisitingNewActivities(const ActivityStateActionPtr &action,
                                                             const stringPtrSet &visitedActivities) const {
    // 这里实现你的自定义逻辑
    double value = 0.0;
    int total = 0;
    int unvisited = 0;
    
    // 查找action在widget重用模型中的记录
    auto actionMapIterator = this->_widgetReuseModel.find(action->hash());
    if (actionMapIterator != this->_widgetReuseModel.end()) {
        // 遍历包含活动名称和访问计数的映射
        for (const auto &activityCountMapIterator: (*actionMapIterator).second) {
            if (!activityCountMapIterator.second.empty()) {
                total += activityCountMapIterator.second.begin()->second;// 计算当前action的总执行次数
            }
            stringPtr activity = activityCountMapIterator.first;
            if (visitedActivities.find(activity) == visitedActivities.end()) {
                    unvisited += activityCountMapIterator.second.size();// 未访问过的widget的个数
            }
        }
        if (total > 0 && unvisited > 0) {
                value = static_cast<double>(unvisited) / total;// 计算未访问过的widget的个数占总执行次数的百分比，可能大于1
        }
    }
    return value;
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
                // Get the expectation of this action for accessing unvisited new activity.
                rewardValue = this->probabilityOfVisitingNewActivities(lastSelectedAction,
                                                                       visitedActivities);
                // If this is an action not in reuse model, this action is new and should definitely be used
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
                // 如果该action是新action，value加1，否则加0.5
                if (this->_widgetReuseModel.find(actionHash) == this->_widgetReuseModel.end()) {
                    value += 1.0;
                } else if (action->getVisitedCount() >= 1) {
                    value += 0.5;
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
            for (const auto &actionIterator : this->_widgetReuseModel) {
                uint64_t actionHash = actionIterator.first;
                const LocalActivityWidgetMap &activityMap = actionIterator.second;
                
                std::vector<flatbuffers::Offset<fastbotx::ActivityWidgetMap>> activityVector;
                for (const auto &activityIterator : activityMap) {
                    const stringPtr &activity = activityIterator.first;
                    const WidgetCountMap &widgetMap = activityIterator.second;
                    
                    std::vector<flatbuffers::Offset<fastbotx::WidgetCount>> widgetVector;
                    for (const auto &widgetIterator : widgetMap) {
                        uint64_t widgetHash = widgetIterator.first;
                        int count = widgetIterator.second;
                        auto widgetCount = CreateWidgetCount(builder, widgetHash, count);
                        widgetVector.push_back(widgetCount);
                    }
                    
                    auto activityWidgetMap = CreateActivityWidgetMap(
                        builder, 
                        builder.CreateString(*activity), 
                        builder.CreateVector(widgetVector)
                    );
                    activityVector.push_back(activityWidgetMap);
                }
                
                auto reuseEntry = CreateReuseEntry(
                    builder, 
                    actionHash, 
                    builder.CreateVector(activityVector)
                );
                reuseEntryVector.push_back(reuseEntry);
            }
        }
        
        auto widgetReuseModel = CreateWidgetReuseModel(builder, builder.CreateVector(reuseEntryVector));
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

    void WidgetReusableAgent::loadReuseModel(const std::string &packageName) {
        std::string modelFilePath = STORAGE_PREFIX + packageName + ".fbm"; // widget binary model
        
        this->_widgetModelSavePath = modelFilePath;
        if (!this->_widgetModelSavePath.empty()) {
            this->_widgetDefaultModelSavePath = STORAGE_PREFIX + packageName + ".tmp.fbm";
        }
        BLOG("begin load widget reuse model: %s", this->_widgetModelSavePath.c_str());
        
        std::ifstream modelFile(modelFilePath, std::ios::binary | std::ios::in);
        if (modelFile.fail()) {
            BLOG("read widget reuse model file %s failed, check if file exists!", modelFilePath.c_str());
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
            
            LocalActivityWidgetMap activityMap;
            for (int activityIndex = 0; activityIndex < activities->size(); activityIndex++) {
                const ::fastbotx::ActivityWidgetMap* activityEntry = activities->Get(activityIndex);
                stringPtr activity = stringPtr(new std::string(activityEntry->activity()->str()));
                const flatbuffers::Vector<flatbuffers::Offset<::fastbotx::WidgetCount>>* widgets = activityEntry->widgets();
                
                WidgetCountMap widgetMap;
                for (int widgetIndex = 0; widgetIndex < widgets->size(); widgetIndex++) {
                    const ::fastbotx::WidgetCount* widgetEntry = widgets->Get(widgetIndex);
                    uint64_t widgetHash = widgetEntry->widget_hash();
                    int count = widgetEntry->count();
                    widgetMap[widgetHash] = count;
                }
                activityMap[activity] = widgetMap;
            }
            
            if (!activityMap.empty()) {
                std::lock_guard<std::mutex> reuseGuard(this->_widgetReuseModelLock);
                this->_widgetReuseModel[actionHash] = activityMap;
            }
        }
        
        BLOG("loaded widget reuse model contains actions: %zu", this->_widgetReuseModel.size());
        delete[] modelFileData;
    }

} // namespace fastbotx 