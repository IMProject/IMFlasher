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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"
#include "crc32.h"

#include <QMessageBox>
#include <QLabel>
#include <QFileDialog>

#include <qmath.h>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::MainWindow),
    m_settings(new SettingsDialog()),
    m_flasher(new Flasher),
    m_isBootloader(false)
{
    m_ui->setupUi(this);

    m_ui->actionConnect->setEnabled(true);
    m_ui->actionDisconnect->setEnabled(false);
    m_ui->actionQuit->setEnabled(true);
    m_ui->loadFirmware->setEnabled(false);

    m_ui->progressBar->hide();
    m_ui->progressBar->setValue(0);

    m_ui->registerButton->setEnabled(true);

    initActionsConnections();

    connect(m_flasher, &Flasher::updateProgress, this, &MainWindow::updateProgressUi);
    connect(m_flasher, &Flasher::connectUsbToPc, this, &MainWindow::connectUsbToPcUi);
    connect(m_flasher, &Flasher::connectedSerialPort, this, &MainWindow::connectedSerialPortUi);
    connect(m_flasher, &Flasher::disconnectedSerialPort, this, &MainWindow::disconnectedSerialPortUi);
    connect(m_flasher, &Flasher::textInBrowser, this, &MainWindow::appendTextInBrowser);
    connect(m_flasher, &Flasher::isBootloader, this, &MainWindow::isBootloaderUi);
    connect(m_flasher, &Flasher::readyToFlashId, this, &MainWindow::enableLoadButtonUi);
    connect(m_flasher, &Flasher::flashingStatusSignal, this, &MainWindow::flashingStatusLabelUi);
    connect(this, &MainWindow::startFlashingSignal, m_flasher, &Flasher::startFlashingSlot);
}

MainWindow::~MainWindow()
{
    delete m_settings;
    delete m_ui;
}

void MainWindow::openSerialPortUi()
{
    m_ui->actionConnect->setEnabled(false);
    m_ui->actionDisconnect->setEnabled(true);
    m_flasher->openSerialPort();
}

void MainWindow::closeSerialPortUi()
{
    m_ui->actionConnect->setEnabled(true);
    m_ui->actionDisconnect->setEnabled(false);
    //m_ui->actionConfigure->setEnabled(true);
    m_flasher->closeSerialPort();
}

void MainWindow::connectedSerialPortUi()
{
    showStatusMessage(tr("Connected"));
    openSerialPortUi();
}

void MainWindow::disconnectedSerialPortUi()
{
    showStatusMessage(tr("Disconnected"));
    closeSerialPortUi();
}

void MainWindow::appendTextInBrowser(QString text)
{
    m_ui->textBrowser->append (text);
}

void MainWindow::isBootloaderUi(bool bootloader)
{
    m_isBootloader = bootloader;

    if(bootloader) {
        m_ui->loadFirmware->setText("Load");
        m_ui->loadFirmware->setEnabled(false);
        m_ui->selectFirmware->setEnabled(true);
        m_ui->registerButton->setEnabled(false);
    } else {
        m_ui->loadFirmware->setText("Enter in bootlaoder");
        m_ui->loadFirmware->setEnabled(true);
        m_ui->selectFirmware->setEnabled(false);
    }
}
void MainWindow::enableLoadButtonUi()
{
    m_ui->loadFirmware->setEnabled(true);
}

void MainWindow::flashingStatusLabelUi(QString status)
{
    showStatusMessage(tr("%1").arg(status));
}

void MainWindow::connectUsbToPcUi()
{
    showStatusMessage(tr("Reconnect board to PC"));
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About IMFlahser"),
                       tr("The <b>IMFlahser</b> v1.0.0"));
}

void MainWindow::updateProgressUi(qint64 sentSize, qint64 firmwareSize)
{
    uint8_t progressPercentage = (100 * sentSize) / firmwareSize; //%
    m_ui->progressBar->setValue(progressPercentage);
    qInfo() << sentSize << "/" << firmwareSize << "B, " << progressPercentage <<"%";
}

void MainWindow::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        closeSerialPortUi();
    }
}

void MainWindow::initActionsConnections()
{
    connect(m_ui->actionConnect, &QAction::triggered, this, &MainWindow::openSerialPortUi);
    connect(m_ui->actionDisconnect, &QAction::triggered, this, &MainWindow::closeSerialPortUi);
    connect(m_ui->actionQuit, &QAction::triggered, this, &MainWindow::close);
    connect(m_ui->actionAbout, &QAction::triggered, this, &MainWindow::about);
}

void MainWindow::showStatusMessage(const QString &message)
{
    m_ui->statusLabel->setText(message);
}

Flasher* MainWindow::getFlasherPtr()
{
    return m_flasher;
}

void MainWindow::on_selectFirmware_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this,
            tr("Flight control binary"), "",
            tr("Binary (*.bin);;All Files (*)"));

    m_flasher->setFilePath(filePath);
    m_flasher->actionOpenFirmwareFile();
}

void MainWindow::on_loadFirmware_clicked()
{
    if(m_isBootloader) {
        m_ui->loadFirmware->setEnabled(false);
        m_ui->progressBar->show();
        emit startFlashingSignal();
    } else {
        m_flasher->sendFlashCommandToApp();
        closeSerialPortUi();
        openSerialPortUi();
    }
}

void MainWindow::on_registerButton_clicked()
{
    m_flasher->startRegistrationProcedure();
}
