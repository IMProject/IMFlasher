/****************************************************************************
 *
 *   Copyright (c) 2021 IMProject Development Team. All rights reserved.
 *   Authors: Igor Misic <igy1000mb@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name IMProject nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#ifndef FLASHER_H
#define FLASHER_H

#include "serial_port.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <socket.h>

typedef enum flahser_states
{
    FLASHER_IDLE,
    FLASHER_TRY_CONNECT,
    FLASHER_PLUG_USB,
    FLASHER_CONNECTECTED,
    FLASHER_DISCONNECTECTED,
    FLASHER_BOARD_ID,
    FLASHER_GET_BOARD_ID_KEY,
    FLASHER_BOARD_CHECK_REGISTRATION,       //!< Check if file exist and size of key
    FLASHER_OPEN_FILE,
    FLASHER_FLASH,
    FLASHER_EXIT
}flahser_states_t;

class SerialPort;

class Worker : public QObject
{
    Q_OBJECT

public slots:
    void doWork();

signals:
    void flasherLoop();
};


class Flasher : public QObject {

Q_OBJECT
    QThread serialPortThread;
    QThread workerThread;

public:
    explicit Flasher();
    ~Flasher(){}

    void init();
    void deinit();
    SerialPort* getSerialPort();
    bool collectBoardId();
    bool getBoardKey();
    void saveBoardKeyToFile();
    bool getBoardKeyFromServer();
    bool sendKey();
    bool startErase();
    void startFlash();
    bool crcCheck(const uint8_t* data, uint32_t size);
    bool checkAck();
    void startRegistrationProcedure();
    void openFirmwareFile(QString filePath);
    void openSerialPort();
    void closeSerialPort();
    void actionOpenFirmwareFile();
    void deserialize32(uint8_t* buf, uint32_t* value);
    void setFilePath(const QString& filePath);
    void setFlashWriteAddress(QByteArray setFlashWriteAddress);
    void sendFlashCommandToApp(void); //enter in bootlaoder
    void getVersion(void);
    QThread& getWorkerThread();

signals:
    void updateProgress(qint64 dataPosition, qint64 firmwareSize);
    void connectUsbToPc();
    void connectedSerialPort();
    void disconnectedSerialPort();
    void openSerialPortInThread();
    void closeSerialPortInThread();
    void runLoop();
    void textInBrowser(QString boardId);
    void isBootloader(bool bootloader);
    void readyToFlashId();
    void flashingStatusSignal(QString status);

public slots:
    void loopHandler();
    void startFlashingSlot();

private:

    SerialPort* m_serialPort = nullptr;;
    QFile* m_fileFirmware = nullptr;
    QFile* m_keysFile = nullptr;

    flahser_states_t m_state;
    qint64 m_firmwareSize = 0;
    bool m_tryOpen;
    bool m_isPortOpen;
    bool m_isSecureBoot;
    QString m_boardId;
    QString m_boardKey;
    QJsonObject m_jsonObject;
    QString m_filePath;
    SocketClient* m_socketClient = nullptr;

    QByteArray m_flashWriteAddress;
};

#endif // FLASHER_H
