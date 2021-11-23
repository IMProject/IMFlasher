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

constexpr int kSerialTimeoutInMs {100};
constexpr char kSoftwareTypeCmd[] = "software_type";
constexpr char kSwTypeImBoot[] = "IMBootloader";
constexpr char kSwTypeImApp[] = "IMApplication";

} // namespace

SerialPort::SerialPort() = default;
SerialPort::~SerialPort() = default;

void SerialPort::CloseConn()
{
    if (isOpen()) {
        close();
    }
}

bool SerialPort::DetectBoard(bool& is_bootloader)
{
    bool is_board_detected;
    write(kSoftwareTypeCmd, sizeof(kSoftwareTypeCmd));
    waitForReadyRead(kSerialTimeoutInMs);
    QString software_type = readAll();

    if (software_type == kSwTypeImApp) {
        is_bootloader = false;
        is_board_detected = true;
    }
    else if (software_type == kSwTypeImBoot) {
        is_bootloader = true;
        is_board_detected = true;

    } else {
        is_board_detected = false;
    }

    return is_board_detected;
}

bool SerialPort::OpenConnection(const QString& port_name)
{
    if (port_name.isEmpty()) return false;

    setPortName(port_name);
    setBaudRate(QSerialPort::Baud115200);
    setDataBits(QSerialPort::Data8);
    setParity(QSerialPort::NoParity);

    setStopBits(QSerialPort::OneStop);
    setFlowControl(QSerialPort::NoFlowControl);

    return open(QIODevice::ReadWrite);
}

bool SerialPort::TryOpenPort(bool& is_bootloader)
{
    const auto& infos = QSerialPortInfo::availablePorts();
    for (const auto& info : infos) {
        if (OpenConnection(info.portName())) {
            if (DetectBoard(is_bootloader)) return true;
            else CloseConn();
        }
    }

    return false;
}

} // namespace communication
