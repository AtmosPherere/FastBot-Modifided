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
    typedef std::map<stringPtr, WidgetCountMap> LocalActivityWidgetMap;// activity -> widget map
    typedef std::map<uint64_t, LocalActivityWidgetMap> WidgetReuseEntryIntMap;// action_hash -> activity map
    typedef std::map<uint64_t, double> WidgetReuseEntryQValueMap;

    class WidgetReusableAgent : public ModelReusableAgent {
    public:
        explicit WidgetReusableAgent(const ModelPtr &model);
        virtual ~WidgetReusableAgent();

        void updateReuseModel() override; // 重写更新逻辑
        void saveReuseModel(const std::string &modelFilepath) override;
        void loadReuseModel(const std::string &modelFilepath) override;

    protected:
        double probabilityOfVisitingNewActivities(const ActivityStateActionPtr &action,
                                                  const stringPtrSet &visitedActivities) const override;
        double computeRewardOfLatestAction() override;
        double getStateActionExpectationValue(const WidgetPtr &widget,
                                                  const stringPtrSet &visitedActivities) const;
    private:
        // 可以添加新的成员变量或方法
        WidgetReuseEntryIntMap _widgetReuseModel;
        WidgetReuseEntryQValueMap _widgetReuseQValue;
        std::string _widgetModelSavePath;
        std::string _widgetDefaultModelSavePath;
        static std::string DefaultWidgetModelSavePath; // if the saved path is not specified, use this as the default.
        std::mutex _widgetReuseModelLock;
        
};

} // namespace fastbotx

#endif // WIDGET_REUSABLE_AGENT_H_ 