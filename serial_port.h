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

#ifndef SERIAL_PORT_H_
#define SERIAL_PORT_H_

#include <QSerialPort>

namespace communication {

class SerialPort : public QSerialPort {
    Q_OBJECT

  public:
    SerialPort();
    ~SerialPort();
    void CloseConn();
    bool TryOpenPort(bool& is_bootloader);

    /**
     * Wait until RX data is ready. The function has a predefined wait value with no data on the serial line,
     * so it is able to receive data in chunks.
     *
     * @param[in] timeout   Function timeout value
     */
    void WaitForReadyRead(int timeout);

    /**
     * Function for copying data to a given reference. After data is copied internal buffer will be cleared.
     *
     * @param[out] data_out Reference to where data will be copied
     */
    void ReadData(QByteArray& data_out);

  public slots:
    void ReadyRead();

  private:
    bool DetectBoard(bool& is_bootloader);
    bool OpenConnection(const QString& port_name);

    QByteArray serial_rx_data_;     //!< Byte Array work as an RX buffer.
    int previous_rx_data_size_{0};
};

} // namespace communication
#endif // SERIAL_PORT_H_
