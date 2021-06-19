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

#include "serial_port.h"
#include <QDebug>
#include <QSerialPortInfo>

#define TIMER_TIMEOUT       10000u //ms

#define SOFTWARE_TYPE_CMD           "software_type"
#define IM_BOOTLOADER_STR           "IMBootloader"
#define IM_APPLICATION_STR          "IMApplication"

SerialPort::SerialPort()
    : m_settings(new SettingsDialog),
      m_isOpen(false),
      m_timer(new QTimer),
      m_isBootlaoder(false)
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

    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        //We are supporting multiple different manufacturer names
        success = true;

        if(MANUFACT_IMBOOT == info.manufacturer()) {

            m_port.manufactNameEnum = manufacturerName::IMBOOT;
            m_port.name = info.portName();

        } else if(MANUFACT_IMAPP == info.manufacturer()) {

            m_port.manufactNameEnum = manufacturerName::IMAPP;
            m_port.name = info.portName();

        } else if(MANUFACT_MICROSOFT == info.manufacturer()) {
            m_port.manufactNameEnum = manufacturerName::MICROSOFT;
            m_port.name = info.portName();
        } else {
            success = false;
        }

        if(success) {
            openConn();

            // For Microsoft, we can't be sure if we are connected to the proper board so we need to check this.
            bool isBoardDetected = detectBoard();
            if(isBoardDetected) {
                // we are connected to proper board, exit for loop
                break;
            } else {
                if(m_isOpen) {
                    closeConn();
                }
            }

        } else {
            if(m_isOpen) {
                closeConn();
            }
        }
    }

    return success;
}

bool SerialPort::detectBoard(void)
{
    bool isBoardDetected = false;
    write(SOFTWARE_TYPE_CMD, sizeof(SOFTWARE_TYPE_CMD));
    waitForReadyRead(SERIAL_TIMEOUT);
    QString softwareType = readAll();

    if(softwareType == IM_APPLICATION_STR) {
        // We are in application
        m_isBootlaoder = false;
        isBoardDetected = true;

    } else if (softwareType == IM_BOOTLOADER_STR) {
        m_isBootlaoder = true;
        isBoardDetected = true;

    }

    return isBoardDetected;
}

bool SerialPort::isBootloaderDetected(void)
{
    return m_isBootlaoder;
}

