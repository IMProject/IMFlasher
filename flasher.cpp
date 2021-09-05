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
#include "serial_port.h"
#include "crc32.h"
#include <QJsonDocument>
#include <QDebug>
#include <QMessageBox>

const QByteArray Flasher::NOT_SECURED_MAGIC_STRING = "NOT_SECURED_MAGIC_STRING_1234567"; // 32 bytes magic string
const QString Flasher::KEY_FILE_NAME = "keys.json";

// Commands
const char Flasher::VERIFY_FLASHER_CMD[17] = "IMFlasher_Verify";
const char Flasher::ERASE_CMD[6] = "erase";
const char Flasher::VERSION_CMD[8] = "version";
const char Flasher::BOARD_ID_CMD[9] = "board_id";
const char Flasher::FLASH_FW_CMD[9] = "flash_fw";
const char Flasher::ENTER_BL_CMD[9] = "enter_bl";
const char Flasher::IS_FW_PROTECTED_CMD[16] = "is_fw_protected";
const char Flasher::ENABLE_FW_PROTECTION_CMD[21] = "enable_fw_protection";
const char Flasher::DISABLE_FW_PROTECTION_CMD[22] = "disable_fw_protection";
const char Flasher::EXIT_BL_CMD[8] = "exit_bl";
const char Flasher::CHECK_SIGNATURE_CMD[16] = "check_signature";
const char Flasher::DISCONNECT_CMD[11] = "disconnect";

void Worker::doWork()
{
    emit flasherLoop();
}

Flasher::Flasher() :
        m_serialPort(std::make_shared<SerialPort>()),
        m_fileFirmware(std::make_unique<QFile>()),
        m_keysFile(std::make_shared<QFile>()),
        m_socketClient(std::make_unique<SocketClient>()),
        m_state(FlasherStates::INIT),
        m_firmwareSize(0),
        m_tryOpen(false),
        m_isPortOpen(false),
        m_isSecureBoot(true),
        m_boardId(),
        m_boardKey(),
        m_jsonObject(),
        m_isTryConnectStart(false)
{
    m_keysFile->setFileName(KEY_FILE_NAME);
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

void Flasher::deinit()
{
    m_serialPort->closeConn();
}

void Flasher::openSerialPort()
{
    this->setState(FlasherStates::TRY_TO_CONNECT);
    m_tryOpen = true;
}

void Flasher::closeSerialPort()
{
    this->setState(FlasherStates::DISCONNECTED);
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

        case FlasherStates::IDLE:
            if(m_fileFirmware->isOpen() && m_serialPort->isOpen()) {
                emit readyToFlashId();
            }
            break;

        case FlasherStates::INIT:

            if (m_tryOpen) {

                if (!(m_serialPort->tryOpenPort())) {
                    this->setState(FlasherStates::TRY_TO_CONNECT);
                }
            }

            break;

        case FlasherStates::TRY_TO_CONNECT:
        {
            bool isConnected = false;

            if (m_isTryConnectStart) {

                if (m_serialPort->tryOpenPort()) {

                    if (m_serialPort->isOpen()) {
                        this->setState(FlasherStates::CONNECTED);
                        m_isTryConnectStart = false;
                        isConnected = true;

                    } else {
                        emit showStatusMsg("Trying to connect!");
                    }

                } else {
                    emit showStatusMsg("Trying to connect!");
                }

                if ((!isConnected) && (m_timerTryConnect.hasExpired(TRY_TO_CONNECT_TIMEOUT_IN_MS))) {
                    emit failedToConnect();
                    this->setState(FlasherStates::INIT);
                    m_tryOpen = false;
                    m_isTryConnectStart = false;
                }

            } else {
                m_isTryConnectStart = true;
                m_timerTryConnect.start();
            }

            break;
        }

        case FlasherStates::CONNECTED:

            emit showStatusMsg("Connected");
            if(m_serialPort->isOpen()) {
                getVersion();

                if(m_serialPort->isBootloaderDetected()) {
                    emit isBootloader(true);
                    this->setState(FlasherStates::BOARD_ID);
                } else {
                    emit isBootloader(false);
                    this->setState(FlasherStates::IDLE);
                }
            }
            break;

        case FlasherStates::DISCONNECTED:
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

            this->setState(FlasherStates::IDLE);

            break;
        }

        case FlasherStates::BOARD_ID:

            success = collectBoardId();
            if(success) {
                emit textInBrowser("Board ID: " + m_boardId);
                this->setState(FlasherStates::BOARD_CHECK_REGISTRATION);
            } else {
                emit textInBrowser("Board ID Error. Unplug your board, press disconnect/connect, and plug your board again.");
                this->setState(FlasherStates::INIT);
            }

            break;

        case FlasherStates::BOARD_CHECK_REGISTRATION:
            success = getBoardKey();
            if(success) {
                emit textInBrowser("Board verified!");
            } else {
                emit textInBrowser("Board is not registered! Press the register button");
            }

            emit isReadProtectionEnabled(checkIfFirmwareIsProtected());

            this->setState(FlasherStates::IDLE);

            break;

        case FlasherStates::GET_BOARD_ID_KEY:
            if(m_boardId.size() != BOARD_ID_SIZE_STRING) {
                emit textInBrowser("First connect board to get board id!");

            }   else {
                success = getBoardKeyFromServer();
                if(success && (m_boardKey.size() != KEY_SIZE_STRING)) {
                    emit textInBrowser("This Board ID is not verified. Check www.imsecure.xyz");
                }
                else if(!success) {
                    emit textInBrowser("Server error, please contact the administrator!");
                } else {
                    emit textInBrowser("Key: " + m_boardKey);
                    saveBoardKeyToFile();
                }
            }

            this->setState(FlasherStates::IDLE);
            break;

        case FlasherStates::OPEN_FILE:
            if(m_filePath.size() > 0) { //check if file path exist
                openFirmwareFile(m_filePath);
                if(m_fileFirmware->isOpen()) {
                    QByteArray fileSize;
                    fileSize.setNum(m_firmwareSize);
                    emit textInBrowser("Board ID: " + m_boardId);
                }
            }

            this->setState(FlasherStates::IDLE);
            break;

        case FlasherStates::FLASH:
        {
            std::tuple<bool, QString, QString> flashingInfo = this->startFlash();
            this->showInfoMsg(std::get<1>(flashingInfo), std::get<2>(flashingInfo));
            emit clearProgress();

            if (std::get<0>(flashingInfo)) {
                m_serialPort->closeConn();
                this->setState(FlasherStates::TRY_TO_CONNECT);

            } else {
                this->setState(FlasherStates::IDLE);
            }
            break;
        }

        default:
            break;
    }

    workerThread.msleep(THREAD_SLEEP_TIME_IN_MS);
    emit runLoop();
}

