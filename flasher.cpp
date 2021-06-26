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

const QByteArray Flasher::NOT_SECURED_MAGIC_STRING = "NOT_SECURED_MAGIC_STRING_1234567"; // 32 bytes magic string
const QString Flasher::KEY_FILE_NAME = "keys.json";

// Commands
const char Flasher::VERIFY_FLASHER_CMD[17] = "IMFlasher_Verify";
const char Flasher::ERASE_CMD[6] = "erase";
const char Flasher::VERSION_CMD[8] = "version";
const char Flasher::BOARD_ID_CMD[9] = "board_id";
const char Flasher::FLASH_FW_CMD[9] = "flash_fw";
const char Flasher::CHECK_SIGNATURE_CMD[16] = "check_signature";

void Worker::doWork()
{
    emit flasherLoop();
}

Flasher::Flasher() :
        m_serialPort(std::make_shared<SerialPort>()),
        m_fileFirmware(std::make_unique<QFile>()),
        m_keysFile(std::make_shared<QFile>()),
        m_socketClient(std::make_unique<SocketClient>()),
        m_state(FlasherStates::TRY_CONNECT),
        m_firmwareSize(0),
        m_tryOpen(false),
        m_isPortOpen(false),
        m_isSecureBoot(true),
        m_boardId(),
        m_boardKey(),
        m_jsonObject()
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
    this->setState(FlasherStates::PLUG_USB);
    m_tryOpen = true;
}

void Flasher::closeSerialPort()
{
    this->setState(FlasherStates::DISCONNECTED);
    m_tryOpen = false;
}

void Flasher::loopHandler()
{
    bool success;
    bool manufacturer;

    switch (m_state)
    {

        case FlasherStates::IDLE:
            if(m_fileFirmware->isOpen() && m_serialPort->isOpen()) {
                emit readyToFlashId();
            }
            break;

        case FlasherStates::TRY_CONNECT:
            if(m_tryOpen) {
                manufacturer = m_serialPort->tryOpenPort();

                if(!manufacturer) {
                    this->setState(FlasherStates::PLUG_USB);
                }
            }

            break;

        case FlasherStates::PLUG_USB:
            emit connectUsbToPc();
            m_serialPort->tryOpenPort();
            if(m_serialPort->isOpen()) {
                this->setState(FlasherStates::CONNECTED);
            }
            break;

        case FlasherStates::CONNECTED:

            emit connectedSerialPort();
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
            m_serialPort->closeConn();
            if(!(m_serialPort->isOpen())) {
                emit disconnectedSerialPort();
            }
            break;

        case FlasherStates::BOARD_ID:

            success = collectBoardId();
            if(success) {
                emit textInBrowser("Board ID: " + m_boardId);
                this->setState(FlasherStates::BOARD_CHECK_REGISTRATION);
            } else {
                emit textInBrowser("Board ID Error. Unplug your board, press disconnect/connect, and plug your board again.");
                this->setState(FlasherStates::TRY_CONNECT);
            }

            break;

        case FlasherStates::BOARD_CHECK_REGISTRATION:
            success = getBoardKey();
            if(success) {
                emit textInBrowser("Board verified!");
            } else {
                emit textInBrowser("Board is not registered! Press the register button");
            }
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
            this->startFlash();
            this->setState(FlasherStates::IDLE);
            break;

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

bool Flasher::startFlash()
{
    bool success = false;
    qint64 dataPosition = 0;
    QByteArray byteArray = m_fileFirmware->readAll();

    m_fileFirmware->close();
    m_firmwareSize = m_fileFirmware->size() - SIGNATURE_SIZE;

    char *ptrDataSignature = byteArray.data();
    char *ptrDataFirmware = byteArray.data() + SIGNATURE_SIZE;

    if (m_serialPort->isOpen()) {
        success = sendKey();
    }

    if(success) {
        //Send command for check signature
        qInfo() << "SIGNATURE CMD";
        m_serialPort->write(CHECK_SIGNATURE_CMD, sizeof(CHECK_SIGNATURE_CMD));
        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
        success = checkAck();
    }

    if(success) {
        //Send signature
        qInfo() << "SEND SIGNATURE";
        m_serialPort->write(ptrDataSignature, SIGNATURE_SIZE);
        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
        success = checkAck();
    }

    if(success) {
        //Send flash verifying string
        qInfo() << "VERIFY_BOOTLOADER";
        m_serialPort->write(VERIFY_FLASHER_CMD, sizeof(VERIFY_FLASHER_CMD));
        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
        success = checkAck();
    }

    if(success) {
        //Send file size
        QByteArray sizeDataString;
        sizeDataString.setNum(m_firmwareSize);
        qInfo() << "SIZE";
        m_serialPort->write(sizeDataString);
        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
        success = checkAck();
    }

    if(success) {
        //Send command for erasing
        emit flashingStatusSignal("ERASING");
        qInfo() << "ERASE";
        m_serialPort->write(ERASE_CMD, sizeof(ERASE_CMD));
        m_serialPort->waitForReadyRead(ERASE_TIMEOUT_IN_MS);
        success = checkAck();
    }

    //Send file in packages
    if(success) {
        emit flashingStatusSignal("FLASHING");
        qint64 packetsNumber = (m_firmwareSize / PACKET_SIZE);
        for( dataPosition = 0; dataPosition < packetsNumber; dataPosition++) {
            char* ptrDataPosition;
            ptrDataPosition = ptrDataFirmware + (dataPosition * PACKET_SIZE);

            m_serialPort->write(ptrDataPosition, PACKET_SIZE);
            m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
            emit updateProgress((dataPosition + 1u) * PACKET_SIZE, m_firmwareSize);

            success = checkAck();
            if(!success) {
                break;
            }

        }

        if(success) {
            qint64 restSize = m_firmwareSize % PACKET_SIZE;

            if(restSize > 0u) {
                m_serialPort->write(ptrDataFirmware + (dataPosition * PACKET_SIZE), restSize);
                m_serialPort->waitForReadyRead(SERIAL_TIMEOUT_IN_MS);
                success = checkAck();
                emit updateProgress(dataPosition * PACKET_SIZE + restSize, m_firmwareSize);
            }
        }

        if(success) {
            //CRC
            qInfo() << "CRC";
            crcCheck((const uint8_t*)ptrDataFirmware, m_firmwareSize);
            if(success) {
                emit flashingStatusSignal("DONE");
            } else {
                emit flashingStatusSignal("ERROR");
            }
        }
    }

    return success;
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
    if(data.size() > 2) {

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
        success = false;
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

void Flasher::sendFlashCommandToApp(void)
{
    m_serialPort->write(FLASH_FW_CMD, sizeof(FLASH_FW_CMD));
    m_serialPort->waitForBytesWritten(1000);
    QThread::msleep(400); //wait for restart
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
