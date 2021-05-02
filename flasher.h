/*
 * Copyright (C) 2021 Igor Misic, <igy1000mb@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 *
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FLASHER_H
#define FLASHER_H

#include "serial_port.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <socket.h>

typedef enum flahser_states
{
    FLASHER_IDLE,
    FLASHER_TRY_CONNECT,
    FLASHER_PLUG_USB,
    FLASHER_CONNECTECTED,
    FLASHER_DISCONNECTECTED,
    FLASHER_DETECT_SW_TYPE,
    FLASHER_BOARD_ID,
    FLASHER_GET_BOARD_ID_KEY,
    FLASHER_BOARD_CHECK_REGISTRATION,       //!< Check if file exist and size of key
    FLASHER_OPEN_FILE,
    FLASHER_FLASH,
    FLASHER_EXIT
}flahser_states_t;

class SerialPort;

class Worker : public QObject
{
    Q_OBJECT

public slots:
    void doWork();

signals:
    void flasherLoop();
};


class Flasher : public QObject {

Q_OBJECT
    QThread serialPortThread;
    QThread workerThread;

public:
    explicit Flasher();
    ~Flasher(){}

    void init();
    void deinit();
    SerialPort* getSerialPort();
    bool collectBoardId();
    bool getBoardKey();
    void saveBoardKeyToFile();
    bool getBoardKeyFromServer();
    bool sendKey();
    bool startErase();
    void startFlash();
    bool crcCheck(const uint8_t* data, uint32_t size);
    bool checkAck();
    void startRegistrationProcedure();
    void openFirmwareFile(QString filePath);
    void openSerialPort();
    void closeSerialPort();
    void actionOpenFirmwareFile();
    void deserialize32(uint8_t* buf, uint32_t* value);
    void setFilePath(QString filePath);
    void setFlashWriteAddress(QByteArray setFlashWriteAddress);
    void sendFlashCommandToApp(void); //enter in bootlaoder
    void detectSoftwareType(void);
    void getVersion(void);
    bool isBootloaderDetected(void);

signals:
    void updateProgress(qint64 dataPosition, qint64 firmwareSize);
    void connectUsbToPc();
    void connectedSerialPort();
    void disconnectedSerialPort();
    void openSerialPortInThread();
    void closeSerialPortInThread();
    void runLoop();
    void textInBrowser(QString boardId);
    void isBootloader(bool bootloader);
    void readyToFlashId();
    void flashingStatusSignal(QString status);

public slots:
    void loopHandler();
    void startFlashingSlot();

private:

    SerialPort* m_serialPort = nullptr;;
    QFile* m_fileFirmware = nullptr;
    QFile* m_keysFile = nullptr;

    flahser_states_t m_state;
    qint64 m_firmwareSize = 0;
    bool m_tryOpen;
    bool m_isPortOpen;
    bool m_isSecureBoot;
    QString m_boardId;
    QString m_boardKey;
    QJsonObject m_jsonObject;
    QString m_filePath;
    SocketClient* m_socketClient = nullptr;
    bool m_isBootlaoder;

    QByteArray m_flashWriteAddress;
};

#endif // FLASHER_H