std::shared_ptr<SerialPort> Flasher::getSerialPort() const
{
    return m_serialPort;
}

bool Flasher::collectBoardId()
{
    bool success = false;

    if (m_serialPort->isOpen()) {
        m_serialPort->write(BOARD_ID_CMD, sizeof(BOARD_ID_CMD));
        m_serialPort->waitForReadyRead(COLLECT_BOARD_ID_SERIAL_TIMEOUT_IN_MS);
        QByteArray data = m_serialPort->readAll();
        QByteArray boardId;
        QByteArray dataCrc;
        uint32_t crc;

        boardId = data.left(BOARD_ID_SIZE);
        dataCrc = data.right(CRC32_SIZE);
        deserialize32((uint8_t*)dataCrc.data(), &crc);

        uint32_t calcCrc = CalculateCRC32((uint8_t*)boardId.data(), static_cast<uint32_t>(BOARD_ID_SIZE), false, false);

        if((boardId.size() == BOARD_ID_SIZE) && (calcCrc == crc)) {
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

    //Skip security
    QByteArray magic_string = NOT_SECURED_MAGIC_STRING;
    if (m_boardId != magic_string.toHex()) {

        QJsonValue boardKey = m_jsonObject.value(m_boardId);
        m_boardKey = boardKey.toString();

        if (m_boardKey.size() == KEY_SIZE_STRING) {

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
    success = m_socketClient->dataTransfer(dataIn, dataOut);
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

        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
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
            m_serialPort->write(ERASE_CMD, sizeof(ERASE_CMD));
            m_serialPort->waitForReadyRead(ERASE_TIMEOUT_IN_MS);
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
    QByteArray byteArray = m_fileFirmware->readAll();

    m_fileFirmware->close();
    m_firmwareSize = m_fileFirmware->size() - SIGNATURE_SIZE;

    char *ptrDataSignature = byteArray.data();
    char *ptrDataFirmware = byteArray.data() + SIGNATURE_SIZE;

    if (m_serialPort->isOpen()) {
        success = sendKey();

    } else {
        title = "Error";
        description = "Serial port is not opened";
    }

    if (success) {
        qInfo() << "Check signature";
        m_serialPort->write(CHECK_SIGNATURE_CMD, sizeof(CHECK_SIGNATURE_CMD));
        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
        success = checkAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Check signature problem";
        }
    }

    if (success) {
        qInfo() << "Send signature";
        m_serialPort->write(ptrDataSignature, SIGNATURE_SIZE);
        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
        success = checkAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Send signature problem";
        }
    }

    if (success) {
        qInfo() << "Verify flasher";
        m_serialPort->write(VERIFY_FLASHER_CMD, sizeof(VERIFY_FLASHER_CMD));
        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
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
        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
        success = checkAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Send file size problem";
        }
    }

    if (success) {
        qInfo() << "Erase";
        m_serialPort->write(ERASE_CMD, sizeof(ERASE_CMD));
        m_serialPort->waitForReadyRead(ERASE_TIMEOUT_IN_MS);
        success = checkAck();

        if (!success) {
            title = "Flashing process failed";
            description = "Erasing problem";
        }
    }

    // Send file in packages
    if (success) {
        qint64 packetsNumber = (m_firmwareSize / PACKET_SIZE);
        for (dataPosition = 0; dataPosition < packetsNumber; ++dataPosition) {
            char* ptrDataPosition;
            ptrDataPosition = ptrDataFirmware + (dataPosition * PACKET_SIZE);

            m_serialPort->write(ptrDataPosition, PACKET_SIZE);
            m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
            emit updateProgress((dataPosition + 1u) * PACKET_SIZE, m_firmwareSize);
            success = checkAck();

            if (!success) {
                title = "Flashing process failed";
                description = "Problem with flashing";
                break;
            }
        }

        if (success) {
            qint64 restSize = m_firmwareSize % PACKET_SIZE;

            if (restSize > 0) {
                m_serialPort->write(ptrDataFirmware + (dataPosition * PACKET_SIZE), restSize);
                m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
                emit updateProgress(dataPosition * PACKET_SIZE + restSize, m_firmwareSize);
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
    uint32_t crc = CalculateCRC32(data, size, false, false);
    crcData.setNum(crc);
    m_serialPort->write(crcData);
    m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);

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
    m_fileFirmware->setFileName(filePath);

    return m_fileFirmware->open(QIODevice::ReadOnly);
}

void Flasher::deserialize32(uint8_t* buf, uint32_t* value)
{
    *value = (uint32_t)(buf[0] << 24u);
    *value |= (uint32_t)(buf[1] << 16u);
    *value |= (uint32_t)(buf[2] << 8u);
    *value |= (uint32_t)(buf[3] << 0u);
}

void Flasher::setFilePath(const QString& filePath)
{
    m_filePath = filePath;
}

void Flasher::setState(const FlasherStates& state)
{
    m_state = state;
}


bool Flasher::sendEnterBootloaderCommand(void)
{
    bool success;
    qInfo() << "Send enter bl command";
    m_serialPort->write(ENTER_BL_CMD, sizeof(ENTER_BL_CMD));
    m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
    success = checkAck();

    if(success) {
        QThread::msleep(400); //wait for restart
    }

    return success;
}

bool Flasher::sendExitBootlaoderCommand(void)
{
    bool success;
    qInfo() << "Send exit bl command";
    m_serialPort->write(EXIT_BL_CMD, sizeof(EXIT_BL_CMD));
    m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
    success = checkAck();
    return success;
}

void Flasher::sendFlashCommand(void)
{
    qInfo() << "Send flash command";
    m_serialPort->write(FLASH_FW_CMD, sizeof(FLASH_FW_CMD));
    m_serialPort->waitForBytesWritten(1000);
    QThread::msleep(400); //wait for restart
}

bool Flasher::checkIfFirmwareIsProtected(void)
{
    bool success;
    qInfo() << "Send is firmware protected command";
    m_serialPort->write(IS_FW_PROTECTED_CMD, sizeof(IS_FW_PROTECTED_CMD));
    m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
    success = checkTrue();
    return success;
}

bool Flasher::sendEnableFirmwareProtection(void)
{
    bool success;
    qInfo() << "Send enable firmware protected command";
    m_serialPort->write(ENABLE_FW_PROTECTION_CMD, sizeof(ENABLE_FW_PROTECTION_CMD));
    m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
    success = checkAck();
    showInfoMsg("Enable readout protection", "Powercyle the board!");
    return success;
}

bool Flasher::sendDisableFirmwareProtection(void)
{
        bool success;
        qInfo() << "Send disable firmware protected command";
        m_serialPort->write(DISABLE_FW_PROTECTION_CMD, sizeof(DISABLE_FW_PROTECTION_CMD));
        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
        success = checkAck();
        return success;
}

void Flasher::getVersion(void)
{
    m_serialPort->write(VERSION_CMD, sizeof(VERSION_CMD));
    m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
    QString gitVersion = m_serialPort->readAll();
    emit textInBrowser(gitVersion);
    //qInfo() << gitVersion;
}

QThread& Flasher::getWorkerThread()
{
    return workerThread;
}

void Flasher::showInfoMsg(const QString& title, const QString& description)
{
    QMessageBox msgBox;
    msgBox.setText(title);
    msgBox.setInformativeText(description);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

bool Flasher::sendDisconnectCmd()
{
    m_serialPort->write(DISCONNECT_CMD, sizeof(DISCONNECT_CMD));
    m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
    return checkAck();
}
