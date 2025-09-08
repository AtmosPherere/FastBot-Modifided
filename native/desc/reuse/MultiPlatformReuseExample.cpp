/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */
/**
 * @authors 
 */
#ifndef MultiPlatformReuseExample_CPP_
#define MultiPlatformReuseExample_CPP_

#include "../agent/WidgetReusableAgent.h"
#include "../desc/reuse/ActionSimilarity.h"
#include "../utils.hpp"

namespace fastbotx {

/**
 * 多平台复用示例
 * 
 * 展示如何在不同平台（phone、tablet、TV、车载等）之间复用测试经验
 */
class MultiPlatformReuseExample {
public:
    static void demonstrateMultiPlatformScenario() {
        BLOG("=== 多平台复用场景示例 ===");
        
        BLOG("场景描述:");
        BLOG("- 网易云音乐有多个平台版本：手机端、平板端、TV端、车载端");
        BLOG("- 虽然APK包不同，但功能相似，可以复用测试经验");
        BLOG("- 通过widget的四个属性进行相似度匹配");
        
        BLOG("模型文件命名规范:");
        BLOG("- 手机端: fastbot_com.netease.cloudmusic.phone.fbm");
        BLOG("- 平板端: fastbot_com.netease.cloudmusic.tablet.fbm");
        BLOG("- TV端: fastbot_com.netease.cloudmusic.tv.fbm");
        BLOG("- 车载端: fastbot_com.netease.cloudmusic.car.fbm");
        
        BLOG("=== 多平台场景示例完成 ===");
    }
    
    static void demonstrateAutoDetection() {
        BLOG("=== 自动检测多平台模型示例 ===");
        
        BLOG("自动检测逻辑:");
        BLOG("1. 从当前模型路径提取包名和平台信息");
        BLOG("   例如: fastbot_com.netease.cloudmusic.phone.fbm");
        BLOG("   -> 包名: com.netease.cloudmusic");
        BLOG("   -> 当前平台: phone");
        
        BLOG("2. 在/sdcard目录下搜索其他平台的模型文件");
        BLOG("   搜索模式: fastbot_<包名>.<其他平台>.fbm");
        
        BLOG("3. 自动加载找到的外部平台模型");
        BLOG("   - 解析FlatBuffers数据");
        BLOG("   - 提取action和widget的相似度属性");
        BLOG("   - 缓存到内存中用于相似度匹配");
        
        BLOG("示例代码:");
        BLOG("// 在WidgetReusableAgent构造函数中自动调用");
        BLOG("autoLoadMultiPlatformModels(\"/sdcard\");");
        
        BLOG("=== 自动检测示例完成 ===");
    }
    
    static void demonstrateActionSelection() {
        BLOG("=== 多平台Action选择示例 ===");
        
        BLOG("selectUnperformedActionNotInReuseModel修改:");
        BLOG("1. 检查action是否在本地模型中");
        BLOG("2. 检查action是否在外部模型中（相似度匹配）");
        BLOG("3. 只有既不在本地也不在外部的action才进入候选集");
        
        BLOG("selectUnperformedActionInReuseModel修改:");
        BLOG("1. 本地模型中的action: 正常计算概率");
        BLOG("2. 外部模型中的相似action:");
        BLOG("   - 使用外部模型数据计算widget访问概率");
        BLOG("   - 根据相似度调整质量值");
        BLOG("   - qualityValue *= similarity");
        
        BLOG("相似度匹配过程:");
        BLOG("for (外部模型中的每个action) {");
        BLOG("  if (actionType匹配) {");
        BLOG("    similarity = calculateWidgetSimilarity(");
        BLOG("      currentText, currentActivityName, currentResourceId, currentIcon,");
        BLOG("      externalText, externalActivityName, externalResourceId, externalIcon");
        BLOG("    );");
        BLOG("    if (similarity >= 0.8) {");
        BLOG("      使用外部模型数据计算概率");
        BLOG("    }");
        BLOG("  }");
        BLOG("}");
        
        BLOG("=== Action选择示例完成 ===");
    }
    
    static void demonstrateSimilarityMatching() {
        BLOG("=== 相似度匹配示例 ===");
        
        BLOG("四个属性的相似度计算:");
        BLOG("1. text (权重: 35%): 文本完全匹配或包含关系");
        BLOG("2. activity_name (权重: 10%): Activity名称匹配");
        BLOG("3. resource_id (权重: 20%): 资源ID匹配，支持资源名称提取");
        BLOG("4. icon (权重: 35%): 图标数据比较");
        
        BLOG("匹配示例:");
        BLOG("手机端播放按钮:");
        BLOG("  text: \"播放\"");
        BLOG("  activity_name: \"com.netease.cloudmusic.MainActivity\"");
        BLOG("  resource_id: \"com.netease.cloudmusic:id/play_btn\"");
        BLOG("  icon: [播放图标数据]");
        
        BLOG("平板端播放按钮:");
        BLOG("  text: \"播放\"");
        BLOG("  activity_name: \"com.netease.cloudmusic.MainActivity\"");
        BLOG("  resource_id: \"com.netease.cloudmusic:id/play_button\"");
        BLOG("  icon: [相似播放图标数据]");
        
        BLOG("相似度计算:");
        BLOG("  text: 1.0 (完全匹配)");
        BLOG("  activity_name: 1.0 (完全匹配)");
        BLOG("  resource_id: 0.9 (资源名称相似: play_btn vs play_button)");
        BLOG("  icon: 0.8 (图标相似)");
        BLOG("  最终相似度: 0.35*1.0 + 0.1*1.0 + 0.2*0.9 + 0.35*0.8 = 0.91");
        
        BLOG("=== 相似度匹配示例完成 ===");
    }
    
