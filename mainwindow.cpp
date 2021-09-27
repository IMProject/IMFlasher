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

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>

#include "ui_mainwindow.h"
#include "flasher.h"

namespace gui {
namespace {

bool ShowInfoMsg(const QString& title, const QString& description)
{
    QMessageBox msgBox;
    msgBox.setText(title);
    msgBox.setInformativeText(description);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.addButton(QMessageBox::Cancel);
    return (msgBox.exec() == QMessageBox::Ok);
}

} // namespace

MainWindow::MainWindow(std::shared_ptr<flasher::Flasher> flasher, QWidget *parent) :
    QMainWindow(parent),
    m_ui(std::make_shared<Ui::MainWindow>()),
    m_flasher(flasher),
    m_isBootloader(false)
{
    m_ui->setupUi(this);

    m_ui->actionConnect->setEnabled(true);
    m_ui->actionDisconnect->setEnabled(false);
    m_ui->actionQuit->setEnabled(true);
    m_ui->enterBootloader->setEnabled(true);
    m_ui->loadFirmware->setEnabled(false);
    m_ui->protectButton->setEnabled(false);

    m_ui->progressBar->hide();
    m_ui->progressBar->setValue(0);

    m_ui->registerButton->setEnabled(true);

    this->initActionsConnections();

    connect(m_flasher.get(), &flasher::Flasher::updateProgress, this, [&] (const qint64& sentSize, const qint64& firmwareSize) {
        int progressPercentage = (100 * sentSize) / firmwareSize;
        m_ui->progressBar->setValue(progressPercentage);
        qInfo() << sentSize << "/" << firmwareSize << "B, " << progressPercentage <<"%";
    });

    connect(m_flasher.get(), &flasher::Flasher::clearProgress, this, [&] (void) {
        m_ui->progressBar->hide();
        m_ui->progressBar->setValue(0);
    });

    connect(m_flasher.get(), &flasher::Flasher::showStatusMsg, this, [&] (const QString& text) { this->showStatusMessage(text); });

    connect(m_flasher.get(), &flasher::Flasher::failedToConnect, this, [&] (void) {
        this->showStatusMessage(tr("Failed to connect!"));
        m_ui->actionConnect->setEnabled(true);
        m_ui->actionDisconnect->setEnabled(false);
    });

    connect(m_flasher.get(), &flasher::Flasher::textInBrowser, this, [&] (const auto& text) { m_ui->textBrowser->append(text); });
    connect(m_flasher.get(), &flasher::Flasher::isBootloader, this, &MainWindow::isBootloaderUi);
    connect(m_flasher.get(), &flasher::Flasher::isReadProtectionEnabled, this, &MainWindow::isReadProtectionEnabledUi);
    connect(m_flasher.get(), &flasher::Flasher::readyToFlashId, this, [&] (void) { m_ui->loadFirmware->setEnabled(true); });
}

MainWindow::~MainWindow() = default;

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
    m_flasher->closeSerialPort();
}

void MainWindow::isBootloaderUi(const bool& bootloader)
{
    m_isBootloader = bootloader;

    if(bootloader) {
        m_ui->enterBootloader->setText("Exit bootloader");
        m_ui->loadFirmware->setEnabled(false);
        m_ui->selectFirmware->setEnabled(true);
        m_ui->protectButton->setEnabled(true);
        m_ui->registerButton->setEnabled(false);
        m_ui->enterBootloader->setEnabled(true);

    } else {
        m_ui->enterBootloader->setText("Enter bootloader");
        m_ui->enterBootloader->setEnabled(true);
        m_ui->loadFirmware->setEnabled(false);
        m_ui->selectFirmware->setEnabled(false);
        m_ui->protectButton->setEnabled(false);
    }
}

void MainWindow::isReadProtectionEnabledUi(const bool& enabled)
{
    m_isReadProtectionEnabled = enabled;

    if(enabled) {
        m_ui->protectButton->setText("Disable read protection");
    } else {
        m_ui->protectButton->setText("Enable read protection");
    }
}

void MainWindow::initActionsConnections()
{
    connect(m_ui->actionConnect, &QAction::triggered, this, &MainWindow::openSerialPortUi);

    connect(m_ui->actionDisconnect, &QAction::triggered, this, &MainWindow::closeSerialPortUi);

    connect(m_ui->actionQuit, &QAction::triggered, this, [&] (void) { this->close(); });

    connect(m_ui->actionAbout, &QAction::triggered, this, [&] (void) {
        QMessageBox::about(this,
                           tr("About IMFlasher"),
                           tr("The <b>IMFlasher</b> v1.2.0"));
    });
}

void MainWindow::showStatusMessage(const QString &message)
{
    m_ui->statusLabel->setText(message);
}

void MainWindow::on_selectFirmware_clicked()
{
    m_flasher->getWorkerThread().quit();

    QString filePath = QFileDialog::getOpenFileName(this,
            tr("Flight control binary"), "",
            tr("Binary (*.bin);;All Files (*)"));

    m_flasher->init();

    m_flasher->setFilePath(filePath);
    m_flasher->setState(flasher::FlasherStates::kOpenFile);
}

void MainWindow::on_loadFirmware_clicked()
{
    if(m_isBootloader) {
        m_ui->loadFirmware->setEnabled(false);
        m_ui->progressBar->show();
        m_flasher->setState(flasher::FlasherStates::kFlash);
    }
}

void MainWindow::on_registerButton_clicked()
{
    m_flasher->setState(flasher::FlasherStates::kGetBoardIdKey);
}

void MainWindow::on_enterBootloader_clicked()
{
    if (m_isBootloader) {
        if (m_flasher->SendExitBootloaderCommand()) {
            isBootloaderUi(false);
        }

    } else {
        if (!m_flasher->SendEnterBootloaderCommand()) {
            m_flasher->sendFlashCommand();
        }
    }

    m_flasher->reopenSerialPort();
}

void MainWindow::on_protectButton_clicked()
{
    if(!m_isReadProtectionEnabled) {
        if (m_flasher->sendEnableFirmwareProtection()) {
            m_flasher->reopenSerialPort();
        }

    } else {

        bool proceed = ShowInfoMsg("Disable read protection", "Once disabled, complete flash will be erased including bootloader!");

        if(proceed) {
            bool success = m_flasher->sendDisableFirmwareProtection();
            if(success) {
                m_ui->protectButton->setEnabled(false);
            }
        }
    }
}

} // namespace gui
