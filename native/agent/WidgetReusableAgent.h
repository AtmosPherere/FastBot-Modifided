#ifndef WIDGET_REUSABLE_AGENT_H_
#define WIDGET_REUSABLE_AGENT_H_

#include "ModelReusableAgent.h"
#include "AbstractAgent.h"
#include "State.h"
#include "Action.h"
#include "Model.h"
#include <vector>
#include <map>

namespace fastbotx {

    typedef std::map<uint64_t, int> WidgetCountMap;// widget_hash -> count
    
    // 扩展的widget计数结构，包含相似度属性
    struct WidgetCountWithAttributes {
        int count;
        std::string text;
        std::string activityName;
        std::string resourceId;
        std::string iconBase64;
        
        WidgetCountWithAttributes() : count(0) {}
        WidgetCountWithAttributes(int c) : count(c) {}
    };
    
    // 扩展的action属性结构
    struct ActionAttributes {
        int actionType;
        std::string activityName;
        std::string targetWidgetText;
        std::string targetWidgetResourceId;
        std::string targetWidgetIconBase64;
        
        ActionAttributes() : actionType(1) {}
    };
    
    // 统一使用带属性的数据结构
    typedef std::map<uint64_t, WidgetCountWithAttributes> WidgetCountMapWithAttrs;
    typedef std::map<uint64_t, WidgetCountMapWithAttrs> WidgetReuseEntryIntMap;
    typedef std::map<uint64_t, double> WidgetReuseEntryQValueMap;

    class WidgetReusableAgent : public ModelReusableAgent {
    public:
        // ========== 多平台复用数据结构 ==========

        // 外部平台模型数据
        struct ExternalPlatformData {
            std::string platformId;
            std::string modelPath;
            WidgetReuseEntryIntMap reuseModel;  // action_hash -> widget_counts

            // 相似度匹配所需的属性数据
            struct ActionAttributes {
                uint64_t actionHash;
                int actionType;
                std::string widgetText;
                std::string activityName;
                std::string widgetResourceId;
                std::string widgetIconBase64;  // 改为base64字符串
            };

            struct WidgetAttributes {
                uint64_t widgetHash;
                std::string widgetText;
                std::string activityName;
                std::string widgetResourceId;
                std::string widgetIconBase64;  // 改为base64字符串
            };

            std::vector<ActionAttributes> actionAttributes;
            std::map<uint64_t, WidgetAttributes> widgetAttributes; // widget_hash -> attributes
        };
        explicit WidgetReusableAgent(const ModelPtr &model);
        virtual ~WidgetReusableAgent();

        void updateReuseModel() override; // 重写更新逻辑
        void updateStrategy() override; // 重写策略更新逻辑，添加控件访问跟踪
        void saveReuseModel(const std::string &modelFilepath) override;
        void loadReuseModel(const std::string &modelFilepath) override;

        // 强制保存模型，无论是否为空
        void forceSaveReuseModel();

    protected:
        double probabilityOfVisitingNewWidgets(const ActivityStateActionPtr &action,
                                               const stringPtrSet &visitedActivities) const;
        double probabilityOfVisitingNewActivities(const ActivityStateActionPtr &action,
                                                  const stringPtrSet &visitedActivities) const override;
        double computeRewardOfLatestAction() override;
        double getStateActionExpectationValue(const WidgetPtr &widget,
                                                  const stringPtrSet &visitedActivities) const;

        // 重写父类的方法，使用 _widgetReuseModel 而不是 _reuseModel
        ActionPtr selectUnperformedActionInReuseModel() const override;
        ActionPtr selectUnperformedActionNotInReuseModel() const override;
        ActionPtr selectActionByQValue();  // 支持多平台复用

        // 控件访问跟踪方法
        void updateVisitedWidgets(const StatePtr& state);
        void clearVisitedWidgets();

        // ========== 多平台复用功能 ==========

        // 自动检测并加载多平台模型
        void autoLoadMultiPlatformModels(const std::string& baseDir = "/sdcard", const std::string& packageNameParam = "");

        // 添加外部平台模型
        bool addExternalPlatformModel(const std::string& modelPath, const std::string& platformInfo);

        // 检查action是否在任何模型中（本地或外部）
        bool isActionInAnyModel(const ActivityNameActionPtr& action, double similarityThreshold = 0.8) const;

        // 检查widget是否已访问（本机模型hash匹配）
        bool isWidgetVisitedWithSimilarity(const WidgetPtr& widget, double similarityThreshold = 0.8) const;

        // 在外部模型中查找相似的action
        struct ExternalActionMatch {
            bool found;
            std::string platformId;
            uint64_t actionHash;
            std::map<uint64_t, int> widgetCounts;
            double similarity;
        };

        ExternalActionMatch findSimilarActionInExternalModels(const ActivityNameActionPtr& action,
                                                             double similarityThreshold = 0.8) const;

        // 使用外部模型数据计算访问新widget的概率
        double probabilityOfVisitingNewWidgetsFromExternalModel(const ActivityStateActionPtr& action,
                                                               const ExternalActionMatch& externalMatch) const;

        // 查找外部模型中widget的属性
        const ExternalPlatformData::WidgetAttributes* findExternalWidgetAttributes(uint64_t widgetHash, const std::string& platformId) const;



    private:
        // 统一使用带属性的数据结构
        WidgetReuseEntryIntMap _widgetReuseModel;           // action_hash -> widget_hash -> WidgetCountWithAttributes
        WidgetReuseEntryQValueMap _widgetReuseQValue;
        std::map<uint64_t, ActionAttributes> _actionAttributes;      // action_hash -> ActionAttributes
        
        std::string _widgetModelSavePath;
        std::string _widgetDefaultModelSavePath;
        static std::string DefaultWidgetModelSavePath; // if the saved path is not specified, use this as the default.
        mutable std::mutex _widgetReuseModelLock;

        // 跟踪当前测试轮次中访问过的控件hash值
        std::set<uint64_t> _visitedWidgets;

        // 外部平台模型列表
        std::vector<ExternalPlatformData> _externalPlatformModels;

        // 外部模型访问锁
        mutable std::mutex _externalModelsLock;
        
};

} // namespace fastbotx

#endif // WIDGET_REUSABLE_AGENT_H_ 