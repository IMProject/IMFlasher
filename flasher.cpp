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

#include "flasher.h"

#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QMessageBox>

#include "crc32.h"
#include "socket_client.h"
#include "file_downloader.h"
#include "worker.h"

namespace flasher {
namespace {

constexpr qint64 kSignatureSize {64};
constexpr int kEraseTimeoutInMs {5000};
constexpr qint64 kPacketSize {256};
constexpr qint64 kSecurePacketSize {296};
constexpr unsigned long kThreadSleepTimeInMs {100U};
constexpr int kSerialTimeoutInMs {100};
constexpr int kCollectDataTimeoutInMs {300};
constexpr int kCrc32Size {4};
constexpr int kBoardIdSize {32};
constexpr int kTryToConnectTimeoutInMs {20000};
constexpr int kTryToDownloadFileTimeoutInMs {5000};

// Commands
constexpr char kVerifyFlasherCmd[] = "IMFlasher_Verify";
constexpr char kEraseCmd[] = "erase";
constexpr char kVersionCmd[] = "version";
constexpr char kSoftwareInfoJsonCmd[] = "software_info_json";
constexpr char kSecurityJsonCmd[] = "security_json";
constexpr char kBoardIdCmd[] = "board_id";
constexpr char kBoardInfoJsonCmd[] = "board_info_json";
constexpr char kFlashFwCmd[] = "flash_fw";
constexpr char kEnterBlCmd[] = "enter_bl";
constexpr char kIsFwProtectedCmd[] = "is_fw_protected";
constexpr char kEnableFwProtectionCmd[] = "enable_fw_protection";
constexpr char kDisableFwProtectionCmd[] = "disable_fw_protection";
constexpr char kExitBlCmd[] = "exit_bl";
constexpr char kCheckSignatureCmd[] = "check_signature";
constexpr char kDisconnectCmd[] = "disconnect";

constexpr char kFakeBoardIdBase64[] = "Tk9UX1NFQ1VSRURfTUFHSUNfU1RSSU5HXzEyMzQ1Njc="; // NOT_SECURED_MAGIC_STRING_1234567

// Config
constexpr char kConfigFileName[] = "config.json";
constexpr uint32_t kConfigOpenAttempt = 2;
constexpr char kConfigVersionStr[] = "config_version";
constexpr char kEnableSignatureWarningStr[] = "enable_signature_warning";

// Servers default config
constexpr char kDefaultServerAddress1[] = "server1.imtech.hr";
constexpr char kDefaultServerAddress2[] = "server2.imtech.hr";
constexpr char kDefaultServerAddress3[] = "server3.imtech.hr";
constexpr int kDefaultPort = 5322;
constexpr char kDefaultKey[] = "NDQ4N2Y1YjFhZTg3ZGI3MTA1MjlhYmM3";

uint32_t Deserialize32(const uint8_t *buf) {
    uint32_t result;
    result = static_cast<uint32_t>(buf[0] << 24U);
    result |= static_cast<uint32_t>(buf[1] << 16U);
    result |= static_cast<uint32_t>(buf[2] << 8U);
    result |= static_cast<uint32_t>(buf[3] << 0U);
    return result;
}

bool ShowInfoMsg(const QString& title, const QString& description) {
    QMessageBox msg_box;
    msg_box.setText(title);
    msg_box.setInformativeText(description);
    msg_box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    return (msg_box.exec() == QMessageBox::Ok);
}

} // namespace

Flasher::Flasher() = default;

Flasher::~Flasher() {
    worker_thread_.quit();
    worker_thread_.wait();
}

void Flasher::Init() {
    QJsonDocument json_document;

    if (OpenConfigFile(json_document)) {
        QJsonArray servers_array = json_document.object().find("servers")->toArray();
        socket_client_ = std::make_shared<socket::SocketClient>(std::move(servers_array));

        if (0 == QString::compare("true", json_document.object().find(kEnableSignatureWarningStr)->toString(), Qt::CaseInsensitive)) {
            is_signature_warning_enabled_ = true;
        }
    }

    file_downloader_ = std::make_unique<file_downloader::FileDownloader>();
    connect(file_downloader_.get(), &file_downloader::FileDownloader::Downloaded, this, &Flasher::FileDownloaded);
    connect(file_downloader_.get(), &file_downloader::FileDownloader::DownloadProgress, this, &Flasher::DownloadProgress);
    connect(socket_client_.get(), &socket::SocketClient::DownloadProgress, this, &Flasher::DownloadProgress);

    connect(&serial_port_, &QSerialPort::errorOccurred, this, &Flasher::HandleSerialPortError);

    Worker *worker = new Worker;
    worker->moveToThread(&worker_thread_);
    connect(&worker_thread_, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &Flasher::RunLoop, worker, &Worker::DoWork);
    connect(worker, &Worker::FlasherLoop, this, &Flasher::LoopHandler);
    worker_thread_.start();
    emit RunLoop();
}

void Flasher::FileDownloaded() {
    is_download_success_ = file_downloader_->GetDownloadedData(file_content_);
    is_file_downloaded_ = true;
}

void Flasher::UpdateProgressBar(const quint64& sent_size, const quint64& total_size) {
    int progress_percentage = 0;
    if (total_size != 0) {
        progress_percentage = (100 * sent_size) / total_size;
    }

    if (last_progress_percentage_ != progress_percentage) {
        last_progress_percentage_ = progress_percentage;
        qInfo() << sent_size << "/" << total_size << "B, " << progress_percentage << "%";
        emit UpdateProgressBarSignal(progress_percentage);
    }
}

void Flasher::DownloadProgress(const qint64& bytes_received, const qint64& bytes_total) {
    UpdateProgressBar(bytes_received, bytes_total);
}

void Flasher::LoopHandler() {
    switch (state_) {

        case FlasherStates::kIdle:
            // Idle state
            break;

        case FlasherStates::kTryToConnect:
            TryToConnect();
            break;

        case FlasherStates::kConnected:
            emit ClearTextInBrowser();
            emit ShowStatusMsg("Connected");
            if (serial_port_.isOpen()) {
                emit SetButtons(is_bootloader_);

                if (is_bootloader_) {
                    GetVersionJson(bl_version_);
                    if (bl_version_.empty()) {
                        GetVersion();
                    } else {
                        if ("secured" == bl_version_.value("build_variant").toString()) {
                            emit DisableBrowseFileButton();
                        }
                    }
                    SetState(FlasherStates::kCheckBoardInfo);
                } else {
                    GetVersionJson(fw_version_);
                    if (fw_version_.empty()) {
                        GetVersion();
                    }
                    SetState(FlasherStates::kIdle);
                }
            }
            break;

        case FlasherStates::kDisconnected: {
            bool is_disconnected_success = true;
            is_timer_started_ = false;

            if (serial_port_.isOpen()) {
                if (is_bootloader_) {
                    qInfo() << "Send disconnect command";
                    is_disconnected_success = SendMessage(kDisconnectCmd, sizeof(kDisconnectCmd), kSerialTimeoutInMs);
                }
                serial_port_.CloseConn();
            }

            if (is_disconnected_success) {
                emit ShowStatusMsg("Disconnected");
            } else {
                emit ShowStatusMsg("Unplug board!");
            }

            SetState(FlasherStates::kIdle);

            break;
        }

        case FlasherStates::kCheckBoardInfo:
            if (CollectBoardInfo() || CollectBoardId()) {
                emit ShowTextInBrowser("Board ID: " + board_id_);
                is_read_protection_enabled_ = IsFirmwareProtected();
                emit SetReadProtectionButtonText(is_read_protection_enabled_);

                if (0 == QString::compare(kFakeBoardIdBase64, board_id_, Qt::CaseInsensitive)) {
                    SetState(FlasherStates::kIdle);
                } else {
                    SetState(FlasherStates::kServerDataExchange);
                }
            } else {
                emit ShowTextInBrowser("Board ID Error. Unplug your board, press disconnect/connect, and plug your board again.");
                SetState(FlasherStates::kError);
            }

            break;

        case FlasherStates::kServerDataExchange:
            if (!board_info_.empty() && socket_client_) {
                if (socket_client_->SendBoardInfo(board_info_, bl_version_, fw_version_)) {
                    if (socket_client_->ReceiveProductInfo(board_info_, product_info_)) {
                        if (!product_info_.empty()) {
                            emit SetFileVersionsList(product_info_);
                        }
                    }
                }
            }

            SetState(FlasherStates::kIdle);
            break;

        case FlasherStates::kBrowseFile: {
            QString file_path = QFileDialog::getOpenFileName(nullptr,
                                                             tr("File binary"),
                                                             "",
                                                             tr("Binary (*.bin);;All Files (*)"));

            if (!file_path.isEmpty()) {
                if (OpenFile(file_path)) {
                    emit ShowTextInBrowser("File: " + file_path);
                    emit EnableLoadButton();
                }
            }

            SetState(FlasherStates::kIdle);
            break;
        }

        case FlasherStates::kLoadFile: {
            packet_size_ = kPacketSize;
            if (SetLocalFileContent()) {
                // Local file
                SetState(FlasherStates::kCheckSignature);

            } else {

                if (file_source_ == "url") {
                    DownloadFileFromUrl();
                    emit ShowStatusMsg("Downloading");
                    timer_.start();
                    SetState(FlasherStates::kDownloadFileFromUrl);

                } else if (file_source_ == "server") {
                    // Load file from server
                    emit ShowStatusMsg("Downloading");

                    CollectSecurityDataFromBoard();

                    if (socket_client_->DownloadFile(board_info_, client_security_data_, selected_file_version_, server_security_data_, file_content_)) {
                        if (file_content_.isEmpty()) {
                            emit ClearProgress();
                            emit ShowStatusMsg("Download file error");
                            SetState(FlasherStates::kIdle);
                        } else {

                            if (client_security_data_.empty() || server_security_data_.empty()) {
                                SetState(FlasherStates::kCheckSignature);
                            } else {
                                packet_size_ = kSecurePacketSize;
                                SetState(FlasherStates::kSendServerSecurityData);
                                emit ShowTextInBrowser("Secure connection detected!");
                            }
                        }

                    } else {
                        emit ClearProgress();
                        emit ShowStatusMsg("Download server error");

                        SetState(FlasherStates::kIdle);
                    }

                } else {
                    // Unknown source, go to idle
                    SetState(FlasherStates::kIdle);
                }
            }

            break;
        }

        case FlasherStates::kDownloadFileFromUrl: {
            if (is_file_downloaded_) {
                is_file_downloaded_ = false;
                if (!is_download_success_ || file_content_.isEmpty()) {
                    emit ShowStatusMsg("Download error");
                    SetState(FlasherStates::kIdle);
                } else {
                    SetState(FlasherStates::kCheckSignature);
                }

            } else {

                if (timer_.hasExpired(kTryToDownloadFileTimeoutInMs)) {
                    emit ShowStatusMsg("Download timeout");
                    SetState(FlasherStates::kIdle);
                }
            }

            break;
        }

        case FlasherStates::kSendServerSecurityData: {

            FlashingInfo flashing_info = SendServerSecurityData();
            if (flashing_info.success) {
                SetState(FlasherStates::kCheckSignature);
            } else {
                emit ClearStatusMsg();
                ShowInfoMsg(flashing_info.title, flashing_info.description);
                SetState(FlasherStates::kIdle);
            }
            break;
        }

        case FlasherStates::kCheckSignature: {
            emit ShowStatusMsg("Flashing");
            FlashingInfo flashing_info = CheckSignature();
            if (flashing_info.success) {
                SetState(FlasherStates::kSendSignature);
            } else {
                emit ClearStatusMsg();
                ShowInfoMsg(flashing_info.title, flashing_info.description);
                emit ClearProgress();
                SetState(FlasherStates::kIdle);
            }
            break;
        }

        case FlasherStates::kSendSignature: {
            FlashingInfo flashing_info = SendSignature();
            if (flashing_info.success) {
                SetState(FlasherStates::kVerifyFlasher);
            } else {
                emit ClearStatusMsg();
                ShowInfoMsg(flashing_info.title, flashing_info.description);
                emit ClearProgress();
                SetState(FlasherStates::kIdle);
            }
            break;
        }

        case FlasherStates::kVerifyFlasher: {
            FlashingInfo flashing_info = VerifyFlasher();
            if (flashing_info.success) {
                SetState(FlasherStates::kSendFileSize);
            } else {
                emit ClearStatusMsg();
                ShowInfoMsg(flashing_info.title, flashing_info.description);
                emit ClearProgress();
                SetState(FlasherStates::kIdle);
            }
            break;
        }

        case FlasherStates::kSendFileSize: {
            FlashingInfo flashing_info = SendFileSize();
            if (flashing_info.success) {
                SetState(FlasherStates::kErase);
            } else {
                emit ClearStatusMsg();
                ShowInfoMsg(flashing_info.title, flashing_info.description);
                emit ClearProgress();
                SetState(FlasherStates::kIdle);
            }
            break;
        }

        case FlasherStates::kErase: {
            FlashingInfo flashing_info = Erase();
            if (flashing_info.success) {
                SetState(FlasherStates::kFlash);
            } else {
                emit ClearStatusMsg();
                ShowInfoMsg(flashing_info.title, flashing_info.description);
                emit ClearProgress();
                SetState(FlasherStates::kIdle);
            }
            break;
        }

        case FlasherStates::kFlash: {
            FlashingInfo flashing_info = Flash();
            if (flashing_info.success) {
                SetState(FlasherStates::kCheckCrc);
            } else {
                emit ClearStatusMsg();
                ShowInfoMsg(flashing_info.title, flashing_info.description);
                emit ClearProgress();
                SetState(FlasherStates::kIdle);
            }
            break;
        }

        case FlasherStates::kCheckCrc: {
            FlashingInfo flashing_info = CrcCheck();
            ShowInfoMsg(flashing_info.title, flashing_info.description);
            emit ClearProgress();

            if (flashing_info.success) {
                SetState(FlasherStates::kTryToConnect);
            } else {
                emit ClearStatusMsg();
                SetState(FlasherStates::kIdle);
            }
            break;
        }

        case FlasherStates::kEnterBootloader:
            if (!SendEnterBootloaderCommand()) {
                SendFlashCommand();
            }

            is_bootloader_expected_ = true;
            serial_port_.CloseConn();
            SetState(FlasherStates::kEnteringBootloader);
            break;

        case FlasherStates::kEnteringBootloader:
            emit ShowStatusMsg("Entering bootloader...");
            ReconnectingToBoard();
            break;

        case FlasherStates::kExitBootloader:
            qInfo() << "Send exit bootloader command";
            if (SendMessage(kExitBlCmd, sizeof(kExitBlCmd), kSerialTimeoutInMs)) {
                is_bootloader_expected_ = false;
                serial_port_.CloseConn();
                SetState(FlasherStates::kExitingBootloader);
            } else {
                SetState(FlasherStates::kError);
            }

            break;

        case FlasherStates::kExitingBootloader:
            emit ShowStatusMsg("Exiting bootloader...");
            ReconnectingToBoard();
            break;

        case FlasherStates::kReconnect:
            emit DisableAllButtons();
            serial_port_.CloseConn();

            if (!serial_port_.isOpen()) {
                SetState(FlasherStates::kTryToConnect);
            }

            break;

        case FlasherStates::kEnableReadProtection:
            qInfo() << "Send enable firmware protected command";
            if (SendMessage(kEnableFwProtectionCmd, sizeof(kEnableFwProtectionCmd), kSerialTimeoutInMs)) {
                ShowInfoMsg("Enable readout protection", "Powercyle the board!");
                SetState(FlasherStates::kReconnect);
            } else {
                SetState(FlasherStates::kError);
            }
            break;

        case FlasherStates::kDisableReadProtection:
            qInfo() << "Send disable firmware protected command";
            if (ShowInfoMsg("Disable read protection", "Once disabled, complete flash will be erased including bootloader!")) {
                if (SendMessage(kDisableFwProtectionCmd, sizeof(kDisableFwProtectionCmd), kSerialTimeoutInMs)) {
                    SetState(FlasherStates::kIdle);
                } else {
                    SetState(FlasherStates::kError);
                }
            } else {
                SetState(FlasherStates::kIdle);
            }
            break;

        case FlasherStates::kError:
            emit ShowStatusMsg("Error");
            emit DisableAllButtons();
            break;

        default:
            qInfo() << "Unspecified state";
            break;
    }

    worker_thread_.msleep(kThreadSleepTimeInMs);
    emit RunLoop();
}

FlashingInfo Flasher::Flash() {
    FlashingInfo flashing_info;
    const qint64 file_size = file_content_.size() - signature_size_;
    const qint64 num_of_packets = (file_size / packet_size_);
    const char *data_file = file_content_.data() + signature_size_;

    // Send file in packages
    for (qint64 packet = 0; packet < num_of_packets; ++packet) {
        const char *data_position = data_file + (packet * packet_size_);
        UpdateProgressBar((packet + 1U) * packet_size_, file_size);
        flashing_info.success = SendMessage(data_position, packet_size_, kSerialTimeoutInMs);

        if (!flashing_info.success) {
            flashing_info.title = "Flashing process failed";
            flashing_info.description = "Problem with flashing";
            break;
        }
    }

    if (flashing_info.success) {
        const qint64 rest_size = file_size % packet_size_;

        if (rest_size > 0) {
            UpdateProgressBar(num_of_packets * packet_size_ + rest_size, file_size);
            flashing_info.success = SendMessage(data_file + (num_of_packets * packet_size_), rest_size, kSerialTimeoutInMs);

            if (!flashing_info.success) {
                flashing_info.title = "Flashing process failed";
                flashing_info.description = "Problem with flashing";
            }
        }
    }

    return flashing_info;
}

FlashingInfo Flasher::CheckSignature() {
    FlashingInfo flashing_info;
    flashing_info.success = SendMessage(kCheckSignatureCmd, sizeof(kCheckSignatureCmd), kSerialTimeoutInMs);

    if (!flashing_info.success) {
        flashing_info.title = "Flashing process failed";
        flashing_info.description = "Check signature problem";
    }

    return flashing_info;
}

bool Flasher::CheckAck() {
    bool success = false;
    QByteArray data;
    serial_port_.ReadData(data);
    if (data.size() >= 2) {
        if (0 == QString::compare("OK", data, Qt::CaseInsensitive)) {
            qInfo() << "ACK";
            success = true;
        } else if (0 == QString::compare("NOK", data, Qt::CaseInsensitive)) {
            qInfo() << "NOK ACK";
        } else {
            qInfo() << "ERROR or TIMEOUT";
        }
    } else {
        qInfo() << "NO ACK";
    }

    return success;
}

bool Flasher::CheckTrue() {
    //TODO: better handling needed. For error false is returned
    bool success = false;
    QByteArray data;
    serial_port_.ReadData(data);

    if (0 == QString::compare("TRUE", data, Qt::CaseInsensitive)) {
        qInfo() << "TRUE";
        success = true;
    } else if (0 == QString::compare("FALSE", data, Qt::CaseInsensitive)) {
        qInfo() << "FALSE";
    } else {
        qInfo() << "ERROR or TIMEOUT";
    }

    return success;
}

FlashingInfo Flasher::CrcCheck() {
    FlashingInfo flashing_info;
    const qint64 file_size = file_content_.size() - signature_size_;
    const char *data_file = file_content_.data() + signature_size_;

    const uint8_t *data = reinterpret_cast<const uint8_t *>(data_file);
    uint32_t crc = crc::CalculateCrc32(data, file_size, false, false);
    QByteArray crc_data;
    crc_data.setNum(crc);

    flashing_info.success = SendMessage(crc_data.data(), crc_data.size(), kSerialTimeoutInMs);

    if (flashing_info.success) {
        flashing_info.title = "Flashing process done";
        flashing_info.description = "Successful flashing process";

    } else {
        flashing_info.title = "Flashing process failed";
        flashing_info.description = "CRC problem";
    }

    file_content_.clear();

    return flashing_info;
}

bool Flasher::CollectBoardId() {
    bool success = false;

    QByteArray out_data;
    if (ReadMessageWithCrc(kBoardIdCmd, sizeof(kBoardIdCmd), kCollectDataTimeoutInMs, out_data)) {

        if (out_data.size() == kBoardIdSize) {
            board_id_ = out_data.toBase64();
            qInfo() << "Board ID: " << board_id_;
            success = true;
        }
    }

    if (!success) {
        qInfo() << "Board id error";
    }

    return success;
}

bool Flasher::CollectBoardInfo() {
    bool success = false;

    QByteArray out_data;
    if (ReadMessageWithCrc(kBoardInfoJsonCmd, sizeof(kBoardInfoJsonCmd), kCollectDataTimeoutInMs, out_data)) {
        QJsonDocument json_document = QJsonDocument::fromJson(QString(out_data).toUtf8());
        board_info_ = json_document.object();
        if (!board_info_.empty()) {
            board_id_ = board_info_.value("board_id").toString();
            qInfo() << "Board ID: " << board_id_;
            qInfo() << "manufacturer ID: " << board_info_.value("manufacturer_id").toString();
            success = true;
        }
    }

    if (!success) {
        qInfo() << "Board info error";
    }

    return success;
}

bool Flasher::CollectSecurityDataFromBoard() {
    bool success = false;

    QByteArray out_data;
    if (ReadMessageWithCrc(kSecurityJsonCmd, sizeof(kSecurityJsonCmd), kCollectDataTimeoutInMs, out_data)) {
        QJsonDocument json_document = QJsonDocument::fromJson(QString(out_data).toUtf8());
        client_security_data_ = json_document.object();
        if (!client_security_data_.empty()) {
            success = true;
        }
    }

    if (!success) {
        qInfo() << "Security data error";
    }

    return success;
}

FlashingInfo Flasher::ConsoleFlash() {
    FlashingInfo flashing_info = CheckSignature();
    if (!flashing_info.success) {
        return flashing_info;
    }

    flashing_info = SendSignature();
    if (!flashing_info.success) {
        return flashing_info;
    }

    flashing_info = VerifyFlasher();
    if (!flashing_info.success) {
        return flashing_info;
    }

    flashing_info = SendFileSize();
    if (!flashing_info.success) {
        return flashing_info;
    }

    flashing_info = Erase();
    if (!flashing_info.success) {
        return flashing_info;
    }

    flashing_info = Flash();
    if (!flashing_info.success) {
        return flashing_info;
    }

    flashing_info = CrcCheck();

    return flashing_info;
}

FlashingInfo Flasher::Erase() {
    FlashingInfo flashing_info;
    flashing_info.success = SendMessage(kEraseCmd, sizeof(kEraseCmd), kEraseTimeoutInMs);

    if (!flashing_info.success) {
        flashing_info.title = "Flashing process failed";
        flashing_info.description = "Erasing problem";
    }

    return flashing_info;
}

void Flasher::GetVersion() {
    serial_port_.write(kVersionCmd, sizeof(kVersionCmd));
    serial_port_.WaitForReadyRead(kSerialTimeoutInMs);
    QByteArray data;
    serial_port_.ReadData(data);
    emit ShowTextInBrowser(data);
}

bool Flasher::GetVersionJson(QJsonObject& out_json_object) {
    bool success = false;
    QByteArray out_data;
    if (ReadMessageWithCrc(kSoftwareInfoJsonCmd, sizeof(kSoftwareInfoJsonCmd), kSerialTimeoutInMs, out_data)) {
        QJsonDocument out_json_document = QJsonDocument::fromJson(QString(out_data).toUtf8());
        out_json_object = out_json_document.object();

        QString software_info = "Git branch: ";
        software_info.append(out_json_object.value("git_branch").toString());
        software_info.append("\nGit hash: ");
        software_info.append(out_json_object.value("git_hash").toString());
        software_info.append("\nGit tag: ");
        software_info.append(out_json_object.value("git_tag").toString());
        software_info.append("\nRunning from: ");
        software_info.append(out_json_object.value("ld_script_variant").toString());
        software_info.append("\nBuild variant: ");
        software_info.append(out_json_object.value("build_variant").toString());
        emit ShowTextInBrowser(software_info);

        success = true;
    }

    return success;
}

void Flasher::HandleSerialPortError(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::ResourceError) {
        qInfo() << "Serial port error";
        serial_port_.CloseConn();
        emit ClearStatusMsg();
        emit DisableAllButtons();
        emit EnableConnectButton();
        SetState(FlasherStates::kIdle);
    }
}

