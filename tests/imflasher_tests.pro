QT += testlib network
QT -= gui

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

GIT_TAG = $$system(git --git-dir $$PWD/.git --work-tree $$PWD describe --always --tags)
GIT_TAG ~= s/-.*//
GIT_HASH = $$system(git --git-dir $$PWD/.git --work-tree $$PWD rev-parse --verify HEAD)
GIT_BRANCH = $$system(git --git-dir $$PWD/.git --work-tree $$PWD rev-parse --abbrev-ref HEAD)

DEFINES += GIT_TAG=\\\"$$GIT_TAG\\\"
DEFINES += GIT_HASH=\\\"$$GIT_HASH\\\"
DEFINES += GIT_BRANCH=\\\"$$GIT_BRANCH\\\"

INCLUDEPATH += ../

SOURCES +=  tst_socket.cpp \
    main.cpp \
    ../socket_client.cpp

HEADERS += \
    tst_socket.h \
    ../socket_client.h

RESOURCES += \
    ../imflasher.qrc
