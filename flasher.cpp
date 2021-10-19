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
void Worker::doWork()
{
    emit flasherLoop();
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
    *value = (uint32_t)(buf[0] << 24U);
    *value |= (uint32_t)(buf[1] << 16U);
    *value |= (uint32_t)(buf[2] << 8U);
    *value |= (uint32_t)(buf[3] << 0U);
}

void ShowInfoMsg(const QString& title, const QString& description)
{
    QMessageBox msgBox;
    msgBox.setText(title);
    msgBox.setInformativeText(description);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

} // namespace

Flasher::Flasher()
{
    const QString kKeyFileName = "keys.json";
    m_keysFile.setFileName(kKeyFileName);
    m_keysFile.open(QIODevice::ReadWrite);
    if(m_keysFile.isOpen()) {
        QString keysJson = m_keysFile.readAll();
        m_keysFile.close();

        QJsonDocument jsonData = QJsonDocument::fromJson(keysJson.toUtf8());
        if(!jsonData.isEmpty()) {
            m_jsonObject = jsonData.object();
        }
    }
}

Flasher::~Flasher()
{
    workerThread.quit();
    workerThread.wait();
}

void Flasher::init()
{
    Worker *worker = new Worker;
    worker->moveToThread(&workerThread);
    connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &Flasher::runLoop, worker, &Worker::doWork);
    connect(worker, &Worker::flasherLoop, this, &Flasher::loopHandler);
    workerThread.start();
    emit runLoop();
}

