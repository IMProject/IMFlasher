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
constexpr int kBoardIdSizeString {kBoardIdSize * 2};
constexpr int kKeySize {32};
constexpr int kKeySizeString {kKeySize * 2};
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

void Deserialize32(const uint8_t* buf, uint32_t* value)
{
    *value = static_cast<uint32_t>(buf[0] << 24U);
    *value |= static_cast<uint32_t>(buf[1] << 16U);
    *value |= static_cast<uint32_t>(buf[2] << 8U);
    *value |= static_cast<uint32_t>(buf[3] << 0U);
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

Flasher::Flasher()
{
    const QString kKeyFileName = "keys.json";
    keys_file_.setFileName(kKeyFileName);

    if (keys_file_.open(QIODevice::ReadWrite)) {
        QString keys_json = keys_file_.readAll();
        keys_file_.close();

        QJsonDocument json_data = QJsonDocument::fromJson(keys_json.toUtf8());
        if (!json_data.isEmpty()) {
            json_object_ = json_data.object();
        }
    }
}

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
                is_disconnected_success = SendDisconnectCmd();
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
            if (GetBoardKey()) {
                emit ShowTextInBrowser("Board verified!");
                emit SetReadProtectionButtonText(IsFirmwareProtected());
            }
            else {
                emit ShowTextInBrowser("Board is not registered! Press the register button");
                emit EnableRegisterButton();
            }

            SetState(FlasherStates::kIdle);
        }
        else {
            emit ShowTextInBrowser("Board ID Error. Unplug your board, press disconnect/connect, and plug your board again.");
            SetState(FlasherStates::kError);
        }

        break;

    case FlasherStates::kRegisterBoard:
        if (board_id_.size() != kBoardIdSizeString) {
            emit ShowTextInBrowser("First connect board to get board id!");
        }
        else {
            if (GetBoardKeyFromServer()) {
                if (board_key_.size() != kKeySizeString) {
                    emit ShowTextInBrowser("This Board ID is not verified. Check www.imsecure.xyz");
                }
                else {
                    emit ShowTextInBrowser("Key: " + board_key_);
                    SaveBoardKeyToFile();
                }
            }
            else {
                emit ShowTextInBrowser("Server error, please contact the administrator!");
            }
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
        std::tuple<bool, QString, QString> flashing_info = Flash();
        ShowInfoMsg(std::get<1>(flashing_info), std::get<2>(flashing_info));
        emit ClearProgress();

        if (std::get<0>(flashing_info)) {
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
        if (SendExitBootloaderCommand()) {
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

std::tuple<bool, QString, QString> Flasher::Flash()
{
    bool success = false;
    QString title = "Unknown";
    QString description = "Unknown";
    qint64 data_position = 0;
    QByteArray byte_array = firmware_file_.readAll();

    firmware_file_.close();
    const qint64 firmware_size = firmware_file_.size() - kSignatureSize;

    char *data_signature = byte_array.data();
    char *data_firmware = byte_array.data() + kSignatureSize;

    if (serial_port_.isOpen()) {
        success = SendKey();
        if (!success) {
            title = "Error";
            description = "Send key problem";
        }
    }
    else {
        title = "Error";
        description = "Serial port is not opened";
    }

    if (success) {
        qInfo() << "Check signature";
        serial_port_.write(kCheckSignatureCmd, sizeof(kCheckSignatureCmd));
        serial_port_.waitForReadyRead(kSerialTimeoutInMs);
        success = CheckAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Check signature problem";
        }
    }

    if (success) {
        qInfo() << "Send signature";
        serial_port_.write(data_signature, kSignatureSize);
        serial_port_.waitForReadyRead(kSerialTimeoutInMs);
        success = CheckAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Send signature problem";
        }
    }

    if (success) {
        qInfo() << "Verify flasher";
        serial_port_.write(kVerifyFlasherCmd, sizeof(kVerifyFlasherCmd));
        serial_port_.waitForReadyRead(kSerialTimeoutInMs);
        success = CheckAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Verify flasher problem";
        }
    }

    if (success) {
        QByteArray file_size;
        file_size.setNum(firmware_size);
        qInfo() << "Send file size";
        serial_port_.write(file_size);
        serial_port_.waitForReadyRead(kSerialTimeoutInMs);
        success = CheckAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Send file size problem";
        }
    }

    if (success) {
        qInfo() << "Erase";
        serial_port_.write(kEraseCmd, sizeof(kEraseCmd));
        serial_port_.waitForReadyRead(kEraseTimeoutInMs);
        success = CheckAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Erasing problem";
        }
    }

    // Send file in packages
    if (success) {
        const qint64 packet_number = (firmware_size / kPacketSize);
        for (data_position = 0; data_position < packet_number; ++data_position) {
            char* ptr_data_position;
            ptr_data_position = data_firmware + (data_position * kPacketSize);

            serial_port_.write(ptr_data_position, kPacketSize);
            serial_port_.waitForReadyRead(kSerialTimeoutInMs);
            emit UpdateProgress((data_position + 1U) * kPacketSize, firmware_size);
            success = CheckAck();

            if (!success) {
                title = "Flashing process failed";
                description = "Problem with flashing";
                break;
            }
        }

        if (success) {
            const qint64 rest_size = firmware_size % kPacketSize;

            if (rest_size > 0) {
                serial_port_.write(data_firmware + (data_position * kPacketSize), rest_size);
                serial_port_.waitForReadyRead(kSerialTimeoutInMs);
                emit UpdateProgress(data_position * kPacketSize + rest_size, firmware_size);
                success = CheckAck();

                if (!success) {
                    title = "Flashing process failed";
                    description = "Problem with flashing";
                }
            }
        }

        if (success) {
            qInfo() << "CRC";
            success = CrcCheck((const uint8_t*) data_firmware, firmware_size);

            if (success) {
                title = "Flashing process done";
                description = "Successful flashing process";

            } else {
                title = "Flashing process failed";
                description = "CRC problem";
            }
        }
    }

    return std::make_tuple(success, title, description);
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
    serial_port_.write(crc_data);
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);

    return CheckAck();
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
        uint32_t crc;

        Deserialize32((uint8_t*)data_crc.data(), &crc);

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

        if (SendKey()) {
            qInfo() << "ERASE";
            serial_port_.write(kEraseCmd, sizeof(kEraseCmd));
            serial_port_.waitForReadyRead(kEraseTimeoutInMs);
            success = CheckAck();
        }
    }

    return success;
}

