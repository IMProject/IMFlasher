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

#include "socket_client.h"

#include <QHostAddress>
#include <QMessageAuthenticationCode>
#include <QJsonDocument>

namespace socket {

SocketClient::SocketClient() {}
SocketClient::~SocketClient() {}

bool SocketClient::Connect()
{
    bool success = false;
    connectToHost(QHostAddress::LocalHost, kPort);
    if (Authentication()) {
        success = true;
    } else {
        disconnectFromHost();
    }

    return success;
}

bool SocketClient::Disconnect()
{
    bool success = false;

    SocketState socket_state = state();

    if (socket_state == ConnectedState) {
        disconnectFromHost();
        socket_state = state();
    }

    if (socket_state != ConnectedState) {
        success = true;
    }

    return success;
}

bool SocketClient::Authentication()
{
    bool success = false;
    QMessageAuthenticationCode code(QCryptographicHash::Sha256);
    code.setKey(kShaKey);

    QByteArray token;
    success = ReadAll(token);
    if (success) {

        code.addData(token);
        QByteArray hash = code.result();
        success = SendData(hash);
    }

    return success;
}

bool SocketClient::ReadAll(QByteArray& out_data)
{
    bool success = false;
    success = waitForReadyRead();
    out_data = readAll();
    return success;
}

bool SocketClient::SendData(const QByteArray& in_data)
{
    bool success = false;
    qint64 size = write(in_data);
    success = waitForBytesWritten();

    if (success && in_data.size() == size) {
        success = CheckAck();
    }

    return success;
}

bool SocketClient::CheckAck()
{
    bool success = false;

    QByteArray ack;
    success = ReadAll(ack);

    if (success && (kACK == ack)) {
        success = true;
    }

    return success;
}

bool SocketClient::SendBoardInfo(QJsonObject board_info, QJsonObject bl_version, QJsonObject fw_version)
{
    bool success = false;

    success = Connect();

    if (success) {

        QJsonObject packet_object;
        packet_object.insert("header", kHeaderClientBoardInfo);
        packet_object.insert("board_info", board_info);
        packet_object.insert("bl_version", bl_version);
        packet_object.insert("fw_version", fw_version);

        QJsonDocument json_doc;
        json_doc.setObject(packet_object);
        success = SendData(json_doc.toJson());
    }

    Disconnect();

    return success;
}

bool SocketClient::ReceiveProductInfo(QJsonObject board_info, QJsonArray& product_info)
{
    bool success = false;

    success = Connect();

    if (success) {
        QJsonObject packet_object;
        packet_object.insert("header", kHeaderClientProductInfo);
        packet_object.insert("board_info", board_info);

        QJsonDocument json_data;
        json_data.setObject(packet_object);
        success = SendData(json_data.toJson());
    }

    if (success) {
        QByteArray data;
        success = ReadAll(data);

        QJsonDocument json_data = QJsonDocument::fromJson(data);
        QJsonObject packet_object = json_data.object();

        if (packet_object.value("header").toString() == kHeaderServerProductInfo) {
            product_info = packet_object.value("product_info").toArray();
        } else {
            success = false;
        }
    }

    Disconnect();

    return success;
}

} // namespace socket
