# -------------------------------------------------
# Project created by QtCreator 2010-03-15T21:59:31
# -------------------------------------------------
TARGET = ext2explore
TEMPLATE = app
#DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII
QMAKE_LFLAGS += -static-libgcc -static-libstdc++
SOURCES += main.cpp \
    ext2explore.cpp \
    ext2read.cpp \
    platform_win32.c \
    platform_unix.c \
    log.c \
    ext2fs.cpp \
    lvm.cpp \
    ext2copyfile.cpp
HEADERS += platform.h \
    parttypes.h \
    lvm.h \
    ext2read.h \
    ext2fs.h \
    ext2explore.h \
    partition.h \
    ext2copyfile.h
