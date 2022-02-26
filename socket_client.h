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

namespace  {

const QString kHeaderClientBoardInfo{"header_client_board_info"};
const QString kHeaderClientProductInfo{"header_client_product_info"};
const QString kHeaderServerProductInfo{"header_server_product_info"};
}

class SocketClient : public QTcpSocket
{
  public:

    /**
     * Socket client constructor
     *
     * @param[in] servers_array   Json array with servers config
     */

    SocketClient(const QJsonArray& servers_array);

    virtual ~SocketClient();

    /**
     * Sending information about board to the server
     *
     * @param[in] board_info    Json object with board info for server
     * @param[in] bl_version    Json object with bootlaoder git version
     * @param[in] fw_version    Json object with firmware git version
     */

    virtual bool SendBoardInfo(QJsonObject board_info, QJsonObject bl_version, QJsonObject fw_version);

    /**
     * Receiving product info for given board info from the server
     *
     * @param[in] board_info    Json object with board info for server
     * @param[out] product_info Json array with firmwares to download
     */

    virtual bool ReceiveProductInfo(QJsonObject board_info, QJsonArray& product_info);

  private:
    virtual bool Connect();
    virtual bool Disconnect();
    virtual bool Authentication();
    virtual bool ReadAll(QByteArray& data_out);
    virtual bool SendData(const QByteArray& in_data);
    virtual bool CheckAck();

    quint16 server_port_;
    QString server_address_;
    QByteArray preshared_key_;
    QJsonArray servers_array_;

    const QByteArray kAck{"ACK"};
};

} // namespace socket

#endif // SOCKET_H_
