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
#include <QJsonDocument>
#include <QMessageBox>

#include "crc32.h"
#include "serial_port.h"

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

Flasher::Flasher() :
        m_serialPort(std::make_shared<communication::SerialPort>()),
        m_keysFile(std::make_shared<QFile>())
{
    const QString kKeyFileName = "keys.json";
    m_keysFile->setFileName(kKeyFileName);
    m_keysFile->open(QIODevice::ReadWrite);
    if(m_keysFile->isOpen()) {
        QString keysJson = m_keysFile->readAll();
        m_keysFile->close();

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

void Flasher::openSerialPort()
{
    this->setState(FlasherStates::kTryToConnect);
    m_tryOpen = true;
}

void Flasher::closeSerialPort()
{
    this->setState(FlasherStates::kDisconnected);
    m_tryOpen = false;
}

void Flasher::reopenSerialPort()
{
    closeSerialPort();
    QThread::msleep(400);
    openSerialPort();
}

void Flasher::loopHandler()
{
    bool success;

    switch (m_state) {

        case FlasherStates::kIdle:
            if (m_fileFirmware.isOpen() && m_serialPort->isOpen()) {
                emit readyToFlashId();
            }
            break;

        case FlasherStates::kInit:

            if (m_tryOpen) {

                if (!(m_serialPort->tryOpenPort())) {
                    this->setState(FlasherStates::kTryToConnect);
                }
            }

            break;

        case FlasherStates::kTryToConnect:
        {
            bool isConnected = false;

            if (m_isTryConnectStart) {

                if (m_serialPort->tryOpenPort()) {

                    if (m_serialPort->isOpen()) {
                        this->setState(FlasherStates::kConnected);
                        m_isTryConnectStart = false;
                        isConnected = true;

                    } else {
                        emit showStatusMsg("Trying to connect!");
                    }

                } else {
                    emit showStatusMsg("Trying to connect!");
                }

                if ((!isConnected) && (m_timerTryConnect.hasExpired(kTryToConnectTimeoutInMs))) {
                    emit failedToConnect();
                    this->setState(FlasherStates::kInit);
                    m_tryOpen = false;
                    m_isTryConnectStart = false;
                }

            } else {
                m_isTryConnectStart = true;
                m_timerTryConnect.start();
            }

            break;
        }

        case FlasherStates::kConnected:

            emit showStatusMsg("Connected");
            if(m_serialPort->isOpen()) {
                getVersion();

                if(m_serialPort->isBootloaderDetected()) {
                    emit isBootloader(true);
                    this->setState(FlasherStates::kBoardId);
                } else {
                    emit isBootloader(false);
                    this->setState(FlasherStates::kIdle);
                }
            }
            break;

        case FlasherStates::kDisconnected:
        {
            bool isDisconnectedSuccess = true;
            m_isTryConnectStart = false;

            if (m_serialPort->isOpen()) {

                if (m_serialPort->isBootloaderDetected()) {
                    isDisconnectedSuccess = this->sendDisconnectCmd();
                }

                m_serialPort->closeConn();
            }

            if (isDisconnectedSuccess) {
                emit showStatusMsg("Disconnected");

            } else {
                emit showStatusMsg("Unplug board!");
            }

            this->setState(FlasherStates::kIdle);

            break;
        }

        case FlasherStates::kBoardId:

            success = collectBoardId();
            if(success) {
                emit textInBrowser("Board ID: " + m_boardId);
                this->setState(FlasherStates::kBoardCheckRegistration);
            } else {
                emit textInBrowser("Board ID Error. Unplug your board, press disconnect/connect, and plug your board again.");
                this->setState(FlasherStates::kInit);
            }

            break;

        case FlasherStates::kBoardCheckRegistration:
            success = getBoardKey();
            if(success) {
                emit textInBrowser("Board verified!");
            } else {
                emit textInBrowser("Board is not registered! Press the register button");
            }

            emit isReadProtectionEnabled(checkIfFirmwareIsProtected());

            this->setState(FlasherStates::kIdle);

            break;

        case FlasherStates::kGetBoardIdKey:
            if(m_boardId.size() != kBoardIdSizeString) {
                emit textInBrowser("First connect board to get board id!");

            }   else {
                success = getBoardKeyFromServer();
                if(success && (m_boardKey.size() != kKeySizeString)) {
                    emit textInBrowser("This Board ID is not verified. Check www.imsecure.xyz");
                }
                else if(!success) {
                    emit textInBrowser("Server error, please contact the administrator!");
                } else {
                    emit textInBrowser("Key: " + m_boardKey);
                    saveBoardKeyToFile();
                }
            }

            this->setState(FlasherStates::kIdle);
            break;

        case FlasherStates::kOpenFile:
            if(m_filePath.size() > 0) { //check if file path exist
                openFirmwareFile(m_filePath);
                if (m_fileFirmware.isOpen()) {
                    QByteArray fileSize;
                    fileSize.setNum(m_firmwareSize);
                    emit textInBrowser("Board ID: " + m_boardId);
                }
            }

            this->setState(FlasherStates::kIdle);
            break;

        case FlasherStates::kFlash:
        {
            std::tuple<bool, QString, QString> flashingInfo = this->startFlash();
            ShowInfoMsg(std::get<1>(flashingInfo), std::get<2>(flashingInfo));
            emit clearProgress();

            if (std::get<0>(flashingInfo)) {
                m_serialPort->closeConn();
                this->setState(FlasherStates::kTryToConnect);

            } else {
                this->setState(FlasherStates::kIdle);
            }
            break;
        }

        default:
            break;
    }

    workerThread.msleep(kThreadSleepTimeInMs);
    emit runLoop();
}

std::shared_ptr<communication::SerialPort> Flasher::getSerialPort() const
{
    return m_serialPort;
}

bool Flasher::collectBoardId()
{
    bool success = false;

    if (m_serialPort->isOpen()) {
        m_serialPort->write(kBoardIdCmd, sizeof(kBoardIdCmd));
        m_serialPort->waitForReadyRead(kCollectBoardIdSerialTimeoutInMs);
        QByteArray data = m_serialPort->readAll();
        QByteArray boardId;
        QByteArray dataCrc;
        uint32_t crc;

        boardId = data.left(kBoardIdSize);
        dataCrc = data.right(kCrc32Size);
        Deserialize32((uint8_t*)dataCrc.data(), &crc);

        uint32_t calcCrc = crc::CalculateCrc32((uint8_t*)boardId.data(), static_cast<uint32_t>(kBoardIdSize), false, false);

        if((boardId.size() == kBoardIdSize) && (calcCrc == crc)) {
            qInfo() << "BOARD ID";
            m_boardId = boardId.toHex();
            qInfo() << m_boardId;
            success = true;
        } else {
            qInfo() << "Board id error";
        }
    }

    return success;
}

bool Flasher::getBoardKey()
{
    bool success = false;
    const QByteArray kNotSecuredMagicString = "NOT_SECURED_MAGIC_STRING_1234567";

    //Skip security
    QByteArray magic_string = kNotSecuredMagicString;
    if (m_boardId != magic_string.toHex()) {

        QJsonValue boardKey = m_jsonObject.value(m_boardId);
        m_boardKey = boardKey.toString();

        if (m_boardKey.size() == kKeySizeString) {

            success = true;

        } else {
            qInfo() << "Board key error";
        }

    } else {
        m_isSecureBoot = false;
        success = true;
    }

    return success;
}

void Flasher::saveBoardKeyToFile()
{
    QTextStream streamConfigFile(m_keysFile.get());
    m_keysFile->open(QIODevice::ReadWrite);

    QJsonDocument jsonData;
    m_jsonObject.insert(m_boardId, m_boardKey);

    jsonData.setObject(m_jsonObject);

    streamConfigFile << jsonData.toJson();
    m_keysFile->close();
}

bool Flasher::getBoardKeyFromServer()
{
    bool success = false;
    QByteArray dataIn = QByteArray::fromHex(m_boardId.toUtf8());

    QByteArray dataOut;
    success = m_socketClient.DataTransfer(dataIn, dataOut);
    if(success) {
        m_boardKey = dataOut.toHex();
    }

    return success;
}

bool Flasher::sendKey()
{
    bool success = false;
    if(m_isSecureBoot) {
        //Send key
        qInfo() << "KEY";
        QByteArray boardKeyData = QByteArray::fromHex(m_boardKey.toUtf8());
        m_serialPort->write(boardKeyData, boardKeyData.size());

        m_serialPort->waitForReadyRead(kSerialTimeoutInMs);
        success = checkAck();
    } else {
        success = true;
    }

    return success;
}

bool Flasher::startErase()
{
    bool success = false;

    if (m_serialPort->isOpen()) {

        if (this->sendKey()) {
            // Send flash verifying string
            qInfo() << "ERASE";
            m_serialPort->write(kEraseCmd, sizeof(kEraseCmd));
            m_serialPort->waitForReadyRead(kEraseTimeoutInMs);
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
    m_firmwareSize = m_fileFirmware.size() - kSignatureSize;

    char *ptrDataSignature = byteArray.data();
    char *ptrDataFirmware = byteArray.data() + kSignatureSize;

    if (m_serialPort->isOpen()) {
        success = sendKey();

    } else {
        title = "Error";
        description = "Serial port is not opened";
    }

    if (success) {
        qInfo() << "Check signature";
        m_serialPort->write(kCheckSignatureCmd, sizeof(kCheckSignatureCmd));
        m_serialPort->waitForReadyRead(kSerialTimeoutInMs);
        success = checkAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Check signature problem";
        }
    }

    if (success) {
        qInfo() << "Send signature";
        m_serialPort->write(ptrDataSignature, kSignatureSize);
        m_serialPort->waitForReadyRead(kSerialTimeoutInMs);
        success = checkAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Send signature problem";
        }
    }

    if (success) {
        qInfo() << "Verify flasher";
        m_serialPort->write(kVerifyFlasherCmd, sizeof(kVerifyFlasherCmd));
        m_serialPort->waitForReadyRead(kSerialTimeoutInMs);
        success = checkAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Verify flasher problem";
        }
    }

    if (success) {
        QByteArray sizeDataString;
        sizeDataString.setNum(m_firmwareSize);
        qInfo() << "Send file size";
        m_serialPort->write(sizeDataString);
        m_serialPort->waitForReadyRead(kSerialTimeoutInMs);
        success = checkAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Send file size problem";
        }
    }

    if (success) {
        qInfo() << "Erase";
        m_serialPort->write(kEraseCmd, sizeof(kEraseCmd));
        m_serialPort->waitForReadyRead(kEraseTimeoutInMs);
        success = checkAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Erasing problem";
        }
    }

    // Send file in packages
    if (success) {
        qint64 packetsNumber = (m_firmwareSize / kPacketSize);
        for (dataPosition = 0; dataPosition < packetsNumber; ++dataPosition) {
            char* ptrDataPosition;
            ptrDataPosition = ptrDataFirmware + (dataPosition * kPacketSize);

            m_serialPort->write(ptrDataPosition, kPacketSize);
            m_serialPort->waitForReadyRead(kSerialTimeoutInMs);
            emit updateProgress((dataPosition + 1U) * kPacketSize, m_firmwareSize);
            success = checkAck();

            if (!success) {
                title = "Flashing process failed";
                description = "Problem with flashing";
                break;
            }
        }

        if (success) {
            qint64 restSize = m_firmwareSize % kPacketSize;

            if (restSize > 0) {
                m_serialPort->write(ptrDataFirmware + (dataPosition * kPacketSize), restSize);
                m_serialPort->waitForReadyRead(kSerialTimeoutInMs);
                emit updateProgress(dataPosition * kPacketSize + restSize, m_firmwareSize);
                success = checkAck();

                if (!success) {
                    title = "Flashing process failed";
                    description = "Problem with flashing";
                }
            }
        }

        if (success) {
            qInfo() << "CRC";
            success = this->crcCheck((const uint8_t*) ptrDataFirmware, m_firmwareSize);

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
    m_serialPort->write(crcData);
    m_serialPort->waitForReadyRead(kSerialTimeoutInMs);

    return checkAck();
}

bool Flasher::checkAck()
{
    bool success = false;
    const QByteArray data = m_serialPort->readAll();
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
    const QByteArray data = m_serialPort->readAll();

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

bool Flasher::openFirmwareFile(const QString& filePath)
{
    m_fileFirmware.setFileName(filePath);

    return m_fileFirmware.open(QIODevice::ReadOnly);
}

void Flasher::setFilePath(const QString& filePath)
{
    m_filePath = filePath;
}

void Flasher::setState(const FlasherStates& state)
{
    m_state = state;
}

bool Flasher::SendEnterBootloaderCommand()
{
    qInfo() << "Send enter bl command";
    m_serialPort->write(kEnterBlCmd, sizeof(kEnterBlCmd));
    m_serialPort->waitForReadyRead(kSerialTimeoutInMs);
    return checkAck();
}

bool Flasher::SendExitBootloaderCommand()
{
    qInfo() << "Send exit bl command";
    m_serialPort->write(kExitBlCmd, sizeof(kExitBlCmd));
    m_serialPort->waitForReadyRead(kSerialTimeoutInMs);
    return checkAck();
}

void Flasher::sendFlashCommand(void)
{
    qInfo() << "Send flash command";
    m_serialPort->write(kFlashFwCmd, sizeof(kFlashFwCmd));
    m_serialPort->waitForBytesWritten(1000);
    QThread::msleep(400); //wait for restart
}

bool Flasher::checkIfFirmwareIsProtected(void)
{
    qInfo() << "Send is firmware protected command";
    m_serialPort->write(kIsFwProtectedCmd, sizeof(kIsFwProtectedCmd));
    m_serialPort->waitForReadyRead(kSerialTimeoutInMs);
    return checkTrue();
}

bool Flasher::sendEnableFirmwareProtection(void)
{
    bool success;
    qInfo() << "Send enable firmware protected command";
    m_serialPort->write(kEnableFwProtectionCmd, sizeof(kEnableFwProtectionCmd));
    m_serialPort->waitForReadyRead(kSerialTimeoutInMs);
    success = checkAck();
    ShowInfoMsg("Enable readout protection", "Powercyle the board!");
    return success;
}

bool Flasher::sendDisableFirmwareProtection(void)
{
    qInfo() << "Send disable firmware protected command";
    m_serialPort->write(kDisableFwProtectionCmd, sizeof(kDisableFwProtectionCmd));
    m_serialPort->waitForReadyRead(kSerialTimeoutInMs);
    return checkAck();
}

bool Flasher::sendDisconnectCmd()
{
    qInfo() << "Send disconnect command";
    m_serialPort->write(kDisconnectCmd, sizeof(kDisconnectCmd));
    m_serialPort->waitForReadyRead(kSerialTimeoutInMs);
    return checkAck();
}

void Flasher::getVersion(void)
{
    m_serialPort->write(kVersionCmd, sizeof(kVersionCmd));
    m_serialPort->waitForReadyRead(kSerialTimeoutInMs);
    QString gitVersion = m_serialPort->readAll();
    emit textInBrowser(gitVersion);
}

QThread& Flasher::getWorkerThread()
{
    return workerThread;
}

} // namespace flasher
