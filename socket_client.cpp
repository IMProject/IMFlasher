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
#include "crc32.h"

#include <QHostAddress>
#include <QMessageAuthenticationCode>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QThread>

namespace socket {

namespace {

constexpr int kMaxNoDataPeriod {10}; //!< Max time in [ms] while waiting
constexpr qint64 kSocketTimeout {1000};

} // namespace

SocketClient::SocketClient(QJsonArray servers_array) :
    servers_array_(std::move(servers_array)) {

    connect(this, &socket::SocketClient::readyRead, this, &socket::SocketClient::ReadyRead);
}

SocketClient::~SocketClient() = default;

void SocketClient::ReadyRead() {
    socket_rx_data_.append(readAll());

    if (emit_progress) {
        emit DownloadProgress(socket_rx_data_.size(), file_size_);
    }
}

bool SocketClient::WaitForReadyRead(int timeout = 0) {

    bool success = false;
    QElapsedTimer timer;
    timer.start();

    while (!timer.hasExpired(timeout)) {

        QObject().thread()->msleep(kMaxNoDataPeriod); // Give some time to the sender to send the data
        waitForReadyRead(1); //known workaround for triggering readyRead signal(https://bugreports.qt.io/browse/QTBUG-78086)

        int current_rx_data_size = socket_rx_data_.size();
        if ((current_rx_data_size == previous_rx_data_size_) && (current_rx_data_size != 0)) {
            // No new data. Ready to read, exit the loop.
            success = true;
            break;
        }

        previous_rx_data_size_ = current_rx_data_size;
    }

    return success;
}

void SocketClient::ReadData(QByteArray& data_out) {
    data_out = socket_rx_data_;
    socket_rx_data_.clear();
    previous_rx_data_size_ = 0;
}

bool SocketClient::Connect() {
    bool success = false;

    foreach (const QJsonValue& server, servers_array_) {
        QJsonObject obj = server.toObject();
        server_address_ = obj["address"].toString();
        server_port_ = obj["port"].toInt();
        preshared_key_ = obj["preshared_key"].toString().toUtf8();

        if (state() == UnconnectedState) {
            connectToHost(server_address_, server_port_);

            if (waitForConnected(kSocketTimeout) && Authentication()) {
                success = true;
                break;
            } else {
                disconnectFromHost();
            }
        }
    }

    return success;
}

bool SocketClient::Disconnect() {
    if (state() == ConnectedState) {
        disconnectFromHost();
    }

    return (state() != ConnectedState);
}

bool SocketClient::Authentication() {
    bool success = false;
    QMessageAuthenticationCode code(QCryptographicHash::Sha256);
    code.setKey(preshared_key_);

    QByteArray token;
    success = ReadAll(token);
    if (success) {

        code.addData(token);
        QByteArray hash = code.result();
        success = SendDataWithAck(hash);
    }

    return success;
}

bool SocketClient::ReadAll(QByteArray& out_data) {

    bool success = WaitForReadyRead(kSocketTimeout);
    if (success) {
        ReadData(out_data);
    }

    return success;
}

bool SocketClient::SendDataWithAck(const QByteArray& in_data) {
    bool success = false;
    qint64 size = write(in_data);
    success = waitForBytesWritten();

    if (success && in_data.size() == size) {
        success = CheckAck();
    }

    return success;
}

bool SocketClient::SendQJsonObject(const QJsonObject& json_objec) {
    QJsonDocument json_doc;
    json_doc.setObject(json_objec);
    return SendDataWithAck(json_doc.toJson());
}

bool SocketClient::RequestData() {

    QJsonObject packet_object;
    packet_object.insert("header", kHeaderClientRequestData);
    QJsonDocument json_doc;
    json_doc.setObject(packet_object);

    QByteArray data = json_doc.toJson();
    write(data);
    return waitForBytesWritten();
}

bool SocketClient::CheckAck() {
    bool success = false;

    QByteArray ack;
    success = ReadAll(ack);

    if (success && (kAck == ack)) {
        success = true;
    }

    return success;
}

bool SocketClient::SendBoardInfo(QJsonObject board_info, QJsonObject bl_version, QJsonObject fw_version) {
    bool success = Connect();

    if (success) {

        QJsonObject app_version;
        app_version.insert("app_branch", GIT_BRANCH);
        app_version.insert("app_hash", GIT_HASH);
        app_version.insert("app_tag", GIT_TAG);

        QJsonObject packet_object;
        packet_object.insert("header", kHeaderClientBoardInfo);
        packet_object.insert("board_info", board_info);
        packet_object.insert("bl_version", bl_version);
        packet_object.insert("fw_version", fw_version);
        packet_object.insert("app_version", app_version);

        success = SendQJsonObject(packet_object);

        if (success) {
            qInfo() << "Board info updated to server " << server_address_;
        }
    }

    Disconnect();

    return success;
}

bool SocketClient::ReceiveProductInfo(QJsonObject board_info, QJsonArray& product_info) {
    bool success = Connect();

    if (success) {
        QJsonObject packet_object;
        packet_object.insert("header", kHeaderClientProductInfo);
        packet_object.insert("board_info", board_info);

        success = SendQJsonObject(packet_object);
    }

    if (success) {

        success = RequestData(); // request JSON with product info

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
    }

    Disconnect();

    return success;
}

bool SocketClient::DownloadFirmwareFile(QJsonObject board_info, QString fw_version, QByteArray& firmware_file) {

    qint32 file_crc;

    bool success = Connect();

    if (success) {
        QJsonObject packet_object;
        packet_object.insert("header", kHeaderClientDownloadFirmware);
        packet_object.insert("board_info", board_info);
        packet_object.insert("fw_version", fw_version);

        QJsonDocument json_data;
        json_data.setObject(packet_object);
        success = SendDataWithAck(json_data.toJson());
    }

    if (success) {

        QByteArray data;

        success = RequestData(); // request JSON with file info

        if (success) {
            success = ReadAll(data);
        }

        QJsonDocument json_data = QJsonDocument::fromJson(data);
        QJsonObject packet_object = json_data.object();

        if (packet_object.value("header").toString() == kHeaderServerDownloadFirmware) {

            file_crc = packet_object.value("file_crc").toInt();
            file_size_ = packet_object.value("file_size").toInt();

        } else {
            success = false;
        }
    }

    if (success) {
        emit_progress = true;
        success = RequestData(); // request firmware binary file

        if (success) {
            success = ReadAll(firmware_file);

            if (success) {

                const uint8_t *data = reinterpret_cast<const uint8_t *>(firmware_file.data());
                qint32 crc = crc::CalculateCrc32(data, firmware_file.size(), false, false);

                if ((firmware_file.size() != file_size_) || (crc != file_crc)) {
                    success = false;
                }
            }
        }

        emit_progress = false;
    }

    Disconnect();

    return success;
}

} // namespace socket
