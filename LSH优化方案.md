# LSH优化方案

## 总体思路

1. 向量归一化
2. 构建LSH 索引：离线完成
3. 在线查询：对于当前 action/widget 仅计算一次向量，查 LSH 得到 top-K 候选，再用ActionSimilarity::calculateSimilarity(...) 做精算与阈值筛选。
4. 加入缓存，每次成功匹配action或widget都建立索引，加入缓存，下次遇到相同action或widget直接查缓存。



## 向量归一化

1. 对 action（或 widget）生成向量：

2. 文本 text: 取 getBertEmbedding(text) → 768 维

3. resource-id: 预处理后 getBertEmbedding(preprocessResourceId(id)) → 768 维

4. activity: 预处理后 getBertEmbedding(preprocessActivityName(activity)) → 768 维

5. icon: 若有图标 → CLIP image encoder → 512 维；无图标则用全零
6. 拼接后做按权重缩放并 L2 归一：与你当前权重对齐（有图标: text 0.35, id 0.2, activity 0.1, icon 0.35；无图标: 0.4/0.2/0.4/0），对每个子向量乘以权重再拼接，最后整体 normalize。

## 离线预计算

1. 预先把外部模型中保存的数据转为“embedding向量+LSH索引”文件，实际运行时直接加载预先计算好的即可。

2. LSH参数：k（每表 bit 数），L（表数），candidate（top-K 候选上限），probe_hamming_radius（探查相邻桶的数量）

3. 离线向量与索引

   - 新增可执行工具 offline_index_builder（桌面构建），输入 .fbm，输出同名前缀的：

   - <xxx>.actions.vec/.actions.lsh：外部平台 action 向量与 Cosine-LSH 索引

   - <xxx>.widgets.vec/.widgets.lsh：外部平台 widget 向量与 Cosine-LSH 索引

## 运行时加载与匹配（WidgetReusableAgent）

1. 设备端禁用运行时索引/向量构建（开关 _enableRuntimeIndexBuild=false，避免 BERT/CLIP 推理与建索引耗时）

2. addExternalPlatformModel：

   - 自动探测并加载外部平台 <xxx>.actions.vec/.lsh 到 platformData.actionEmbeddings/actionsLSH

   - 自动探测并加载外部平台 <xxx>.widgets.vec/.lsh 到 platformData.widgetEmbeddings/widgetsLSH

3. findSimilarActionInExternalModels：

   - 进入findSimilarActionInExternalModels前先查询缓存_externalActionMatchCache

   - 当前 action 嵌入只计算一次并缓存（_actionEmbeddingCache）

   - 先用 actionsLSH 取候选（candidate=256），再用向量余弦 calculateSimilarityFromVectors 精算

   - 严格类型过滤（strictTypeMatching=true），减少候选规模

   - 命中后填充 ExternalActionMatch 并写入 _externalActionMatchCache

4. probabilityOfVisitingNewWidgetsFromExternalModel：

   - 若加载了 widgetsLSH：对“本地已访问 widget”的嵌入进行 LSH 查询，快速标记“外部 widget 是否已访问”

   - 若 LSH 不可用才回退旧的属性比对路径

   - 继续使用 _externalWidgetVisitedIndex 做快速命中缓存