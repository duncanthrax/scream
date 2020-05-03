#-------------------------------------------------
#
# Project created by QtCreator 2020-04-30T14:20:05
#
#-------------------------------------------------

QT += core gui network multimedia widgets

TARGET = ScreamSender
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += ../Scream/

#LIBS += evr.lib
#LIBS += mf.lib
#LIBS += mfplay.lib
#LIBS += mfreadwrite.lib

LIBS += Mfplat.lib
LIBS += mfuuid.lib
LIBS += ole32.lib

SOURCES += main.cpp\
        MainWindow.cpp \
    ../Scream/ac3encoder.cpp

HEADERS  += MainWindow.h \
    ../Scream/ac3encoder.h \
    ../Scream/rtpheader.h

FORMS    += MainWindow.ui
