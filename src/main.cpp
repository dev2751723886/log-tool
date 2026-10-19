#include <QApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QStringList>
#include <cstdio>
#include <utility>

#include "ui/mainwindow.h"
#include "core/logscanner.h"
#include "core/gzipreader.h"
#include "core/logparser.h"
#include "core/timeline.h"
#include "analyze/ruleengine.h"
#include "store/reportwriter.h"

// 无界面模式：用于命令行验证引擎（LogInspector --cli <日志目录>）
static int runCli(const QString& rootDir) {
    QElapsedTimer timer;
    timer.start();
    auto files = LogScanner::scan(rootDir);
    printf("扫描到 %d 个日志文件\n", (int)files.size());

    Timeline tl;
    int parsed = 0;
    LogParser::resetParseStats();
    for (const LogFile& lf : files) {
        QStringList lines;
        if (!GzipReader::readAllLines(lf.path, lines)) {
            printf("读取失败: %s\n", qPrintable(lf.path));
            continue;
        }
        for (int li = 0; li < lines.size(); ++li) {
            LogEntry e;
            if (LogParser::parse(lines[li], lf.path, lf.type, e)) {
                e.lineNumber = li + 1;
                tl.append(std::move(e));
                parsed++;
            }
        }
    }
    printf("解析条目：%d\n", parsed);
    tl.buildIndex();
    printf("时间范围：%s → %s\n",
           qPrintable(QDateTime::fromMSecsSinceEpoch(tl.minTime()).toString("yyyy-MM-dd HH:mm:ss")),
           qPrintable(QDateTime::fromMSecsSinceEpoch(tl.maxTime()).toString("yyyy-MM-dd HH:mm:ss")));

    RuleEngine engine;
    QVector<Anomaly> anomalies = engine.analyze(tl);
    printf("%s\n", qPrintable(ReportWriter::summary(anomalies)));

    printf("\n=== 运行汇总 ===\n");
    for (const RunSummary& r : engine.runs()) {
        printf("  %s [%s] err=%d warn=%d %s\n", qPrintable(r.runId), qPrintable(r.verdict),
               r.errorCount, r.warnCount, qPrintable(r.failReason));
    }
    printf("\n=== 约束检查（不满足项）===\n");
    for (const ConstraintCheck& c : engine.constraints()) {
        printf("  %s | expected=%s actual=%s\n", qPrintable(c.runId),
               qPrintable(c.expected), qPrintable(c.actual));
    }
    for (int i = 0; i < anomalies.size() && i < 60; ++i) {
        const Anomaly& a = anomalies[i];
        printf("\n[%d] %s | %s | %s | %s\n", i + 1, qPrintable(a.timeStr),
               qPrintable(severityName(a.severity)), qPrintable(issueTypeName(a.type)),
               qPrintable(a.title));
        printf("    原因: %s\n", qPrintable(a.causes.join("；")));
        printf("    解决: %s\n", qPrintable(a.solutions.join("；")));
    }
    QString out = rootDir + "/分析报告.json";
    if (ReportWriter::save(anomalies, out))
        printf("\n报告已保存: %s\n", qPrintable(out));
    printf("耗时 %lld ms\n", timer.elapsed());
    return 0;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QStringList args = app.arguments();
    if (args.contains("--cli")) {
        int idx = args.indexOf("--cli");
        if (idx + 1 >= args.size()) {
            printf("用法: LogInspector --cli <日志目录>\n");
            return 1;
        }
        return runCli(args[idx + 1]);
    }
    MainWindow w;
    w.show();
    return app.exec();
}
