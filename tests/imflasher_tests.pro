QT += testlib network
QT -= gui

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

DEFINES += GIT_TAG=\\\"1.0.0\\\"
DEFINES += GIT_HASH=\\\"abcdefgh123456789\\\"
DEFINES += GIT_BRANCH=\\\"release\\\"

INCLUDEPATH += ../

SOURCES +=  tst_socket.cpp \
    main.cpp \
    ../socket_client.cpp

HEADERS += \
    tst_socket.h \
    ../socket_client.h

RESOURCES += \
    ../imflasher.qrc