void Flasher::loopHandler()
{
    switch (m_state) {

    case FlasherStates::kIdle:
        // Idle state
        break;

    case FlasherStates::kTryToConnect:
        TryToConnect();
        break;

    case FlasherStates::kConnected:
        emit showStatusMsg("Connected");
        if (serial_port_.isOpen()) {
            GetVersion();
            emit isBootloader(is_bootloader_);

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
        bool isDisconnectedSuccess = true;
        is_timer_started_ = false;

        if (serial_port_.isOpen()) {

            if (is_bootloader_) {
                isDisconnectedSuccess = SendDisconnectCmd();
            }

            serial_port_.CloseConn();
        }

        if (isDisconnectedSuccess) {
            emit showStatusMsg("Disconnected");

        } else {
            emit showStatusMsg("Unplug board!");
        }

        SetState(FlasherStates::kIdle);

        break;
    }

    case FlasherStates::kCheckBoardInfo:
    {
        if (CollectBoardId()) {
            emit textInBrowser("Board ID: " + m_boardId);
            if (GetBoardKey()) {
                emit textInBrowser("Board verified!");
                emit isReadProtectionEnabled(IsFirmwareProtected());
            }
            else {
                emit textInBrowser("Board is not registered! Press the register button");
                emit enableRegisterButton();
            }

            SetState(FlasherStates::kIdle);
        }
        else {
            emit textInBrowser("Board ID Error. Unplug your board, press disconnect/connect, and plug your board again.");
            SetState(FlasherStates::kError);
        }

        break;
    }

    case FlasherStates::kRegisterBoard:
    {
        if (m_boardId.size() != kBoardIdSizeString) {
            emit textInBrowser("First connect board to get board id!");
        }
        else {
            if (GetBoardKeyFromServer()) {
                if (m_boardKey.size() != kKeySizeString) {
                    emit textInBrowser("This Board ID is not verified. Check www.imsecure.xyz");
                }
                else {
                    emit textInBrowser("Key: " + m_boardKey);
                    SaveBoardKeyToFile();
                }
            }
            else {
                emit textInBrowser("Server error, please contact the administrator!");
            }
        }

        SetState(FlasherStates::kIdle);
        break;
    }

    case FlasherStates::kSelectFirmware:
    {
        QString filePath = QFileDialog::getOpenFileName(nullptr,
                                                        tr("Firmware binary"),
                                                        "",
                                                        tr("Binary (*.bin);;All Files (*)"));

        if (!filePath.isEmpty()) {
            if (OpenFirmwareFile(filePath)) {
                emit textInBrowser("Firmware: " + filePath);
                emit enableLoadButton();
            }
        }

        SetState(FlasherStates::kIdle);
        break;
    }

    case FlasherStates::kFlash:
    {
        std::tuple<bool, QString, QString> flashingInfo = this->startFlash();
        ShowInfoMsg(std::get<1>(flashingInfo), std::get<2>(flashingInfo));
        emit clearProgress();

        if (std::get<0>(flashingInfo)) {
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
        emit showStatusMsg("Entering bootloader...");
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
        emit showStatusMsg("Exiting bootloader...");
        ReconnectingToBoard();
        break;

    case FlasherStates::kReconnect:
        emit disableAllButtons();
        serial_port_.CloseConn();

        if (!serial_port_.isOpen()) {
            SetState(FlasherStates::kTryToConnect);
        }

        break;

    case FlasherStates::kError:
        emit showStatusMsg("Error");
        emit disableAllButtons();
        break;

    default:
        qInfo() << "Unspecified state";
        break;
    }

    workerThread.msleep(kThreadSleepTimeInMs);
    emit runLoop();
}

bool Flasher::startErase()
{
    bool success = false;

    if (serial_port_.isOpen()) {

        if (SendKey()) {
            // Send flash verifying string
            qInfo() << "ERASE";
            serial_port_.write(kEraseCmd, sizeof(kEraseCmd));
            serial_port_.waitForReadyRead(kEraseTimeoutInMs);
            success = checkAck();
        }
    }

    return success;
}

std::tuple<bool, QString, QString> Flasher::startFlash()
{
    bool success = false;
    QString title = "Unknown";
    QString description = "Unknown";
    qint64 dataPosition = 0;
    QByteArray byteArray = m_fileFirmware.readAll();

    m_fileFirmware.close();
    const qint64 firmwareSize = m_fileFirmware.size() - kSignatureSize;

    char *ptrDataSignature = byteArray.data();
    char *ptrDataFirmware = byteArray.data() + kSignatureSize;

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
        success = checkAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Check signature problem";
        }
    }

    if (success) {
        qInfo() << "Send signature";
        serial_port_.write(ptrDataSignature, kSignatureSize);
        serial_port_.waitForReadyRead(kSerialTimeoutInMs);
        success = checkAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Send signature problem";
        }
    }

    if (success) {
        qInfo() << "Verify flasher";
        serial_port_.write(kVerifyFlasherCmd, sizeof(kVerifyFlasherCmd));
        serial_port_.waitForReadyRead(kSerialTimeoutInMs);
        success = checkAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Verify flasher problem";
        }
    }

    if (success) {
        QByteArray sizeDataString;
        sizeDataString.setNum(firmwareSize);
        qInfo() << "Send file size";
        serial_port_.write(sizeDataString);
        serial_port_.waitForReadyRead(kSerialTimeoutInMs);
        success = checkAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Send file size problem";
        }
    }

    if (success) {
        qInfo() << "Erase";
        serial_port_.write(kEraseCmd, sizeof(kEraseCmd));
        serial_port_.waitForReadyRead(kEraseTimeoutInMs);
        success = checkAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Erasing problem";
        }
    }

    // Send file in packages
    if (success) {
        qint64 packetsNumber = (firmwareSize / kPacketSize);
        for (dataPosition = 0; dataPosition < packetsNumber; ++dataPosition) {
            char* ptrDataPosition;
            ptrDataPosition = ptrDataFirmware + (dataPosition * kPacketSize);

            serial_port_.write(ptrDataPosition, kPacketSize);
            serial_port_.waitForReadyRead(kSerialTimeoutInMs);
            emit updateProgress((dataPosition + 1U) * kPacketSize, firmwareSize);
            success = checkAck();

            if (!success) {
                title = "Flashing process failed";
                description = "Problem with flashing";
                break;
            }
        }

        if (success) {
            qint64 restSize = firmwareSize % kPacketSize;

            if (restSize > 0) {
                serial_port_.write(ptrDataFirmware + (dataPosition * kPacketSize), restSize);
                serial_port_.waitForReadyRead(kSerialTimeoutInMs);
                emit updateProgress(dataPosition * kPacketSize + restSize, firmwareSize);
                success = checkAck();

                if (!success) {
                    title = "Flashing process failed";
                    description = "Problem with flashing";
                }
            }
        }

        if (success) {
            qInfo() << "CRC";
            success = this->crcCheck((const uint8_t*) ptrDataFirmware, firmwareSize);

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

bool Flasher::crcCheck(const uint8_t* data, uint32_t size)
{
    QByteArray crcData;
    uint32_t crc = crc::CalculateCrc32(data, size, false, false);
    crcData.setNum(crc);
    serial_port_.write(crcData);
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);

    return checkAck();
}

bool Flasher::checkAck()
{
    bool success = false;
    const auto data = serial_port_.readAll();
    if(data.size() >= 2) {

        if(0 == QString::compare("OK", data, Qt::CaseInsensitive)) {
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

bool Flasher::checkTrue()
{
    //TODO: better handeling needed. For error flase is returned
    bool success = false;
    const auto data = serial_port_.readAll();

    if(0 == QString::compare("TRUE", data, Qt::CaseInsensitive)) {
        qInfo() << "TRUE";
        success = true;
    } else if (0 == QString::compare("FALSE", data, Qt::CaseInsensitive)) {
        qInfo() << "FALSE";
    } else {
        qInfo() << "ERROR or TIMEOUT";
    }

    return success;
}

bool Flasher::CollectBoardId()
{
    bool success = false;

    if (serial_port_.isOpen()) {
        serial_port_.write(kBoardIdCmd, sizeof(kBoardIdCmd));
        serial_port_.waitForReadyRead(kCollectBoardIdSerialTimeoutInMs);
        QByteArray data = serial_port_.readAll();
        QByteArray boardId;
        QByteArray dataCrc;
        uint32_t crc;

        boardId = data.left(kBoardIdSize);
        dataCrc = data.right(kCrc32Size);
        Deserialize32((uint8_t*)dataCrc.data(), &crc);

        uint32_t calcCrc = crc::CalculateCrc32((uint8_t*)boardId.data(), static_cast<uint32_t>(kBoardIdSize), false, false);

        if ((boardId.size() == kBoardIdSize) && (calcCrc == crc)) {
            m_boardId = boardId.toHex();
            qInfo() << "Board ID: " << m_boardId;
            success = true;
        }
        else {
            qInfo() << "Board id error";
        }
    }

    return success;
}

bool Flasher::GetBoardKey()
{
    bool success;
    const QByteArray magic_string = "NOT_SECURED_MAGIC_STRING_1234567";

    if (m_boardId != magic_string.toHex()) {
        m_boardKey = m_jsonObject.value(m_boardId).toString();

        if (m_boardKey.size() == kKeySizeString) {
            success = true;
        }
        else {
            success = false;
            qInfo() << "Board key error";
        }

    } else {
        m_isSecureBoot = false;
        success = true;
    }

    return success;
}

bool Flasher::GetBoardKeyFromServer()
{
    bool success = false;
    QByteArray dataIn = QByteArray::fromHex(m_boardId.toUtf8());
    QByteArray dataOut;

    if (socket::DataTransfer(dataIn, dataOut)) {
        m_boardKey = dataOut.toHex();
        success = true;
    }

    return success;
}

void Flasher::GetVersion()
{
    serial_port_.write(kVersionCmd, sizeof(kVersionCmd));
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);
    emit textInBrowser(serial_port_.readAll());
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
    return checkTrue();
}

bool Flasher::OpenFirmwareFile(const QString& filePath)
{
    m_fileFirmware.setFileName(filePath);

    return m_fileFirmware.open(QIODevice::ReadOnly);
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
        emit disableAllButtons();
        is_timer_started_ = true;
        timer_.start();
    }
}

void Flasher::SaveBoardKeyToFile()
{
    QTextStream streamConfigFile(&m_keysFile);
    m_keysFile.open(QIODevice::ReadWrite);

    QJsonDocument jsonData;
    m_jsonObject.insert(m_boardId, m_boardKey);

    jsonData.setObject(m_jsonObject);

    streamConfigFile << jsonData.toJson();
    m_keysFile.close();
}

bool Flasher::SendDisconnectCmd()
{
    qInfo() << "Send disconnect command";
    serial_port_.write(kDisconnectCmd, sizeof(kDisconnectCmd));
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);
    return checkAck();
}

bool Flasher::SendEnterBootloaderCommand()
{
    qInfo() << "Send enter bl command";
    serial_port_.write(kEnterBlCmd, sizeof(kEnterBlCmd));
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);
    return checkAck();
}

