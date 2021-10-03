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
    kTryToConnect,
    kConnected,
    kDisconnected,
    kRegisterBoard,
    kSelectFirmware,
    kCheckBoardInfo,
    kFlash,
    kEnterBootloader,
    kExitBootloader,
    kReconnect,
    kError
};

class Flasher : public QObject
{
    Q_OBJECT

public:
    Flasher();
    ~Flasher();

    void init();
    std::shared_ptr<communication::SerialPort> getSerialPort() const;
    bool startErase();
    std::tuple<bool, QString, QString> startFlash();
    bool crcCheck(const uint8_t* data, uint32_t size);
    bool checkAck();
    bool checkTrue();
    bool CollectBoardId();
    bool GetBoardKey();
    bool OpenFirmwareFile(const QString& filePath);
    void SendFlashCommand();
    void SetState(const FlasherStates& state);
    bool sendEnableFirmwareProtection(void);
    bool sendDisableFirmwareProtection(void);

signals:
    void updateProgress(const qint64& dataPosition, const qint64& firmwareSize);
    void clearProgress();
    void showStatusMsg(const QString& text);
    void failedToConnect();
    void runLoop();
    void textInBrowser(const QString& boardId);
    void isBootloader(const bool& bootloader);
    void isReadProtectionEnabled(const bool& enabled);
    void enableLoadButton();

public slots:
    void loopHandler();

private:
    QThread workerThread;
    std::shared_ptr<communication::SerialPort> m_serialPort;
    QFile m_fileFirmware;
    QFile m_keysFile;
    communication::SocketClient m_socketClient;

    FlasherStates m_state {FlasherStates::kIdle};
    bool m_isSecureBoot {true};
    QString m_boardId;
    QString m_boardKey;
    QJsonObject m_jsonObject;
    bool m_isTryConnectStart {false};
    QElapsedTimer m_timerTryConnect;

    bool GetBoardKeyFromServer();
    void GetVersion();
    bool IsFirmwareProtected();
    void SaveBoardKeyToFile();
    bool SendDisconnectCmd();
    bool SendEnterBootloaderCommand();
    bool SendExitBootloaderCommand();
    bool SendKey();
};

} // namespace flasher
#endif // FLASHER_H
