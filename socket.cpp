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
#include <QMessageAuthenticationCode>

QByteArray shaKeyComm = "NDQ4N2Y1YjFhZTg3ZGI3MTA1MjlhYmM3";

const QHostAddress serverAddress  = QHostAddress::LocalHost;
quint16 port = 5322;

SocketClient::SocketClient() :
    m_tcpClient()
{
}

bool SocketClient::dataTransfer(QByteArray &inData, QByteArray &outData)
{
    bool success = false;
    QMessageAuthenticationCode code(QCryptographicHash::Sha256);
    code.setKey(shaKeyComm);

    m_tcpClient.connectToHost(serverAddress, port);
    success = m_tcpClient.waitForReadyRead();
    if(success) {
        QByteArray token = m_tcpClient.readAll();
        code.addData(token);

        QByteArray hash = code.result();
        m_tcpClient.write(hash);   //send hash
        success = m_tcpClient.waitForBytesWritten();
    }

    if(success) {
        m_tcpClient.write(inData);          //send board id
        success = m_tcpClient.waitForBytesWritten();

    }

    if(success) {
        success = m_tcpClient.waitForReadyRead();
        outData = m_tcpClient.readAll();
    }

    m_tcpClient.close();

    return success;
}