bool Flasher::IsBootloaderDetected() const {
    return is_bootloader_;
}

bool Flasher::IsFirmwareProtected() {
    qInfo() << "Send is firmware protected command";
    serial_port_.write(kIsFwProtectedCmd, sizeof(kIsFwProtectedCmd));
    serial_port_.WaitForReadyRead(kSerialTimeoutInMs);
    return CheckTrue();
}

bool Flasher::IsReadProtectionEnabled() const {
    return is_read_protection_enabled_;
}

bool Flasher::OpenFile(const QString& file_path) {
    file_to_flash_.setFileName(file_path);

    return file_to_flash_.open(QIODevice::ReadOnly);
}

void Flasher::ReconnectingToBoard() {
    if (is_timer_started_) {
        if (serial_port_.TryOpenPort(is_bootloader_)) {
            if (is_bootloader_ == is_bootloader_expected_) {
                SetState(FlasherStates::kConnected);
                is_timer_started_ = false;
            } else {
                serial_port_.CloseConn();
            }
        }

        if (timer_.hasExpired(kTryToConnectTimeoutInMs)) {
            ShowInfoMsg("Error!", "Entering/Exiting bootloader cannot be performed!");
            SetState(FlasherStates::kTryToConnect);
            is_timer_started_ = false;
        }

    } else {
        emit DisableAllButtons();
        is_timer_started_ = true;
        timer_.start();
    }
}

