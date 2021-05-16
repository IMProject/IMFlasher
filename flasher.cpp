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
#include "crc32.h"

#define PACKET_SIZE         256u
#define SIGNATURE_SIZE      64u
#define VERIFY_FLASHER_CMD          "IMFlasher_Verify"
#define CHECK_SINGATURE_CMD         "check_signature"
#define ERASE_CMD                   "erase"
#define GET_BOARD_ID_CMD            "board_id"
#define VERSION_CMD                 "version"
#define FLASH_FW_CMD                "flash_fw"
#define NOT_SECURED_MAGIC_STRING    "NOT_SECURED_MAGIC_STRING_1234567" //32 bytes magic string

#define CRC32_SIZE              4
#define BOARD_ID_SIZE           32
#define BOARD_ID_SIZE_STRING    (BOARD_ID_SIZE * 2) // String use 2 bytes for character
#define KEY_SIZE                32
#define KEY_SIZE_STRING         (KEY_SIZE * 2)      // String use 2 bytes for character
#define SLEEP_TIME              100u                // Take it easy on CPU
#define KEY_FILE_NAME           "keys.json"


void Worker::doWork()
{
    emit flasherLoop();
}

Flasher::Flasher() :
        m_serialPort(new SerialPort),
        m_fileFirmware(new QFile),
        m_keysFile(new QFile),
        m_state(FLASHER_TRY_CONNECT),
        m_firmwareSize(0),
        m_tryOpen(false),
        m_isPortOpen(false),
        m_isSecureBoot(true),
        m_boardId(),
        m_boardKey(),
        m_jsonObject(),
        m_socketClient(new SocketClient),
        m_flashWriteAddress()
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
    runLoop();
}

void Flasher::deinit()
{
    m_serialPort->closeConn();
}

void Flasher::openSerialPort()
{
    m_state = FLASHER_PLUG_USB;
    m_tryOpen = true;
}
void Flasher::closeSerialPort()
{
    m_state = FLASHER_DISCONNECTECTED;
    m_tryOpen = false;
}

void Flasher::actionOpenFirmwareFile()
{
    m_state = FLASHER_OPEN_FILE;
}

void Flasher::loopHandler()
{
    bool success;
    bool manufacturer;

    switch(m_state)
    {

        case(FLASHER_IDLE):
            if(m_fileFirmware->isOpen() && m_serialPort->isOpen()) {
                emit readyToFlashId();
            }
            break;

        case(FLASHER_TRY_CONNECT):
            if(m_tryOpen) {
                manufacturer = m_serialPort->tryOpenPort();

                if(!manufacturer) {
                    m_state = FLASHER_PLUG_USB;
                }
            }

            break;

        case(FLASHER_PLUG_USB):
            emit connectUsbToPc();
            m_serialPort->tryOpenPort();
            if(m_serialPort->isOpen()) {
                m_state = FLASHER_CONNECTECTED;
            }
            break;

        case(FLASHER_CONNECTECTED):

            emit connectedSerialPort();
            if(m_serialPort->isOpen()) {
                getVersion();

                if(m_serialPort->isBootloaderDetected()) {
                    emit isBootloader(true);
                    m_state = FLASHER_BOARD_ID;
                } else {
                    emit isBootloader(false);
                    m_state = FLASHER_IDLE;
                }
            }
            break;

        case(FLASHER_DISCONNECTECTED):
            m_serialPort->closeConn();
            if(!(m_serialPort->isOpen())) {
                emit disconnectedSerialPort();
            }
            break;

        case(FLASHER_BOARD_ID):

            success = collectBoardId();
            if(success) {
                emit textInBrowser("Board ID: " + m_boardId);
                m_state = FLASHER_BOARD_CHECK_REGISTRATION;
            } else {
                emit textInBrowser("Board ID Error. Unplug your board, press disconnect/connect, and plug your board again.");
                m_state = FLASHER_TRY_CONNECT;
            }

        break;

        case(FLASHER_BOARD_CHECK_REGISTRATION):
            success = getBoardKey();
            if(success) {
                emit textInBrowser("Board verified!");
            } else {
                emit textInBrowser("Board is not registered! Press the register button");
            }
            m_state = FLASHER_IDLE;

        break;

        case(FLASHER_GET_BOARD_ID_KEY):
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
            m_state = FLASHER_IDLE;

            break;

        case(FLASHER_OPEN_FILE):
            if(m_filePath.size() > 0) { //check if file path exist
                openFirmwareFile(m_filePath);
                if(m_fileFirmware->isOpen()) {
                    QByteArray fileSize;
                    fileSize.setNum(m_firmwareSize);
                    emit textInBrowser("Board ID: " + m_boardId);
                }
            }
            m_state = FLASHER_IDLE;
            break;

        case(FLASHER_FLASH):
            startFlash();
            m_state = FLASHER_IDLE;
            break;

        case(FLASHER_EXIT):
            break;

        default:
            break;
    }

    workerThread.msleep(SLEEP_TIME);
    runLoop();
}

void  Flasher::startFlashingSlot()
{
    m_state = FLASHER_FLASH;
}

SerialPort* Flasher::getSerialPort()
{
    return m_serialPort;
}

