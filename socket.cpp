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