FlashingInfo Flasher::SendFileSize() {
    FlashingInfo flashing_info;
    const qint64 file_size = file_content_.size() - signature_size_;
    QByteArray file_size_bytes;
    file_size_bytes.setNum(file_size);
    flashing_info.success = SendMessage(file_size_bytes.data(), file_size_bytes.size(), kSerialTimeoutInMs);

    if (!flashing_info.success) {
        flashing_info.title = "Flashing process failed";
        flashing_info.description = "Send file size problem";
    }

    return flashing_info;
}

bool Flasher::SendMessage(const char *data, qint64 length, int timeout_ms) {
    serial_port_.write(data, length);
    serial_port_.WaitForReadyRead(timeout_ms);
    return CheckAck();
}

FlashingInfo Flasher::SendServerSecurityData() {
    FlashingInfo flashing_info;

    QByteArray security_data = QJsonDocument(server_security_data_).toJson(QJsonDocument::Compact);
    flashing_info.success = SendMessage(security_data.data(), security_data.size(), kSerialTimeoutInMs);
    security_data.clear();

    if (!flashing_info.success) {
        flashing_info.title = "Flashing process failed";
        flashing_info.description = "Board rejected server security data";
    }

    return flashing_info;
}

FlashingInfo Flasher::SendSignature() {
    FlashingInfo flashing_info;
    signature_size_ = kSignatureSize;

    flashing_info.success = SendMessage(file_content_.data(), kSignatureSize, kSerialTimeoutInMs);
    if (!flashing_info.success) {

        bool continue_without_signature = true;
        if (is_signature_warning_enabled_) {
            continue_without_signature = ShowInfoMsg("No signature detected!", "Flashing without a signature is not safe. Flasher will assume that file is without signature.");
        }

        if (continue_without_signature) {
            signature_size_ = 0;
            flashing_info.success = true;
        }
    }

    if (!flashing_info.success) {
        flashing_info.title = "Flashing process failed";
        flashing_info.description = "Send signature problem";
    }

    return flashing_info;
}

