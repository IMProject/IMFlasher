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

#ifndef FLASHER_H
#define FLASHER_H

#include <QJsonObject>
#include <QThread>
#include <QFile>
#include <QElapsedTimer>
#include <socket.h>

enum class FlasherStates {
    IDLE,
    INIT,
    TRY_TO_CONNECT,
    CONNECTED,
    DISCONNECTED,
    BOARD_ID,
    GET_BOARD_ID_KEY,
    BOARD_CHECK_REGISTRATION,       //!< Check if file exist and size of key
    OPEN_FILE,
    FLASH
};

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
    QThread workerThread;

public:
    explicit Flasher();
    ~Flasher();

    void init();
    void deinit();
    std::shared_ptr<SerialPort> getSerialPort() const;
    bool collectBoardId();
    bool getBoardKey();
    void saveBoardKeyToFile();
    bool getBoardKeyFromServer();
    bool sendKey();
    bool startErase();
    std::tuple<bool, QString, QString> startFlash();
    bool crcCheck(const uint8_t* data, uint32_t size);
    bool checkAck();
    bool openFirmwareFile(const QString& filePath);
    void openSerialPort();
    void closeSerialPort();
    void deserialize32(uint8_t* buf, uint32_t* value);
    void setFilePath(const QString& filePath);
    void setState(const FlasherStates& state);
    void sendFlashCommandToApp(void); //enter in bootlaoder
    void getVersion(void);
    QThread& getWorkerThread();

signals:
    void updateProgress(const qint64& dataPosition, const qint64& firmwareSize);
    void clearProgress();
    void connectUsbToPc(const QString& text);
    void connectedSerialPort();
    void disconnectedSerialPort();
    void failedToConnect();
    void openSerialPortInThread();
    void closeSerialPortInThread();
    void runLoop();
    void textInBrowser(const QString& boardId);
    void isBootloader(const bool& bootloader);
    void readyToFlashId();

public slots:
    void loopHandler();

private:
    void showInfoMsgAtTheEndOfFlashing(const QString& title, const QString& description);

    std::shared_ptr<SerialPort> m_serialPort;
    std::unique_ptr<QFile> m_fileFirmware;
    std::shared_ptr<QFile> m_keysFile;
    std::unique_ptr<SocketClient> m_socketClient;

    FlasherStates m_state;
    qint64 m_firmwareSize;
    bool m_tryOpen;
    bool m_isPortOpen;
    bool m_isSecureBoot;
    QString m_boardId;
    QString m_boardKey;
    QJsonObject m_jsonObject;
    QString m_filePath;
    bool m_isTryConnectStart;
    QElapsedTimer m_timerTryConnect;

    static constexpr qint64 SIGNATURE_SIZE {64};
    static constexpr int ERASE_TIMEOUT_IN_MS {5000};
    static constexpr qint64 PACKET_SIZE {256};
    static constexpr unsigned long THREAD_SLEEP_TIME_IN_MS {100U};
    static constexpr int SERIAL_TIMEOUT_IN_MS {100};
    static constexpr int COLLECT_BOARD_ID_SERIAL_TIMEOUT_IN_MS {300};
    static constexpr int CRC32_SIZE {4};
    static constexpr int BOARD_ID_SIZE {32};
    static constexpr int BOARD_ID_SIZE_STRING {BOARD_ID_SIZE * 2};
    static constexpr int KEY_SIZE {32};
    static constexpr int KEY_SIZE_STRING {KEY_SIZE * 2};
    static constexpr int TRY_TO_CONNECT_TIMEOUT_IN_MS {20000};

    static const QString KEY_FILE_NAME;
    static const QByteArray NOT_SECURED_MAGIC_STRING;

    // Commands
    static const char VERIFY_FLASHER_CMD[17];
    static const char ERASE_CMD[6];
    static const char VERSION_CMD[8];
    static const char BOARD_ID_CMD[9];
    static const char FLASH_FW_CMD[9];
    static const char CHECK_SIGNATURE_CMD[16];
};

#endif // FLASHER_H