bool Flasher::SendExitBootloaderCommand()
{
    qInfo() << "Send exit bl command";
    serial_port_.write(kExitBlCmd, sizeof(kExitBlCmd));
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);
    return checkAck();
}

void Flasher::SendFlashCommand()
{
    qInfo() << "Send flash command";
    serial_port_.write(kFlashFwCmd, sizeof(kFlashFwCmd));
    QThread::msleep(400); //wait for restart
}

bool Flasher::SendKey()
{
    bool success = false;
    if (m_isSecureBoot) {
        qInfo() << "Key";
        QByteArray boardKeyData = QByteArray::fromHex(m_boardKey.toUtf8());
        serial_port_.write(boardKeyData, boardKeyData.size());
        serial_port_.waitForReadyRead(kSerialTimeoutInMs);
        success = checkAck();
    }
    else {
        success = true;
    }

    return success;
}

void Flasher::SetState(const FlasherStates& state)
{
    m_state = state;
}

bool Flasher::sendEnableFirmwareProtection()
{
    bool success;
    qInfo() << "Send enable firmware protected command";
    serial_port_.write(kEnableFwProtectionCmd, sizeof(kEnableFwProtectionCmd));
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);
    success = checkAck();
    ShowInfoMsg("Enable readout protection", "Powercyle the board!");
    return success;
}

bool Flasher::sendDisableFirmwareProtection()
{
    qInfo() << "Send disable firmware protected command";
    serial_port_.write(kDisableFwProtectionCmd, sizeof(kDisableFwProtectionCmd));
    serial_port_.waitForReadyRead(kSerialTimeoutInMs);
    return checkAck();
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
            emit showStatusMsg("Trying to connect...");
        }

        if ((!is_connected) && (timer_.hasExpired(kTryToConnectTimeoutInMs))) {
            emit failedToConnect();
            SetState(FlasherStates::kError);
            is_timer_started_ = false;
        }
    }
    else {
        emit disableAllButtons();
        is_timer_started_ = true;
        timer_.start();
    }
}

} // namespace flasher