bool Flasher::ReadMessageWithCrc(const char *in_data, qint64 length, int timeout_ms, QByteArray& out_data) {
    bool success = false;

    QElapsedTimer timer;
    timer.start();

    if (serial_port_.isOpen()) {
        serial_port_.write(in_data, length);

        QByteArray data;

        serial_port_.WaitForReadyRead(timeout_ms);
        serial_port_.ReadData(data);
        QByteArray data_crc = data.right(kCrc32Size);

        if (data.size() > kCrc32Size) {

            uint32_t out_data_size = data.size() - kCrc32Size;
            uint32_t crc = Deserialize32(reinterpret_cast<uint8_t *>(data_crc.data()));
            uint32_t calc_crc = crc::CalculateCrc32(reinterpret_cast<uint8_t *>(data.data()), out_data_size, false, false);

            if (calc_crc == crc) {
                out_data = data.left(out_data_size);
                success = true;
            }
        }
    }

    return success;
}

bool Flasher::SendEnterBootloaderCommand() {
    qInfo() << "Send enter bl command";
    return SendMessage(kEnterBlCmd, sizeof(kEnterBlCmd), kSerialTimeoutInMs);
}

void Flasher::SendFlashCommand() {
    qInfo() << "Send flash command";
    serial_port_.write(kFlashFwCmd, sizeof(kFlashFwCmd));
    serial_port_.WaitForReadyRead(kSerialTimeoutInMs);
    // Check ack
}