bool Flasher::GetBoardKey()
{
    bool success;
    const QByteArray magic_string = "NOT_SECURED_MAGIC_STRING_1234567";

    if (board_id_ != magic_string.toHex()) {
        board_key_ = json_object_.value(board_id_).toString();

        if (board_key_.size() == kKeySizeString) {
            success = true;
        }
        else {
            success = false;
            qInfo() << "Board key error";
        }

    } else {
        is_secure_boot_ = false;
        success = true;
    }

    return success;
}

bool Flasher::GetBoardKeyFromServer()
{
    bool success = false;
    QByteArray data_input = QByteArray::fromHex(board_id_.toUtf8());
    QByteArray data_output;

    if (socket::DataTransfer(data_input, data_output)) {
        board_key_ = data_output.toHex();
        success = true;
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

void Flasher::SaveBoardKeyToFile()
{
    QTextStream stream_config_file(&keys_file_);
    keys_file_.open(QIODevice::ReadWrite);

    QJsonDocument json_data;
    json_object_.insert(board_id_, board_key_);

    json_data.setObject(json_object_);

    stream_config_file << json_data.toJson();
    keys_file_.close();
}

bool Flasher::SendDisconnectCmd()
{
    qInfo() << "Send disconnect command";
    serial_port_.write(kDisconnectCmd, sizeof(kDisconnectCmd));
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);
    return CheckAck();
}

bool Flasher::SendEnterBootloaderCommand()
{
    qInfo() << "Send enter bl command";
    serial_port_.write(kEnterBlCmd, sizeof(kEnterBlCmd));
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);
    return CheckAck();
}

bool Flasher::SendExitBootloaderCommand()
{
    qInfo() << "Send exit bl command";
    serial_port_.write(kExitBlCmd, sizeof(kExitBlCmd));
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);
    return CheckAck();
}

void Flasher::SendFlashCommand()
{
    qInfo() << "Send flash command";
    serial_port_.write(kFlashFwCmd, sizeof(kFlashFwCmd));
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);
    // Check ack
}

bool Flasher::SendKey()
{
    bool success = false;
    if (is_secure_boot_) {
        qInfo() << "Key";
        QByteArray board_key = QByteArray::fromHex(board_key_.toUtf8());
        serial_port_.write(board_key, board_key.size());
        serial_port_.waitForReadyRead(kSerialTimeoutInMs);
        success = CheckAck();
    }
    else {
        success = true;
    }

    return success;
}

void Flasher::SetState(const FlasherStates& state)
{
    state_ = state;
}

bool Flasher::SendEnableFirmwareProtection()
{
    bool success;
    qInfo() << "Send enable firmware protected command";
    serial_port_.write(kEnableFwProtectionCmd, sizeof(kEnableFwProtectionCmd));
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);
    success = CheckAck();
    ShowInfoMsg("Enable readout protection", "Powercyle the board!");
    return success;
}

bool Flasher::SendDisableFirmwareProtection()
{
    qInfo() << "Send disable firmware protected command";
    serial_port_.write(kDisableFwProtectionCmd, sizeof(kDisableFwProtectionCmd));
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);
    return CheckAck();
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
