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

#include "serial_port.h"
#include <QDebug>

#define TIMER_TIMEOUT       10000u //ms

SerialPort::SerialPort()
    : m_settings(new SettingsDialog),
      m_isOpen(false),
      m_timer(new QTimer)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(TIMER_TIMEOUT);
    m_timer.start();
}

void SerialPort::openConn()
{
    if(m_isOpen == false) {
        setPortName(m_port.name);
        setBaudRate(m_port.baudRate);
        setDataBits(m_port.dataBits);
        setParity(m_port.parity);

        setStopBits(m_port.stopBits);
        setFlowControl(m_port.flowControl);
        if (open(QIODevice::ReadWrite)) {
            m_isOpen = true;
        } else {
            m_isOpen = false;
        }
    }
}

void SerialPort::openConnBlocking()
{
    while(!m_isOpen) {
        tryOpenPort();
        if(m_timer.remainingTime() == 0){
            qInfo() << "Timeout";
            break;
        }
    }
}
void SerialPort::closeConn()
{
    if (m_isOpen) {
        close();
        m_isOpen = false;
    }
}

bool SerialPort::isOpen()
{
    return m_isOpen;
}

bool SerialPort::tryOpenPort()
{
   //Auto serching for connected USB
    bool success = false;
    m_settings->updateSettings();
    m_port = m_settings->settings();

    switch(m_port.manufactNameEnum)
    {
     case(MANUFACT_NAME_IMBOOT):
     case(MANUFACT_NAME_IMAPP):
     case(MANUFACT_NAME_MICROSOFT):
        success = true;
        break;

    default:
        success = false;
        break;
    }

    if(success) {
        openConn();
    } else {

        if(m_isOpen) {
            close();
        }

        m_isOpen = false;
    }

    return success;
}