bool Flasher::SetLocalFileContent() {
    if (file_to_flash_.isOpen()) {
        file_content_ = file_to_flash_.readAll();
        file_to_flash_.close();
        return true;
    }
    return false;
}

void Flasher::SetState(const FlasherStates& state) {
    state_ = state;
}

void Flasher::SetSelectedFileVersion(const QString& selected_file_version) {
    selected_file_version_ = selected_file_version;

    foreach (const QJsonValue& value, product_info_) {
        QJsonObject obj = value.toObject();
        if (obj["file_version"].toString() == selected_file_version_) {
            file_source_ = obj["file_source"].toString();
        }
    }
}

void Flasher::TryToConnectConsole() {
    QElapsedTimer timer;
    timer.start();

    while (!serial_port_.isOpen()) {
        serial_port_.TryOpenPort(is_bootloader_);

        if (timer.hasExpired(kTryToConnectTimeoutInMs)) {
            qInfo() << "Timeout";
            break;
        }
    }
}

void Flasher::TryToConnect() {
    bool is_connected = false;

    if (is_timer_started_) {

        if (serial_port_.TryOpenPort(is_bootloader_)) {
            emit EnableDisconnectButton();
            SetState(FlasherStates::kConnected);
            is_timer_started_ = false;
            is_connected = true;
        } else {
            emit ShowStatusMsg("Trying to connect...");
        }

        if ((!is_connected) && (timer_.hasExpired(kTryToConnectTimeoutInMs))) {
            emit FailedToConnect();
            SetState(FlasherStates::kError);
            is_timer_started_ = false;
        }
    } else {
        emit DisableAllButtons();
        is_timer_started_ = true;
        timer_.start();
    }
}

