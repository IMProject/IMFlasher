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

#ifndef FLASHER_H_
#define FLASHER_H_

#include <QElapsedTimer>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>

#include "flasher_states.h"
#include "flashing_info.h"
#include "serial_port.h"

namespace socket {

class SocketClient;

} // namespace socket

namespace file_downloader {

class FileDownloader;

} // namespace file_downloader

namespace flasher {

/*!
 * \brief The Flasher class, main class used to handle flashing process
 */
class Flasher : public QObject {

    Q_OBJECT

  public:
    /*!
     * \brief Flasher default constructor
     */
    Flasher();

    /*!
     * \brief Flasher destructor
     */
    ~Flasher();

    /*!
     * \brief Method used to collect board ID
     * \return True if board ID is successfully collected, false otherwise
     */
    bool CollectBoardId();

    /*!
     * \brief Method used to collect board information
     * \return True if board information is successfully collected, false otherwise
     */
    bool CollectBoardInfo();

    /*!
     * \brief Method used to flash from the console
     * \return Flashing information
     */
    FlashingInfo ConsoleFlash();

    /*!
     * \brief Send erase command while flashing
     * \return Flashing information
     */
    FlashingInfo Erase();

    /*!
     * \brief Flasher initialization
     */
    void Init();

    /*!
     * \brief Check if bootloader is detected
     * \return True if bootloader is detected, false otherwise
     */
    bool IsBootloaderDetected() const;

    /*!
     * \brief Check if read protection is enable
     * \return True if read protection is enabled, false otherwise
     */
    bool IsReadProtectionEnabled() const;

    /*!
     * \brief Open firmware file
     * \param file_path - File(firmware) path
     * \return True if file is successfully opened, false otherwise
     */
    bool OpenFirmwareFile(const QString& file_path);

    /*!
     * \brief Send enter bootloader command
     * \return True if command is successfully sent, false otherwise
     */
    bool SendEnterBootloaderCommand();

    /*!
     * \brief Send flash command
     */
    void SendFlashCommand();

    /*!
     * \brief Set local file content
     * \return True if local file content is successully set, false otherwise
     */
    bool SetLocalFileContent();

    /*!
     * \brief Set flasher state
     * \param state - Flasher state that will be set
     */
    void SetState(const FlasherStates& state);

    /*!
     * \brief Set selected firmware version
     * \param selected_firmware_version - Firmware version that will be set
     */
    void SetSelectedFirmwareVersion(const QString& selected_firmware_version);

    /*!
     * \brief Try to connect to the board over console
     */
    void TryToConnectConsole();

    /*!
    * \brief Update progress bar
    * \param sent_size - Size that is sent
    * \param firmware_size - Firmware size
    */
   void UpdateProgressBar(const quint64& sent_size, const quint64& firmware_size);

  signals:
    /*!
     * \brief Update progress bar signal
     * \param progress_percentage - value for the update in percentage
     */
    void UpdateProgressBarSignal(const qint8& progress_percentage);

    /*!
     * \brief Clear progress signal
     */
    void ClearProgress();

    /*!
     * \brief Show status message signal
     * \param text - Text that will be displayed
     */
    void ShowStatusMsg(const QString& text);

    /*!
     * \brief Clear status message signal
     */
    void ClearStatusMsg();

    /*!
     * \brief Failed to connect signal
     */
    void FailedToConnect();

    /*!
     * \brief RunLoop signal
     */
    void RunLoop();

    /*!
     * \brief Show text in browser signal
     * \param text - Text that will be displayed
     */
    void ShowTextInBrowser(const QString& text);

    /*!
     * \brief Clear text in browser signal
     */
    void ClearTextInBrowser();

    /*!
     * \brief Set buttons signal
     * \param isBootloader - Flag that determines if bootloader is entered, and therefore it determines button text
     */
    void SetButtons(const bool& isBootloader);

    /*!
     * \brief Set read protection button text signal
     * \param isEnabled - Flag that determines button text
     */
    void SetReadProtectionButtonText(const bool& isEnabled);

    /*!
     * \brief Disable all buttons signal
     */
    void DisableAllButtons();

    /*!
     * \brief Enable connect button, connect button is enabled and disconnect button disabled
     */
    void EnableConnectButton();

    /*!
     * \brief Enable disconnect button signal, disconnect button is enabled and connect button disabled
     */
    void EnableDisconnectButton();

    /*!
     * \brief Enable load button signal
     */
    void EnableLoadButton();

    /*!
     * \brief Set firmware list signal
     * \param product_info - Json array that contains product information
     */
    void SetFirmwareList(const QJsonArray& product_info);

  public slots:
    /*!
     * \brief LoopHandler slot
     */
    void LoopHandler();

    /*!
     * \brief FileDownloaded slot, file downloading is finished
     */
    void FileDownloaded();

    /*!
     * \brief DownloadProgress slot
     * \param bytes_received - Number of received bytes
     * \param bytes_total - Total number of bytes
     */
    void DownloadProgress(const qint64& bytes_received, const qint64& bytes_total);

