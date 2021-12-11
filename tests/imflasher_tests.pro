QT += testlib network
QT -= gui

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

SOURCES +=  tst_socket.cpp \
    main.cpp \
    ../socket_client.cpp

HEADERS += \
    tst_socket.h \
    ../socket_client.h