void Flasher::DownloadFileFromUrl() {
    foreach (const QJsonValue& value, product_info_) {
        QJsonObject obj = value.toObject();
        if (obj["file_version"].toString() == selected_file_version_) {

            QUrl file_url(obj["url"].toString());
            timer_.start();
            file_downloader_->StartDownload(file_url);
            break;
        }
    }
}

bool Flasher::OpenConfigFile(QJsonDocument& json_document) {
    bool success = false;
    config_file_.setFileName(kConfigFileName);

    for (uint32_t i = 0; i < kConfigOpenAttempt; i++) {

        if (!config_file_.exists()) {
            CreateDefaultConfigFile();
        }

        if (config_file_.open(QIODevice::ReadOnly)) {
            QString json_string = config_file_.readAll();
            config_file_.close();
            json_document = QJsonDocument::fromJson(json_string.toUtf8());
            if (!json_document.isEmpty()) {

                // Check if servers config exist
                if (json_document.object().find("servers")->toArray().empty()) {
                    CreateDefaultConfigFile();
                } else {
                    success = true;
                    break;
                }
            }
        }
    }

    return success;
}

void Flasher::CreateDefaultConfigFile() {
    QTextStream stream_config_file(&config_file_);
    if (config_file_.open(QIODevice::WriteOnly)) {

        QJsonDocument json_data;
        QJsonObject json_object;

        QJsonObject json_object_version;
        json_object_version.insert("major", "1");
        json_object_version.insert("minor", "0");
        json_object_version.insert("patch", "0");

        json_object.insert(kConfigVersionStr, json_object_version);
        json_object.insert(kEnableSignatureWarningStr, "true");

        QJsonObject json_object_server_1;
        QJsonObject json_object_server_2;
        QJsonObject json_object_server_3;
        QJsonArray json_array;

        json_object_server_1.insert("address", kDefaultServerAddress1);
        json_object_server_1.insert("port", kDefaultPort);
        json_object_server_1.insert("preshared_key", kDefaultKey);
        json_array.append(json_object_server_1);

        json_object_server_2.insert("address", kDefaultServerAddress2);
        json_object_server_2.insert("port", kDefaultPort);
        json_object_server_2.insert("preshared_key", kDefaultKey);
        json_array.append(json_object_server_2);

        json_object_server_3.insert("address", kDefaultServerAddress3);
        json_object_server_3.insert("port", kDefaultPort);
        json_object_server_3.insert("preshared_key", kDefaultKey);
        json_array.append(json_object_server_3);

        json_object.insert("servers", json_array);

        json_data.setObject(json_object);
        stream_config_file << json_data.toJson();
        config_file_.close();
    }
}

FlashingInfo Flasher::VerifyFlasher() {
    FlashingInfo flashing_info;
    flashing_info.success = SendMessage(kVerifyFlasherCmd, sizeof(kVerifyFlasherCmd), kSerialTimeoutInMs);

    if (!flashing_info.success) {
        flashing_info.title = "Flashing process failed";
        flashing_info.description = "Verify flasher problem";
    }

    return flashing_info;
}

} // namespace flasher