bool Flasher::collectBoardId()
{
    bool success = false;

    if(m_serialPort->isOpen()) {
        success = true;
    }

    if(success) {
        m_serialPort->write(GET_BOARD_ID_CMD, sizeof(GET_BOARD_ID_CMD));
        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT + 200);
        QByteArray data = m_serialPort->readAll();
        QByteArray boardId;
        QByteArray dataCrc;
        uint32_t crc;

        boardId = data.left(BOARD_ID_SIZE);
        dataCrc = data.right(CRC32_SIZE);
        deserialize32((uint8_t*)dataCrc.data(), &crc);

        uint32_t calcCrc = CalculateCRC32((uint8_t*)boardId.data(), BOARD_ID_SIZE, false, false);

        if((boardId.size() == BOARD_ID_SIZE) && (calcCrc == crc)) {
            qInfo() << "BOARD ID";
            m_boardId = boardId.toHex();
            qInfo() << m_boardId;
        } else {
            qInfo() << "Board id error";
            success = false;
        }

    }

    return success;
}

bool Flasher::getBoardKey()
{
    bool success = false;

    //Skip security
    QByteArray magic_string = NOT_SECURED_MAGIC_STRING;
    if(m_boardId != magic_string.toHex()) {

        QJsonValue boardKey = m_jsonObject.value(m_boardId);
        m_boardKey = boardKey.toString();

        if(m_boardKey.size() == KEY_SIZE_STRING) {
            success = true;
        }
    } else {
        m_isSecureBoot = false;
        success = true;
    }

    if(!success) {
        qInfo() << "Board key error";
    }

    return success;
}

void Flasher::saveBoardKeyToFile()
{
    QTextStream streamConfigFile(m_keysFile);
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

        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT);
        success = checkAck();
    } else {
        success = true;
    }

    return success;
}

bool Flasher::startErase()
{
    bool success = false;

    if(m_serialPort->isOpen()) {
        success = true;
    }

    if(success) {
        success = sendKey();
    }

    if(success) {
        //Send flash verifying string
        qInfo() << "ERASE";
        m_serialPort->write(ERASE_CMD, sizeof(ERASE_CMD));
        m_serialPort->waitForReadyRead(5000); //for erase timeout is 5 sec
        success = checkAck();
    }

    return success;
}

void Flasher::startFlash()
{
    bool success = false;

    if(m_serialPort->isOpen()) {
        success = true;
    }

    qint64 dataPosition = 0u;
    QByteArray byteArray = m_fileFirmware->readAll();
    m_fileFirmware->close();

    m_firmwareSize = m_fileFirmware->size() - SIGNATURE_SIZE;
    char *ptrDataSignature = byteArray.data();
    char *ptrDataFirmware = byteArray.data() + SIGNATURE_SIZE;

    if(success) {
        success = sendKey();
    }

    if(success) {
        //Send command for check signature
        qInfo() << "SIGNATURE CMD";
        m_serialPort->write(CHECK_SINGATURE_CMD, sizeof(CHECK_SINGATURE_CMD));
        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT);
        success = checkAck();
    }

    if(success) {
        //Send signature
        qInfo() << "SEND SIGNATURE";
        m_serialPort->write(ptrDataSignature, SIGNATURE_SIZE);
        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT);
        success = checkAck();
    }

    if(success) {
        //Send flash verifying string
        qInfo() << "VERIFY_BOOTLOADER";
        m_serialPort->write(VERIFY_FLASHER_CMD, sizeof(VERIFY_FLASHER_CMD));
        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT);
        success = checkAck();
    }

    if(success) {
        //Send file size
        QByteArray sizeDataString;
        sizeDataString.setNum(m_firmwareSize);
        qInfo() << "SIZE";
        m_serialPort->write(sizeDataString);
        m_serialPort->waitForReadyRead(SERIAL_TIMEOUT);
        success = checkAck();
    }

    if(success) {
        //Send command for erasing
        emit flashingStatusSignal("ERASING");
        qInfo() << "ERASE";
        m_serialPort->write(ERASE_CMD, sizeof(ERASE_CMD));
        m_serialPort->waitForReadyRead(5000); //for erase timeout is 5 sec
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
            m_serialPort->waitForReadyRead(SERIAL_TIMEOUT);
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
                m_serialPort->waitForReadyRead(SERIAL_TIMEOUT);
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
}

bool Flasher::crcCheck(const uint8_t* data, uint32_t size)
{
    bool success;
    QByteArray crcData;
    uint32_t crc = CalculateCRC32(data, size, false, false);
    crcData.setNum(crc);
    m_serialPort->write(crcData);
    m_serialPort->waitForReadyRead(SERIAL_TIMEOUT);
    success = checkAck();

    return success;
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
            qInfo() << "NO ACK";
        } else {
            qInfo() << "ERROR or TIMEOUT";
        }

    } else {
        success = false;
    }

    return success;
}

void Flasher::startRegistrationProcedure()
{
    m_state = FLASHER_GET_BOARD_ID_KEY;
}

void Flasher::openFirmwareFile(QString filePath)
{
    m_fileFirmware->setFileName(filePath);
    if (!m_fileFirmware->open(QIODevice::ReadOnly)) {
      return;
    }
}

void Flasher::deserialize32(uint8_t* buf, uint32_t* value)
{
    *value = (uint32_t)(buf[0] << 24u);
    *value |= (uint32_t)(buf[1] << 16u);
    *value |= (uint32_t)(buf[2] << 8u);
    *value |= (uint32_t)(buf[3] << 0u);
}

void Flasher::setFilePath(QString filePath)
{
    m_filePath = filePath;
}

void Flasher::setFlashWriteAddress(QByteArray flashWriteAddress)
{
    m_flashWriteAddress = flashWriteAddress;
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
    m_serialPort->waitForReadyRead(SERIAL_TIMEOUT);
    QString gitVersion = m_serialPort->readAll();
    emit textInBrowser(gitVersion);
    //qInfo() << gitVersion;
}
