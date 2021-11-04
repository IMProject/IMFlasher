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

#ifndef FLASHER_H_
#define FLASHER_H_

#include <QElapsedTimer>
#include <QFile>
#include <QJsonObject>
#include <QThread>

#include "serial_port.h"

QT_BEGIN_NAMESPACE
class Worker : public QObject
{
    Q_OBJECT

public slots:
    void DoWork();

signals:
    void FlasherLoop();
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
    kEnteringBootloader,
    kReconnect,
    kExitBootloader,
    kExitingBootloader,
    kError
};

class Flasher : public QObject
{
    Q_OBJECT

public:
    Flasher();
    ~Flasher();

    bool CollectBoardId();
    bool Erase();
    std::tuple<bool, QString, QString> Flash();
    bool GetBoardKey();
    void Init();
    bool IsBootloaderDetected() const;
    bool OpenFirmwareFile(const QString& file_path);
    bool SendEnterBootloaderCommand();
    void SendFlashCommand();
    void SetState(const FlasherStates& state);
    bool SendEnableFirmwareProtection();
    bool SendDisableFirmwareProtection();
    void TryToConnectConsole();

signals:
    void UpdateProgress(const qint64& sent_size, const qint64& firmware_size);
    void ClearProgress();
    void ShowStatusMsg(const QString& text);
    void FailedToConnect();
    void RunLoop();
    void ShowTextInBrowser(const QString &text);
    void SetButtons(const bool& isBootloader);
    void SetReadProtectionButtonText(const bool& isEnabled);
    void DisableAllButtons();
    void EnableLoadButton();
    void EnableRegisterButton();

public slots:
    void LoopHandler();

private:
    QString board_id_;
    QString board_key_;
    QFile firmware_file_;
    bool is_bootloader_ {false};
    bool is_bootloader_expected_ {false};
    bool is_secure_boot_ {true};
    bool is_timer_started_ {false};
    QJsonObject json_object_;
    QFile keys_file_;
    communication::SerialPort serial_port_;
    FlasherStates state_ {FlasherStates::kIdle};
    QElapsedTimer timer_;
    QThread worker_thread_;

    bool CheckAck();
    bool CheckTrue();
    bool CrcCheck(const uint8_t *data, const uint32_t size);
    bool GetBoardKeyFromServer();
    void GetVersion();
    bool IsFirmwareProtected();
    void ReconnectingToBoard();
    void SaveBoardKeyToFile();
    bool SendKey();
    bool SendMessage(const char *data, qint64 length, int timeout_ms);
    void TryToConnect();
};

} // namespace flasher
#endif // FLASHER_H_
