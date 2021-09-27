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

#include <QElapsedTimer>
#include <QFile>
#include <QJsonObject>
#include <QThread>

#include "socket.h"

namespace communication {
class SerialPort;
} // namespace communication

QT_BEGIN_NAMESPACE
class Worker : public QObject
{
    Q_OBJECT

public slots:
    void doWork();

signals:
    void flasherLoop();
};
QT_END_NAMESPACE

namespace flasher {

enum class FlasherStates {
    kIdle,
    kInit,
    kTryToConnect,
    kConnected,
    kDisconnected,
    kBoardId,
    kGetBoardIdKey,
    kBoardCheckRegistration,
    kOpenFile,
    kFlash
};

class Flasher : public QObject
{
    Q_OBJECT

public:
    Flasher();
    ~Flasher();

    void init();
    std::shared_ptr<communication::SerialPort> getSerialPort() const;
    bool collectBoardId();
    bool getBoardKey();
    void saveBoardKeyToFile();
    bool getBoardKeyFromServer();
    bool sendKey();
    bool startErase();
    std::tuple<bool, QString, QString> startFlash();
    bool crcCheck(const uint8_t* data, uint32_t size);
    bool checkAck();
    bool checkTrue();
    bool openFirmwareFile(const QString& filePath);
    void openSerialPort();
    void closeSerialPort();
    void reopenSerialPort();
    void setFilePath(const QString& filePath);
    void setState(const FlasherStates& state);
    bool SendEnterBootloaderCommand();
    bool SendExitBootloaderCommand();
    void sendFlashCommand(void); //enter in bootlaoder, can't exit without FW flash
    bool checkIfFirmwareIsProtected(void);
    bool sendEnableFirmwareProtection(void);
    bool sendDisableFirmwareProtection(void);
    void getVersion(void);
    QThread& getWorkerThread();

signals:
    void updateProgress(const qint64& dataPosition, const qint64& firmwareSize);
    void clearProgress();
    void showStatusMsg(const QString& text);
    void failedToConnect();
    void openSerialPortInThread();
    void closeSerialPortInThread();
    void runLoop();
    void textInBrowser(const QString& boardId);
    void isBootloader(const bool& bootloader);
    void isReadProtectionEnabled(const bool& enabled);
    void readyToFlashId();

public slots:
    void loopHandler();

private:
    QThread workerThread;
    std::shared_ptr<communication::SerialPort> m_serialPort;
    QFile m_fileFirmware;
    std::shared_ptr<QFile> m_keysFile;
    communication::SocketClient m_socketClient;

    FlasherStates m_state {FlasherStates::kInit};
    qint64 m_firmwareSize {0};
    bool m_tryOpen {false};
    bool m_isPortOpen {false};
    bool m_isSecureBoot {true};
    QString m_boardId;
    QString m_boardKey;
    QJsonObject m_jsonObject;
    QString m_filePath;
    bool m_isTryConnectStart {false};
    QElapsedTimer m_timerTryConnect;

    bool sendDisconnectCmd();
};

} // namespace flasher
#endif // FLASHER_H
