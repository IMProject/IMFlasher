QT += widgets serialport network
requires(qtConfig(combobox))

TARGET = imflasher
TEMPLATE = app

GIT_TAG = $$system(git --git-dir $$PWD/.git --work-tree $$PWD describe --always --tags)
GIT_TAG ~= s/-.*//
GIT_HASH = $$system(git --git-dir $$PWD/.git --work-tree $$PWD rev-parse --verify HEAD)
GIT_BRANCH = $$system(git --git-dir $$PWD/.git --work-tree $$PWD rev-parse --abbrev-ref HEAD)

DEFINES += GIT_TAG=\\\"$$GIT_TAG\\\"
DEFINES += GIT_HASH=\\\"$$GIT_HASH\\\"
DEFINES += GIT_BRANCH=\\\"$$GIT_BRANCH\\\"
SOURCES += \
    crc32.cpp \
    file_downloader.cpp \
    flasher.cpp \
    main.cpp \
    mainwindow.cpp \
    serial_port.cpp \
    socket_client.cpp \
    worker.cpp

HEADERS += \
    crc32.h \
    file_downloader.h \
    flasher.h \
    flasher_states.h \
    flashing_info.h \
    mainwindow.h \
    serial_port.h \
    socket_client.h \
    worker.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    imflasher.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/serialport/imflasher
INSTALLS += target
