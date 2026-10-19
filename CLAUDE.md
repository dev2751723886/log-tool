# 日志检查系统

穿梭车/AGV 导航栈日志分析工具。扫描日志目录 → 解析 → 统一时间轴 → 规则检测 + 跨文件印证 → 输出问题报告（含前后文追踪、候选原因、解决建议），并支持时间轴/轨迹/传感器曲线可视化。

## 构建（重要）

- 环境：**Qt 5.14.2 + MinGW 7.3.0 64-bit**，位于 `D:\Qt\Qt5.14.2`（编译器 `Tools/mingw730_64`，Qt 套件 `5.14.2/mingw73_64`）。
- **无 CMake、无 QtCharts**。用 qmake + mingw32-make；图表用 QPainter 自绘。
- gzip 解压用 **zlib**（MinGW 自带 `libz.a`/`zlib.h`，`-lz` 静态链接）。

```bash
export PATH="/d/Qt/Qt5.14.2/5.14.2/mingw73_64/bin:/d/Qt/Qt5.14.2/Tools/mingw730_64/bin:$PATH"
qmake LogInspector.pro && mingw32-make -j4
# 产物 release/LogInspector.exe
```

- 中文路径（`D:\日志检查系统`）能正常编译；Git Bash 终端显示乱码是 GBK 编码问题，不影响产物与数据（内部一律 UTF-8）。
- 运行需 Qt DLL 在 PATH（`5.14.2/mingw73_64/bin`）。

## 运行

```bash
./release/LogInspector.exe                  # GUI
./release/LogInspector.exe --cli <日志目录>   # 无界面验证/导出报告
```

## 日志格式（6 类，统一头部 `[时间] [节点] [级别] [文件:行] 消息`）

| 类型 | 目录前缀 | 压缩 | 关键结构化字段（fields） |
|---|---|---|---|
| bt_status | `bt_status_*` | 否 | `bt_node`/`bt_prev`/`bt_cur`（IDLE/RUNNING/SUCCESS/FAILURE） |
| motion_control_chip | `motion_control_*` | gzip | 内嵌 JSON 展平：`chassis.imu.{roll,pitch,yaw}`、`chassis.location.{pos_x,pos_y}`、`chassis.location.odom.vel_x`、`chassis.chassis_model.drive_wheel.motor_driver.{rpm,I}`、`chassis.motion_control.motion_state`、`robot.motion_group.mot_state` |
| nav_default | `nav_default_*` | gzip | `error_code`（如 NAV030025）、`pos_confidence`、`location_state` |
| planner_control | `planner_control_*` | gzip | `x/y/h/v/omega/cs`、`ctrl_state_cur`、`stuck_state` |
| obstacle / obstacle_view | `obstacle_*` | gzip | （obstacle_view 为空文件，扫描时跳过） |
| oa_detect | `oa_detect_*` | gzip | `obstacle_exist`、`oa_controller_state`、`oa_status` |

## 架构

```
src/
├─ core/       阶段1「找信息」：LogScanner(扫描) GzipReader(gz) LogParser(解析+字段提取) Timeline(统一时间轴+索引)
├─ analyze/    阶段3「解释」：RuleEngine(规则+跨文件印证) KnowledgeBase(原因/解决映射) Anomaly(问题模型)
├─ ui/         阶段4「还原」：MainWindow TimelineView(时间轴) TrajectoryView(轨迹) SensorCurveView(曲线) LogTableView(原始日志)
├─ store/      阶段5「闭环」：ReportWriter(JSON 报告保存)
└─ main.cpp    入口（GUI + --cli）
```

数据流：`LogScanner::scan → GzipReader::readAllLines → LogParser::parse(逐行) → Timeline::buildIndex → RuleEngine::analyze → Anomaly[] → ReportWriter::save / UI 展示`。

时间戳统一到 `epochMs`（本地时区），跨文件对齐靠统一时间轴（`Timeline::range` 二分查询时间窗口）。

## 已知待办

- 分析在 GUI 主线程同步执行（约 13s），UI 会短暂卡顿 → 建议移后台线程。

## 已实现（阶段 3/4/5 能力）

- 误报过滤：行为树 FAILURE 按节点名分类，跳过结构/复合节点（Sequence/Fallback/IfThenElse 等）与条件/检查节点（Check/Is/Effective/Exist 等），仅动作节点失败报「节点执行失败」、`RetryUntilSuccessful` 归「底盘超时」。验证后节点失败 1091 → 4。
- 回放：播放/暂停 + 速度倍率 + 进度条 + 时间游标定时推进，同步时间轴/轨迹/传感器曲线/行为树状态视图。
- 报告对比：`ReportWriter::load` + `compare`，`CompareDialog` 展示已解决/新增/持续存在。
