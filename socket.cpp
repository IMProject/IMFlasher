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

#include "socket.h"

#include <QHostAddress>
#include <QMessageAuthenticationCode>
#include <QTcpSocket>

namespace socket {
namespace {

constexpr quint16 kPort {5322};
constexpr char kShaKey[] = "NDQ4N2Y1YjFhZTg3ZGI3MTA1MjlhYmM3";

} // namespace

bool DataTransfer(const QByteArray& in_data, QByteArray& out_data)
{
    bool success = false;
    QTcpSocket tcp_socket;
    QMessageAuthenticationCode code(QCryptographicHash::Sha256);
    code.setKey(kShaKey);

    tcp_socket.connectToHost(QHostAddress::LocalHost, kPort);

    if (tcp_socket.waitForReadyRead()) {
        QByteArray token = tcp_socket.readAll();
        code.addData(token);

        QByteArray hash = code.result();
        tcp_socket.write(hash);

        if (tcp_socket.waitForBytesWritten()) {
            tcp_socket.write(in_data);

            if (tcp_socket.waitForBytesWritten()) {
                out_data = tcp_socket.readAll();
                success = true;
            }
        }
    }

    tcp_socket.close();

    return success;
}

} // namespace communication