    /*!
     * \brief HandleSerialPortError slot
     * \param error - Serial port error
     */
    void HandleSerialPortError(QSerialPort::SerialPortError error);

  private:
    QString board_id_;                                                      //!< Board ID
    QJsonObject board_info_;                                                //!< Board information
    QJsonObject bl_version_;                                                //!< Bootloader version
    QJsonObject fw_version_;                                                //!< Firmware version
    QJsonArray product_info_;                                               //!< Product information
    QString selected_firmware_version_;                                     //!< Selected firmware version
    QFile config_file_;                                                     //!< Configuration file
    QFile firmware_file_;                                                   //!< Firmware file
    qint64 signature_size_{0};                                              //!< Firmware signature size
    quint8 last_progress_percentage_{0};                                    //!< Last progress percentage
    bool is_bootloader_ {false};                                            //!< Is bootloader detected flag
    bool is_bootloader_expected_ {false};                                   //!< Is bootloader expected after board reset
    bool is_read_protection_enabled_ {false};                               //!< Is read protection enabled flag
    bool is_timer_started_ {false};                                         //!< Is timer started flag
    bool is_firmware_downloaded_{false};                                    //!< Is firmware downloaded flag
    bool is_download_success_{false};                                       //!< Is download successfully done
    bool is_signature_warning_enabled_{false};                              //!< Is signature warning enabled
    QByteArray file_content_;                                               //!< File content
    communication::SerialPort serial_port_;                                 //!< Serial port object
    std::shared_ptr<socket::SocketClient> socket_client_;                   //!< Shared pointer to SocketClient object
    std::unique_ptr<file_downloader::FileDownloader> file_downloader_;      //!< Pointer to FileDownloader object
    FlasherStates state_ {FlasherStates::kIdle};                            //!< Flasher state
    QElapsedTimer timer_;                                                   //!< Timer
    QThread worker_thread_;                                                 //!< Worker thread

    /*!
     * \brief Method is used to check if acknowledge is received from bootloader side
     * \return True if ACK is recevied, false otherwise
     */
    bool CheckAck();

    /*!
     * \brief Method used to check signature
     * \return Flashing info structure
     */
    FlashingInfo CheckSignature();

    /*!
     * \brief CheckTrue
     * \return True if TRUE is received, false otherwise
     */
    bool CheckTrue();

    /*!
     * \brief Method used to check CRC
     * \return Flashing info structure
     */
    FlashingInfo CrcCheck();

    /*!
     * \brief Method used to create default configuration file
     */
    void CreateDefaultConfigFile();

    /*!
     * \brief Method used to donwload firmware file from URL
     */
    void DownloadFirmwareFromUrl();

    /*!
     * \brief Method used to perform flash process
     * \return Flashing info structure
     */
    FlashingInfo Flash();

    /*!
     * \brief Method used to get version of bootloader/firmware
     */
    void GetVersion();

    /*!
     * \brief Method used to get version of bootloader/firmware in JSON format
     * \param out_json_object - Json object where version will be saved
     * \return True if version is fetched successfully, false otherwise
     */
    bool GetVersionJson(QJsonObject& out_json_object);

    /*!
     * \brief Method used to check if firmware is protected or not
     * \return True if firmware is protected, false otherwise
     */
    bool IsFirmwareProtected();

    /*!
     * \brief Method used to open configuration file
     * \param json_document - Json document where configuration file is saved
     * \return True if configuration file is successfully opened, false otherwise
     */
    bool OpenConfigFile(QJsonDocument& json_document);

    /*!
     * \brief Method used to read message with CRC
     * \param in_data - Pointer to the input data
     * \param length - Data length
     * \param timeout_ms - Timeout for reading from serial port [ms]
     * \param out_data - Byte array of output data
     * \return True if message with CRC is successfully read, false otherwise
     */
    bool ReadMessageWithCrc(const char *in_data, qint64 length, int timeout_ms, QByteArray& out_data);

    /*!
     * \brief Method used to reconnect to board
     */
    void ReconnectingToBoard();

    /*!
     * \brief Method used to send file size to the bootloader
     * \return Flashing info structure
     */
    FlashingInfo SendFileSize();

    /*!
     * \brief Method used to send message to the bootloader/firmware
     * \param data - Pointer to data that will be sent
     * \param length - Data length
     * \param timeout_ms - Timeout for reading from serial port [ms]
     * \return True if message is sent correctly, false otherwise
     */
    bool SendMessage(const char *data, qint64 length, int timeout_ms);

    /*!
     * \brief Method used to send signature to the bootloader
     * \return Flashing info structure
     */
    FlashingInfo SendSignature();

    /*!
     * \brief Method used to verify flasher
     * \return Flashing info structure
     */
    FlashingInfo VerifyFlasher();

    /*!
     * \brief Method used to try to connect to the board
     */
    void TryToConnect();
};

} // namespace flasher
#endif // FLASHER_H_
