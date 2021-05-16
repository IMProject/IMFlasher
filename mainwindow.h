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
#include <QSerialPort>
#include <QProgressBar>
#include <QFile>

#include <QApplication>
#include <QThread>
#include <flasher.h>

QT_BEGIN_NAMESPACE

class QLabel;

namespace Ui {
class MainWindow;
}

QT_END_NAMESPACE

class Console;
class SettingsDialog;
class Flasher;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void closeSerialPort();
    void showStatusMessage(const QString &message);
    Flasher* getFlasherPtr();
    void openSerialPortUi();
    void closeSerialPortUi();


private slots:
    void about();
    void handleError(QSerialPort::SerialPortError error);
    void updateProgressUi(qint64 dataPosition, qint64 firmwareSize);
    void connectUsbToPcUi();
    void connectedSerialPortUi();
    void disconnectedSerialPortUi();
    void appendTextInBrowser(QString boardId);
    void isBootloaderUi(bool bootloader);
    void enableLoadButtonUi();
    void flashingStatusLabelUi(QString status);

    void on_selectFirmware_clicked();
    void on_loadFirmware_clicked();
    void on_registerButton_clicked();

signals:
    void startFlashingSignal();

private:
    void initActionsConnections();

private:

    Ui::MainWindow *m_ui = nullptr;
    Console *m_console = nullptr;
    SettingsDialog *m_settings = nullptr;
    Flasher *m_flasher = nullptr;
    bool m_isBootloader; // USB can be connected to blootloader or application
};

#endif // MAINWINDOW_H
