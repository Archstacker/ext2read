# -------------------------------------------------
# Project created by QtCreator 2010-03-15T21:59:31
# -------------------------------------------------
QT     += core
QT     -= gui
TARGET = ext2explore
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app
#DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII
QMAKE_LFLAGS += -static-libgcc -static-libstdc++
SOURCES += main.cpp \
    ext2read.cpp \
    platform_win32.c \
    platform_unix.c \
    log.c \
    ext2fs.cpp \
    lvm.cpp 
HEADERS += platform.h \
    parttypes.h \
    lvm.h \
    ext2read.h \
    ext2fs.h \
    partition.h 
