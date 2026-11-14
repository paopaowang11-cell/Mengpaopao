#-------------------------------------------------
#
# Project created by QtCreator 2026-10-27T10:14:05
#
#-------------------------------------------------

QT       += core gui serialport sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qt_opencv_project
TEMPLATE = app


SOURCES += main.cpp\
        dialog.cpp \
    camera_worker.cpp \
    userdao.cpp \
    managerfactory.cpp \
    recongnizer.cpp

HEADERS  += dialog.h \
    camera_worker.h \
    userdao.h \
    managerfactory.h \
    recongnizer.h

INCLUDEPATH += E:/study/soft/opencv/opencv3.4-install/install/include
INCLUDEPATH += E:/study/soft/opencv/opencv3.4-install/install/include/opencv
INCLUDEPATH += E:/study/soft/opencv/opencv3.4-install/install/include/opencv2
LIBS += E:/study/soft/opencv/opencv3.4-install/install/x86/mingw/lib/libopencv_*.a
