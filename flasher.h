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
#include <QJsonArray>
#include <QThread>
#include <QFile>

#include "flashing_info.h"
#include "serial_port.h"

namespace socket {

class SocketClient;

} // namespace socket

namespace file_downloader {

class FileDownloader;

} // namespace file_downloader

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
    kServerDataExchange,
    kBrowseFirmware,
    kCheckBoardInfo,
    kLoadFirmwareFile,
    kDownloadFirmwareFile,
    kFlash,
    kEnterBootloader,
    kEnteringBootloader,
    kReconnect,
    kExitBootloader,
    kExitingBootloader,
    kEnableReadProtection,
    kDisableReadProtection,
    kError
};

class Flasher : public QObject
{
    Q_OBJECT

  public:
    Flasher();
    ~Flasher();

    bool CollectBoardId();
    bool CollectBoardInfo();
    bool Erase();
    FlashingInfo Flash();
    void Init();
    bool IsBootloaderDetected() const;
    bool IsReadProtectionEnabled() const;
    bool OpenFirmwareFile(const QString& file_path);
    bool SendEnterBootloaderCommand();
    void SendFlashCommand();
    void SetState(const FlasherStates& state);
    void SetSelectedFirmwareVersion(const QString& selected_frimware_version);
    void TryToConnectConsole();

  signals:
    void UpdateProgress(const qint64& sent_size, const qint64& firmware_size);
    void ClearProgress();
    void ShowStatusMsg(const QString& text);
    void FailedToConnect();
    void RunLoop();
    void ShowTextInBrowser(const QString& text);
    void ClearTextInBrowser();
    void SetButtons(const bool& isBootloader);
    void SetReadProtectionButtonText(const bool& isEnabled);
    void DisableAllButtons();
    void EnableLoadButton();
    void SetFirmwareList(const QJsonArray& product_info);

  public slots:
    void LoopHandler();
    void FileDownloaded();
    void DownloadProgress(qint64& bytes_received, qint64& bytes_total);

  private:
    QString board_id_;
    QJsonObject board_info_;
    QJsonObject bl_version_;
    QJsonObject fw_version_;
    QJsonArray product_info_;
    QString selected_frimware_version_;
    QFile config_file_;
    QFile firmware_file_;
    bool is_bootloader_ {false};
    bool is_bootloader_expected_ {false};
    bool is_read_protection_enabled_ {false};
    bool is_timer_started_ {false};
    bool is_firmware_downloaded_{false};
    QByteArray file_content_;
    communication::SerialPort serial_port_;
    std::shared_ptr<socket::SocketClient> socket_client_;
    file_downloader::FileDownloader *file_downloader_;
    FlasherStates state_ {FlasherStates::kIdle};
    QElapsedTimer timer_;
    QThread worker_thread_;

    bool CheckAck();
    bool CheckTrue();
    bool CrcCheck(const uint8_t *data, const uint32_t size);
    void GetVersion();
    bool GetVersionJson(QJsonObject& out_json_object);
    bool IsFirmwareProtected();
    void ReconnectingToBoard();
    bool SendMessage(const char *data, qint64 length, int timeout_ms);
    bool ReadMessageWithCrc(const char *in_data, qint64 length, int timeout_ms, QByteArray& out_data);
    void TryToConnect();
    void DownloadFirmwareFromUrl();
    bool OpenConfigFile(QJsonDocument& json_document);
    void CreateDefaultConfigFile();
};

} // namespace flasher
#endif // FLASHER_H_
