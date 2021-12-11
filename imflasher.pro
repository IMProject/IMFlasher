QT += widgets serialport network
requires(qtConfig(combobox))

TARGET = imflasher
TEMPLATE = app

SOURCES += \
    crc32.cpp \
    flasher.cpp \
    main.cpp \
    mainwindow.cpp \
    serial_port.cpp \
    socket_client.cpp

HEADERS += \
    crc32.h \
    flasher.h \
    flashing_info.h \
    mainwindow.h \
    serial_port.h \
    socket_client.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    imflasher.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/serialport/imflasher
INSTALLS += target
