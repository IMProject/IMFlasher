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
#include <QElapsedTimer>
#include <QSerialPortInfo>

namespace communication {
namespace {

constexpr int kTimerTimeoutInMs {20000};
constexpr int kSerialTimeoutInMs {100};
constexpr char kSoftwareTypeCmd[] = "software_type";
constexpr char kManufactImBoot[] = "IMBOOT";
constexpr char kManufactImApp[] = "IMAPP";
constexpr char kManufactMicrosoft[] = "Microsoft";
constexpr char kSwTypeImBoot[] = "IMBootloader";
constexpr char kSwTypeImApp[] = "IMApplication";

} // namespace

SerialPort::SerialPort()
    : m_settings(std::make_unique<SettingsDialog>()),
      m_isOpen(false),
      m_isBootlaoder(false) {}

SerialPort::~SerialPort() = default;

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
    QElapsedTimer timer;
    timer.start();

    while (!m_isOpen) {
        tryOpenPort();

        if (timer.hasExpired(kTimerTimeoutInMs)) {
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

bool SerialPort::isOpen() const
{
    return m_isOpen;
}

bool SerialPort::tryOpenPort()
{
   //Auto serching for connected USB
    bool success = false;
    m_settings->updateSettings();
    m_port = m_settings->GetCurrentSettings();

    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        //We are supporting multiple different manufacturer names
        success = true;

        if (kManufactImBoot == info.manufacturer()) {

            m_port.manufactNameEnum = ManufacturerName::kImBoot;
            m_port.name = info.portName();

        } else if (kManufactImApp == info.manufacturer()) {

            m_port.manufactNameEnum = ManufacturerName::kImApp;
            m_port.name = info.portName();

        } else if (kManufactMicrosoft == info.manufacturer()) {
            m_port.manufactNameEnum = ManufacturerName::kMicrosoft;
            m_port.name = info.portName();
        } else {
            success = false;
        }

        if(success) {
            openConn();

            if (m_isOpen) {

                // For Microsoft, we can't be sure if we are connected to the proper board so we need to check this.
                if (detectBoard()) {
                    // we are connected to proper board, exit for loop
                    break;

                } else {
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

bool SerialPort::detectBoard()
{
    bool isBoardDetected;
    write(kSoftwareTypeCmd, sizeof(kSoftwareTypeCmd));
    waitForReadyRead(kSerialTimeoutInMs);
    QString softwareType = readAll();

    if (softwareType == kSwTypeImApp) {
        // We are in application
        m_isBootlaoder = false;
        isBoardDetected = true;

    } else if (softwareType == kSwTypeImBoot) {
        m_isBootlaoder = true;
        isBoardDetected = true;

    } else {
        isBoardDetected = false;
    }

    return isBoardDetected;
}

bool SerialPort::isBootloaderDetected() const
{
    return m_isBootlaoder;
}

ManufacturerName SerialPort::getManufactName() const
{
    return m_port.manufactNameEnum;
}

} // namespace communication
