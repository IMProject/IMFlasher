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
