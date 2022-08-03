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

#ifndef SOCKET_H_
#define SOCKET_H_

#include <QByteArray>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonArray>

namespace socket {

namespace {

const QString kHeaderClientBoardInfo{"client_board_info"};
const QString kHeaderClientProductInfo{"client_product_info"};
const QString kHeaderServerProductInfo{"server_product_info"};
const QString kHeaderClientDownloadFile{"client_download_file"};
const QString kHeaderServerDownloadFile{"server_download_file"};
const QString kHeaderClientRequestData{"client_request_data"};

} // namespace

/*!
 * \brief The SocketClient class, contains socket client information
 */
class SocketClient : public QTcpSocket {

    Q_OBJECT

  public:
    /*!
     * \brief SocketClient constructor
     * \param servers_array - Json array with servers config
     */
    explicit SocketClient(QJsonArray servers_array);

    /*!
     * \brief SocketClient destructor
     */
    virtual ~SocketClient();

    /*!
     * \brief Sending information about board to the server
     * \param board_info - Json object with board info for server
     * \param bl_version - Json object with bootloader git version
     * \param fw_version - Json object with firmware git version
     * \return True if board info is successfully send, false otherwise
     */
    virtual bool SendBoardInfo(QJsonObject board_info, QJsonObject bl_version, QJsonObject fw_version);

    /*!
     * \brief Receive product info for given board info from the server
     * \param board_info - Json object with board info from server
     * \param product_info - Json array with product info from server
     * \return
     */
    virtual bool ReceiveProductInfo(QJsonObject board_info, QJsonArray& product_info);

    /*!
     * \brief Download file from the server
     * \param board_info - Json object with board info from server
     * \param file_version - File version to download
     * \param file_content - Reference to file_content to download
     * \return
     */
    virtual bool DownloadFile(QJsonObject board_info, QString file_version, QByteArray& file_content);

  private:
    /*!
     * \brief Method use to perform connect action
     * \return True if connect is performed successfully, false otherwise
     */
    virtual bool Connect();

    /*!
     * \brief Method used to perform disconnect action
     * \return True if disconnect is performed successfuly, false otherwise
     */
    virtual bool Disconnect();

    /*!
     * \brief Method used to perform authentication
     * \return True if authentication is done successfully, false otherwise
     */
    virtual bool Authentication();

    /*!
     * \brief Method used to read all received data
     * \param data_out - Byte array where received data will be stored
     * \return True if all data is read successfully, false otherwise
     */
    virtual bool ReadAll(QByteArray& data_out);

    /*!
     * \brief Method used to send QByteArray data
     * \param in_data - Byte array input data
     * \return True if data is successfully sent, false otherwise
     */
    virtual bool SendDataWithAck(const QByteArray& in_data);

    /*!
     * \brief Method used to send QJsonObject
     * \param json_objec - QJsonObject input data
     * \return True if data is successfully sent, false otherwise
     */
    virtual bool SendQJsonObject(const QJsonObject& json_objec);

    /*!
     * \brief Method used to request data from server
     * \return True if data is successfully requested
     */
    virtual bool RequestData();

    /*!
     * \brief Method used to check acknowledge
     * \return True if ACK is received, false otherwise
     */
    virtual bool CheckAck();

    virtual bool WaitForReadyRead(int timeout);
    virtual void ReadData(QByteArray& data_out);

  public slots:
    /*!
     * \brief ReadyRead slot
     */
    void ReadyRead();

  signals:

    /*!
     * \brief Signal download progress
     * \param bytes_received - Number of received bytes
     * \param bytes_total - Total number of bytes
     */
    void DownloadProgress(const qint64& bytes_received, const qint64& bytes_total);

  private:

    quint16 server_port_;           //!< Server port
    QString server_address_;        //!< Server address
    QByteArray preshared_key_;      //!< Preshared key
    QJsonArray servers_array_;      //!< Server array

    QByteArray socket_rx_data_;     //!< Byte Array work as an Rx buffer
    int previous_rx_data_size_{0};  //!< Previous Rx data size

    qint32 file_size_{0};           //!< File size

    bool emit_progress{false};      //!< Flag for enabling/disabling emiting download prograss

    const QByteArray kAck{"ACK"};   //!< Byte array constant that presents ACKNOWLEDGE
};

} // namespace socket

#endif // SOCKET_H_