    static void demonstrateWidgetProbabilityCalculation() {
        BLOG("=== Widget概率计算示例 ===");
        
        BLOG("本地模型概率计算:");
        BLOG("- 使用probabilityOfVisitingNewWidgets");
        BLOG("- 基于本地的widget访问统计");
        
        BLOG("外部模型概率计算:");
        BLOG("- 使用probabilityOfVisitingNewWidgetsFromExternalModel");
        BLOG("- 基于外部模型的widget计数");
        BLOG("- 考虑相似度调整");
        
        BLOG("外部模型概率计算步骤:");
        BLOG("1. 获取外部模型中匹配action的widget计数");
        BLOG("2. 检查这些widget是否已被访问");
        BLOG("3. 计算未访问widget的比例");
        BLOG("4. 根据action相似度调整最终概率");
        
        BLOG("示例:");
        BLOG("外部模型中播放action有3个widgets: [widget1:5, widget2:3, widget3:2]");
        BLOG("当前已访问: widget1");
        BLOG("未访问widgets: widget2(3) + widget3(2) = 5");
        BLOG("总计数: 5 + 3 + 2 = 10");
        BLOG("基础概率: 5/10 = 0.5");
        BLOG("相似度: 0.91");
        BLOG("最终概率: 0.5 * 0.91 = 0.455");
        
        BLOG("=== Widget概率计算示例完成 ===");
    }
    
    static void demonstrateCompleteWorkflow() {
        BLOG("=== 完整工作流程示例 ===");
        
        BLOG("阶段1: 手机端测试");
        BLOG("1. 创建WidgetReusableAgent");
        BLOG("2. 加载本地模型: agent->loadReuseModel(\"com.netease.cloudmusic.phone\")");
        BLOG("3. 进行测试，积累复用数据");
        BLOG("4. 保存模型: 自动保存到fastbot_com.netease.cloudmusic.phone.fbm");
        
        BLOG("阶段2: 平板端测试");
        BLOG("1. 创建WidgetReusableAgent");
        BLOG("2. 加载本地模型: agent->loadReuseModel(\"com.netease.cloudmusic.tablet\")");
        BLOG("3. 自动检测外部模型: 发现fastbot_com.netease.cloudmusic.phone.fbm");
        BLOG("4. 自动加载外部模型数据");
        BLOG("5. 开始测试，自动进行多平台复用");
        
        BLOG("阶段3: 测试过程中的复用");
        BLOG("1. selectUnperformedActionNotInReuseModel:");
        BLOG("   - 过滤掉在任何模型中找到的action");
        BLOG("   - 只选择完全未知的action进行探索");
        
        BLOG("2. selectUnperformedActionInReuseModel:");
        BLOG("   - 优先选择本地模型中的action");
        BLOG("   - 对于外部模型中的相似action，使用相似度调整概率");
        
        BLOG("3. 模型更新:");
        BLOG("   - 只更新本地模型");
        BLOG("   - 外部模型保持只读");
        
        BLOG("关键优势:");
        BLOG("- 自动检测和加载多平台模型");
        BLOG("- 基于widget属性的语义匹配");
        BLOG("- 保持本地模型的独立性");
        BLOG("- 提高测试效率和覆盖率");
        
        BLOG("=== 完整工作流程示例完成 ===");
    }
    
    static void demonstrateUsageExample() {
        BLOG("=== 使用示例 ===");
        
        BLOG("C++代码示例:");
        BLOG("// 1. 创建agent（自动检测多平台模型）");
        BLOG("auto agent = std::make_unique<WidgetReusableAgent>(model);");
        BLOG("");
        BLOG("// 2. 加载本地模型（触发多平台检测）");
        BLOG("agent->loadReuseModel(\"com.netease.cloudmusic.tablet\");");
        BLOG("");
        BLOG("// 3. 开始测试（自动使用多平台复用）");
        BLOG("while (testing) {");
        BLOG("  ActionPtr action = agent->selectActionByQValue();");
        BLOG("  if (action) {");
        BLOG("    executeAction(action);");
        BLOG("    agent->updateReuseModel(currentState, action);");
        BLOG("  }");
        BLOG("}");
        
        BLOG("文件结构示例:");
        BLOG("/sdcard/");
        BLOG("├── fastbot_com.netease.cloudmusic.phone.fbm    # 手机端模型");
        BLOG("├── fastbot_com.netease.cloudmusic.tablet.fbm   # 平板端模型（当前）");
        BLOG("├── fastbot_com.netease.cloudmusic.tv.fbm       # TV端模型");
        BLOG("└── fastbot_com.netease.cloudmusic.car.fbm      # 车载端模型");
        
        BLOG("=== 使用示例完成 ===");
    }
    
    static void runAllExamples() {
        BLOG("开始运行所有多平台复用示例...");
        
        try {
            demonstrateMultiPlatformScenario();
            demonstrateAutoDetection();
            demonstrateActionSelection();
            demonstrateSimilarityMatching();
            demonstrateWidgetProbabilityCalculation();
            demonstrateCompleteWorkflow();
            demonstrateUsageExample();
        } catch (const std::exception& e) {
            BLOGE("示例运行过程中发生错误: %s", e.what());
        }
        
        BLOG("所有示例运行完成");
        BLOG("");
        BLOG("总结:");
        BLOG("- 自动检测和加载多平台模型");
        BLOG("- 基于ActionSimilarity::calculateSimilarity进行语义匹配");
        BLOG("- 在action选择时考虑外部模型的相似action");
        BLOG("- 根据相似度调整质量值和概率");
        BLOG("- 只更新本地模型，保持外部模型只读");
        BLOG("- 提高跨平台测试的效率和覆盖率");
    }
};

} // namespace fastbotx

#endif // MultiPlatformReuseExample_CPP_
