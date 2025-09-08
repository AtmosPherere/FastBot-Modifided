/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */
/**
 * @authors Jianqiang Guo, Yuhui Su, Zhao Zhang
 */

#ifndef Agent_Factory_CPP_
#define Agent_Factory_CPP_

#include "AgentFactory.h"
#include "utils.hpp"
#include "Model.h"
#include "ModelReusableAgent.h"
#include "WidgetReusableAgent.h" 
#include "json.hpp"
#include "Preference.h"

namespace fastbotx {
/// No matter what kind of Algorithm you choose, only the ModelReusableAgent will be used.
/// \param agentT
/// \param model
/// \param deviceType
/// \return
    AbstractAgentPtr
    AgentFactory::create(AlgorithmType agentT, const ModelPtr &model, DeviceType deviceType) {
        AbstractAgentPtr agent = nullptr;
        // use WidgetReusableAgent under all circumstances.
        ReuseAgentPtr reuseAgent = std::make_shared<WidgetReusableAgent>(model);

        // 使用原来的后台保存线程，现在路径问题已经修复
        // 虚函数调用会正确调用到 WidgetReusableAgent::saveReuseModel
        threadDelayExec(3000, false, &ModelReusableAgent::threadModelStorage,
                        std::weak_ptr<fastbotx::ModelReusableAgent>(reuseAgent));

        agent = reuseAgent;
        return agent;
    }

}

#endif
