#pragma once
#include "analyze/anomaly.h"

// 知识库：问题类型 → 候选原因 + 解决建议（离线规则映射）
class KnowledgeBase {
public:
    static void apply(Anomaly& a);
};
