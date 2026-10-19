#include "analyze/knowledgebase.h"

void KnowledgeBase::apply(Anomaly& a) {
    if (a.causes.isEmpty()) {
        switch (a.type) {
        case IssueType::Crash:
            a.causes << "进程收到致命信号（段错误 / 断言失败 / 未捕获异常）"
                     << "内存越界或空指针解引用"
                     << "资源耗尽（内存 / 句柄 / 栈溢出）";
            break;
        case IssueType::NodeExecutionFailure:
            a.causes << "前置条件未满足（定位未生效 / 传感器未就绪）"
                     << "依赖的外部服务或 IO 未就绪"
                     << "参数缺失或配置错误"
                     << "重试次数耗尽";
            break;
        case IssueType::SensorDataLoss:
            a.causes << "传感器 / 下位机通信中断"
                     << "数据被过滤或置信度不足"
                     << "驱动未正常初始化"
                     << "采样率异常";
            break;
        case IssueType::ChassisControlTimeout:
            a.causes << "底盘控制器未进入运行态"
                     << "急停或安全互锁触发"
                     << "运动指令未下发或未响应"
                     << "电机驱动报警";
            break;
        case IssueType::InterfaceError:
            a.causes << "参数服务返回缺失 / 错误数据"
                     << "能力项未配置"
                     << "协议字段不匹配";
            break;
        case IssueType::CrossValidationMismatch:
            a.causes << "控制流程已完成但执行器无实际动作（指令未达下位机 / 电机未使能）"
                     << "反馈链路未上报"
                     << "状态同步延迟";
            break;
        case IssueType::NumericalError:
            a.causes << "求解过程出现 NaN/Inf，数值不稳定"
                     << "步长过大导致数值发散"
                     << "模型存在除零或负值开方等非法运算"
                     << "状态变量突变";
            break;
        case IssueType::NonConvergence:
            a.causes << "迭代次数达到上限仍未满足容差"
                     << "残差震荡或增大"
                     << "初始值不当"
                     << "模型非线性强 / 病态";
            break;
        case IssueType::DataInconsistency:
            a.causes << "模块间数据耦合不同步"
                     << "状态发布与接收字段不一致"
                     << "数值传递精度损失";
            break;
        case IssueType::PerformanceTimeout:
            a.causes << "阶段耗时超过阈值"
                     << "求解 / 预处理慢"
                     << "重试耗尽";
            break;
        case IssueType::ConfigError:
            a.causes << "参数越界"
                     << "依赖文件缺失"
                     << "配置值无法识别";
            break;
        case IssueType::RegressionDeviation:
            a.causes << "代码 / 配置 / 求解器改动引入偏差"
                     << "指标超出容差";
            break;
        case IssueType::TimestampAnomaly:
            a.causes << "多线程日志缓冲导致乱序"
                     << "时间戳生成错误"
                     << "重复事件";
            break;
        case IssueType::ParseError:
            a.causes << "日志格式不规范"
                     << "字段缺失"
                     << "时间戳格式不一致";
            break;
        default: break;
        }
    }
    if (a.solutions.isEmpty()) {
        switch (a.type) {
        case IssueType::Crash:
            a.solutions << "查看崩溃前最后操作定位触发点"
                        << "用调试器 / 核心转储回溯调用栈"
                        << "检查崩溃节点前的内存与空指针防御"
                        << "修复后回归验证";
            break;
        case IssueType::NodeExecutionFailure:
            a.solutions << "沿时间轴查看该节点 RUNNING→FAILURE 前的状态变化"
                        << "检查其前置节点与依赖数据"
                        << "对照参数服务确认配置项存在"
                        << "必要时增加重试或降级逻辑";
            break;
        case IssueType::SensorDataLoss:
            a.solutions << "检查 MCU 反馈帧间隔是否出现空洞"
                        << "确认传感器零偏 / 置信度状态"
                        << "排查串口 / CAN 通信链路"
                        << "恢复数据后确认定位置信度达标";
            break;
        case IssueType::ChassisControlTimeout:
            a.solutions << "查看控制状态（controller state / motion_state）是否卡在非运行态"
                        << "检查急停 / 限位 IO"
                        << "核对电机驱动 error_list 与转速 / 电流反馈"
                        << "确认速度指令与执行一致性";
            break;
        case IssueType::InterfaceError:
            a.solutions << "依据错误码定位对应接口"
                        << "补齐缺失参数或能力配置"
                        << "核对接口协议版本与字段";
            break;
        case IssueType::CrossValidationMismatch:
            a.solutions << "在同一时间轴上对齐控制指令与电机反馈，确认是“指令未下发”还是“执行未反馈”"
                        << "检查使能 / 上电顺序"
                        << "核对反馈上报频率与地址";
            break;
        case IssueType::NumericalError:
            a.solutions << "定位 NaN/Inf 首次出现的变量与时间"
                        << "减小步长或改用更稳的积分方法"
                        << "检查模型方程是否存在奇点 / 除零"
                        << "加数值保护或限幅";
            break;
        case IssueType::NonConvergence:
            a.solutions << "增大最大迭代次数或放宽容差"
                        << "改进初始值"
                        << "检查残差趋势判断是否发散"
                        << "更换求解方法或预处理";
            break;
        case IssueType::DataInconsistency:
            a.solutions << "按 run_id / step_id 关联比对发布与接收字段"
                        << "检查耦合步序"
                        << "统一字段语义与单位";
            break;
        case IssueType::PerformanceTimeout:
            a.solutions << "定位耗时最长的阶段"
                        << "优化热点代码"
                        << "调整超时阈值或并行化";
            break;
        case IssueType::ConfigError:
            a.solutions << "校验参数约束"
                        << "补全缺失文件"
                        << "修正配置值";
            break;
        case IssueType::RegressionDeviation:
            a.solutions << "对比改动前后的指标"
                        << "检查是否超过容差"
                        << "回归定位引入偏差的改动";
            break;
        case IssueType::TimestampAnomaly:
            a.solutions << "检查日志时间戳生成逻辑"
                        << "确认乱序是否在容忍范围内"
                        << "对重复事件去重";
            break;
        case IssueType::ParseError:
            a.solutions << "修正日志输出格式"
                        << "增强解析容错";
            break;
        default: break;
        }
    }
}
