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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

namespace flasher {
class Flasher;
}

namespace gui {

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(std::shared_ptr<flasher::Flasher> flasher, QWidget *parent = nullptr);
    ~MainWindow();

    void closeSerialPort();
    void showStatusMessage(const QString &message);
    void openSerialPortUi();
    void closeSerialPortUi();
    bool showInfoMsg(const QString& title, const QString& description);

private slots:
    void isBootloaderUi(const bool& bootloader);
    void isReadProtectionEnabledUi(const bool& isProteced);

    void on_selectFirmware_clicked();
    void on_loadFirmware_clicked();
    void on_registerButton_clicked();
    void on_enterBootloader_clicked();
    void on_protectButton_clicked();

private:
    void initActionsConnections();

private:
    std::shared_ptr<Ui::MainWindow> m_ui;
    std::shared_ptr<flasher::Flasher> m_flasher;
    bool m_isBootloader; // USB can be connected to bootloader or application
    bool m_isOverRAM;    // If inside BL over RAM enable exit, over FLASH can't exit.
    bool m_isReadProtectionEnabled;  // Firmware is protected
};

} // namespace gui
#endif // MAINWINDOW_H
