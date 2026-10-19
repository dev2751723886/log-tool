QT += core gui widgets
TARGET = LogInspector
TEMPLATE = app
CONFIG += c++14 console
DEFINES += QT_DEPRECATED_WARNINGS
INCLUDEPATH += src
LIBS += -lz

SOURCES += \
    src/main.cpp \
    src/core/gzipreader.cpp \
    src/core/logscanner.cpp \
    src/core/logparser.cpp \
    src/core/timeline.cpp \
    src/analyze/knowledgebase.cpp \
    src/analyze/ruleengine.cpp \
    src/store/reportwriter.cpp \
    src/ui/mainwindow.cpp \
    src/ui/timelineview.cpp \
    src/ui/trajectoryview.cpp \
    src/ui/sensorcurveview.cpp \
    src/ui/logtableview.cpp \
    src/ui/comparedialog.cpp

HEADERS += \
    src/core/logentry.h \
    src/core/gzipreader.h \
    src/core/logscanner.h \
    src/core/logparser.h \
    src/core/timeline.h \
    src/analyze/anomaly.h \
    src/analyze/knowledgebase.h \
    src/analyze/ruleengine.h \
    src/store/reportwriter.h \
    src/ui/theme.h \
    src/ui/mainwindow.h \
    src/ui/timelineview.h \
    src/ui/trajectoryview.h \
    src/ui/sensorcurveview.h \
    src/ui/logtableview.h \
    src/ui/comparedialog.h
