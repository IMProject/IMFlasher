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
#include "socket.h"

QT_BEGIN_NAMESPACE
void Worker::DoWork()
{
    emit FlasherLoop();
}
QT_END_NAMESPACE

namespace flasher {
namespace {

constexpr qint64 kSignatureSize {64};
constexpr int kEraseTimeoutInMs {5000};
constexpr qint64 kPacketSize {256};
constexpr unsigned long kThreadSleepTimeInMs {100U};
constexpr int kSerialTimeoutInMs {100};
constexpr int kCollectBoardIdSerialTimeoutInMs {300};
constexpr int kCrc32Size {4};
constexpr int kBoardIdSize {32};
constexpr int kTryToConnectTimeoutInMs {20000};

// Commands
constexpr char kVerifyFlasherCmd[] = "IMFlasher_Verify";
constexpr char kEraseCmd[] = "erase";
constexpr char kVersionCmd[] = "version";
constexpr char kBoardIdCmd[] = "board_id";
constexpr char kFlashFwCmd[] = "flash_fw";
constexpr char kEnterBlCmd[] = "enter_bl";
constexpr char kIsFwProtectedCmd[] = "is_fw_protected";
constexpr char kEnableFwProtectionCmd[] = "enable_fw_protection";
constexpr char kDisableFwProtectionCmd[] = "disable_fw_protection";
constexpr char kExitBlCmd[] = "exit_bl";
constexpr char kCheckSignatureCmd[] = "check_signature";
constexpr char kDisconnectCmd[] = "disconnect";

uint32_t Deserialize32(const uint8_t *buf)
{
    uint32_t result;
    result = static_cast<uint32_t>(buf[0] << 24U);
    result |= static_cast<uint32_t>(buf[1] << 16U);
    result |= static_cast<uint32_t>(buf[2] << 8U);
    result |= static_cast<uint32_t>(buf[3] << 0U);
    return result;
}

void ShowInfoMsg(const QString& title, const QString& description)
{
    QMessageBox msg_box;
    msg_box.setText(title);
    msg_box.setInformativeText(description);
    msg_box.setStandardButtons(QMessageBox::Ok);
    msg_box.exec();
}

} // namespace

Flasher::Flasher() = default;

Flasher::~Flasher()
{
    worker_thread_.quit();
    worker_thread_.wait();
}

void Flasher::Init()
{
    Worker *worker = new Worker;
    worker->moveToThread(&worker_thread_);
    connect(&worker_thread_, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &Flasher::RunLoop, worker, &Worker::DoWork);
    connect(worker, &Worker::FlasherLoop, this, &Flasher::LoopHandler);
    worker_thread_.start();
    emit RunLoop();
}

void Flasher::LoopHandler()
{
    switch (state_) {

    case FlasherStates::kIdle:
        // Idle state
        break;

    case FlasherStates::kTryToConnect:
        TryToConnect();
        break;

    case FlasherStates::kConnected:
        emit ShowStatusMsg("Connected");
        if (serial_port_.isOpen()) {
            GetVersion();
            emit SetButtons(is_bootloader_);

            if (is_bootloader_) {
                SetState(FlasherStates::kCheckBoardInfo);
            }
            else {
                SetState(FlasherStates::kIdle);
            }
        }
        break;

    case FlasherStates::kDisconnected:
    {
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
        if (CollectBoardId()) {
            emit ShowTextInBrowser("Board ID: " + board_id_);
            emit SetReadProtectionButtonText(IsFirmwareProtected());
            SetState(FlasherStates::kIdle);
        }
        else {
            emit ShowTextInBrowser("Board ID Error. Unplug your board, press disconnect/connect, and plug your board again.");
            SetState(FlasherStates::kError);
        }

        break;

    case FlasherStates::kServerDataExchange:
        if (board_id_.size() != kBoardIdSize) {
            emit ShowTextInBrowser("First connect board to get board id!");
        }

        SetState(FlasherStates::kIdle);
        break;

    case FlasherStates::kSelectFirmware:
    {
        QString file_path = QFileDialog::getOpenFileName(nullptr,
                                                        tr("Firmware binary"),
                                                        "",
                                                        tr("Binary (*.bin);;All Files (*)"));

        if (!file_path.isEmpty()) {
            if (OpenFirmwareFile(file_path)) {
                emit ShowTextInBrowser("Firmware: " + file_path);
                emit EnableLoadButton();
            }
        }

        SetState(FlasherStates::kIdle);
        break;
    }

    case FlasherStates::kFlash:
    {
        FlashingInfo flashing_info = Flash();
        ShowInfoMsg(flashing_info.title, flashing_info.description);
        emit ClearProgress();

        if (flashing_info.success) {
            serial_port_.CloseConn();
            SetState(FlasherStates::kTryToConnect);
        } else {
            SetState(FlasherStates::kError);
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
        }
        else {
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

FlashingInfo Flasher::Flash()
{
    FlashingInfo flashing_info;
    qint64 data_position = 0;
    QByteArray byte_array = firmware_file_.readAll();

    firmware_file_.close();
    const qint64 firmware_size = firmware_file_.size() - kSignatureSize;

    const char *data_signature = byte_array.data();
    const char *data_firmware = byte_array.data() + kSignatureSize;

    if (serial_port_.isOpen()) {
        flashing_info.success = SendMessage(kCheckSignatureCmd, sizeof(kCheckSignatureCmd), kSerialTimeoutInMs);

        if (!flashing_info.success) {
            flashing_info.title = "Flashing process failed";
            flashing_info.description = "Check signature problem";
        }

    } else {
        flashing_info.title = "Error";
        flashing_info.description = "Serial port is not opened";
    }

    if (flashing_info.success) {
        flashing_info.success = SendMessage(data_signature, kSignatureSize, kSerialTimeoutInMs);

        if (!flashing_info.success) {
            flashing_info.title = "Flashing process failed";
            flashing_info.description = "Send signature problem";
        }
    }

    if (flashing_info.success) {
        flashing_info.success = SendMessage(kVerifyFlasherCmd, sizeof(kVerifyFlasherCmd), kSerialTimeoutInMs);

        if (!flashing_info.success) {
            flashing_info.title = "Flashing process failed";
            flashing_info.description = "Verify flasher problem";
        }
    }

    if (flashing_info.success) {
        QByteArray file_size;
        file_size.setNum(firmware_size);
        flashing_info.success = SendMessage(file_size, file_size.size(), kSerialTimeoutInMs);

        if (!flashing_info.success) {
            flashing_info.title = "Flashing process failed";
            flashing_info.description = "Send file size problem";
        }
    }

    if (flashing_info.success) {
        flashing_info.success = Erase();

        if (!flashing_info.success) {
            flashing_info.title = "Flashing process failed";
            flashing_info.description = "Erasing problem";
        }
    }

    // Send file in packages
    if (flashing_info.success) {
        const qint64 packet_number = (firmware_size / kPacketSize);
        for (data_position = 0; data_position < packet_number; ++data_position) {
            const char *ptr_data_position;
            ptr_data_position = data_firmware + (data_position * kPacketSize);
            emit UpdateProgress((data_position + 1U) * kPacketSize, firmware_size);
            flashing_info.success = SendMessage(ptr_data_position, kPacketSize, kSerialTimeoutInMs);

            if (!flashing_info.success) {
                flashing_info.title = "Flashing process failed";
                flashing_info.description = "Problem with flashing";
                break;
            }
        }

        if (flashing_info.success) {
            const qint64 rest_size = firmware_size % kPacketSize;

            if (rest_size > 0) {
                emit UpdateProgress(data_position * kPacketSize + rest_size, firmware_size);
                flashing_info.success = SendMessage(data_firmware + (data_position * kPacketSize), rest_size, kSerialTimeoutInMs);

                if (!flashing_info.success) {
                    flashing_info.title = "Flashing process failed";
                    flashing_info.description = "Problem with flashing";
                }
            }
        }

        if (flashing_info.success) {
            flashing_info.success = CrcCheck((const uint8_t*) data_firmware, firmware_size);

            if (flashing_info.success) {
                flashing_info.title = "Flashing process done";
                flashing_info.description = "Successful flashing process";

            } else {
                flashing_info.title = "Flashing process failed";
                flashing_info.description = "CRC problem";
            }
        }
    }

    return flashing_info;
}

bool Flasher::CheckAck()
{
    bool success = false;
    const auto data = serial_port_.readAll();
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

bool Flasher::CheckTrue()
{
    //TODO: better handling needed. For error false is returned
    bool success = false;
    const auto data = serial_port_.readAll();

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

bool Flasher::CrcCheck(const uint8_t *data, const uint32_t size)
{
    QByteArray crc_data;
    uint32_t crc = crc::CalculateCrc32(data, size, false, false);
    crc_data.setNum(crc);

    return SendMessage(crc_data, crc_data.size(), kSerialTimeoutInMs);
}

bool Flasher::CollectBoardId()
{
    bool success = false;

    if (serial_port_.isOpen()) {
        serial_port_.write(kBoardIdCmd, sizeof(kBoardIdCmd));
        serial_port_.waitForReadyRead(kCollectBoardIdSerialTimeoutInMs);
        QByteArray data = serial_port_.readAll();
        QByteArray board_id = data.left(kBoardIdSize);
        QByteArray data_crc = data.right(kCrc32Size);

        uint32_t crc = Deserialize32((uint8_t*)data_crc.data());
        uint32_t calc_crc = crc::CalculateCrc32((uint8_t*)board_id.data(), static_cast<uint32_t>(kBoardIdSize), false, false);

        if ((board_id.size() == kBoardIdSize) && (calc_crc == crc)) {
            board_id_ = board_id.toHex();
            qInfo() << "Board ID: " << board_id_;
            success = true;
        }
        else {
            qInfo() << "Board id error";
        }
    }

    return success;
}

bool Flasher::Erase()
{
    bool success = false;

    if (serial_port_.isOpen()) {
        success = SendMessage(kEraseCmd, sizeof(kEraseCmd), kEraseTimeoutInMs);
    }

    return success;
}

void Flasher::GetVersion()
{
    serial_port_.write(kVersionCmd, sizeof(kVersionCmd));
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);
    emit ShowTextInBrowser(serial_port_.readAll());
}

bool Flasher::IsBootloaderDetected() const
{
    return is_bootloader_;
}

bool Flasher::IsFirmwareProtected()
{
    qInfo() << "Send is firmware protected command";
    serial_port_.write(kIsFwProtectedCmd, sizeof(kIsFwProtectedCmd));
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);
    return CheckTrue();
}

bool Flasher::OpenFirmwareFile(const QString& file_path)
{
    firmware_file_.setFileName(file_path);

    return firmware_file_.open(QIODevice::ReadOnly);
}

void Flasher::ReconnectingToBoard()
{
    if (is_timer_started_) {
        if (serial_port_.TryOpenPort(is_bootloader_)) {
            if (is_bootloader_ == is_bootloader_expected_) {
                SetState(FlasherStates::kConnected);
                is_timer_started_ = false;
            }
            else {
                serial_port_.CloseConn();
            }
        }

        if (timer_.hasExpired(kTryToConnectTimeoutInMs)) {
            ShowInfoMsg("Error!", "Entering/Exiting bootloader cannot be performed!");
            SetState(FlasherStates::kError);
            is_timer_started_ = false;
        }

    } else {
        emit DisableAllButtons();
        is_timer_started_ = true;
        timer_.start();
    }
}

bool Flasher::SendMessage(const char *data, qint64 length, int timeout_ms)
{
    serial_port_.write(data, length);
    serial_port_.waitForReadyRead(timeout_ms);
    return CheckAck();
}

bool Flasher::SendEnterBootloaderCommand()
{
    qInfo() << "Send enter bl command";
    return SendMessage(kEnterBlCmd, sizeof(kEnterBlCmd), kSerialTimeoutInMs);
}

void Flasher::SendFlashCommand()
{
    qInfo() << "Send flash command";
    serial_port_.write(kFlashFwCmd, sizeof(kFlashFwCmd));
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);
    // Check ack
}

void Flasher::SetState(const FlasherStates& state)
{
    state_ = state;
}

bool Flasher::SendEnableFirmwareProtection()
{
    bool success;
    qInfo() << "Send enable firmware protected command";
    success = SendMessage(kEnableFwProtectionCmd, sizeof(kEnableFwProtectionCmd), kSerialTimeoutInMs);
    ShowInfoMsg("Enable readout protection", "Powercyle the board!");
    return success;
}

bool Flasher::SendDisableFirmwareProtection()
{
    qInfo() << "Send disable firmware protected command";
    return SendMessage(kDisableFwProtectionCmd, sizeof(kDisableFwProtectionCmd), kSerialTimeoutInMs);
}

void Flasher::TryToConnectConsole()
{
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

void Flasher::TryToConnect()
{
    bool is_connected = false;

    if (is_timer_started_) {

        if (serial_port_.TryOpenPort(is_bootloader_)) {
            SetState(FlasherStates::kConnected);
            is_timer_started_ = false;
            is_connected = true;
        }
        else {
            emit ShowStatusMsg("Trying to connect...");
        }

        if ((!is_connected) && (timer_.hasExpired(kTryToConnectTimeoutInMs))) {
            emit FailedToConnect();
            SetState(FlasherStates::kError);
            is_timer_started_ = false;
        }
    }
    else {
        emit DisableAllButtons();
        is_timer_started_ = true;
        timer_.start();
    }
}

} // namespace flasher
