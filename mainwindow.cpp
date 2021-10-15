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
#include <QMessageBox>

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
    m_flasher(flasher)
{
    m_ui.setupUi(this);

    InitActions();
    ConnectActions();
    DisableAllButtons();
    ClearProgress();

    connect(m_flasher.get(), &flasher::Flasher::updateProgress, this, [&] (const qint64& sentSize, const qint64& firmwareSize) {
        const int progressPercentage = (100 * sentSize) / firmwareSize;
        m_ui.progressBar->setValue(progressPercentage);
        qInfo() << sentSize << "/" << firmwareSize << "B, " << progressPercentage <<"%";
    });

    connect(m_flasher.get(), &flasher::Flasher::clearProgress, this, &MainWindow::ClearProgress);

    connect(m_flasher.get(), &flasher::Flasher::showStatusMsg, this, [&] (const QString& text) { ShowStatusMessage(text); });

    connect(m_flasher.get(), &flasher::Flasher::failedToConnect, this, [&] (void) {
        ShowStatusMessage(tr("Failed to connect!"));
        m_ui.actionConnect->setEnabled(true);
        m_ui.actionDisconnect->setEnabled(false);
    });

    connect(m_flasher.get(), &flasher::Flasher::textInBrowser, this, [&] (const auto& text) { m_ui.textBrowser->append(text); });

    connect(m_flasher.get(), &flasher::Flasher::isBootloader, this, [&] (const auto& isBootloader)
    {
        m_ui.enterBootloader->setEnabled(true);

        if (isBootloader) {
            m_ui.enterBootloader->setText("Exit bootloader");
            m_ui.selectFirmware->setEnabled(true);
            m_ui.protectButton->setEnabled(true);
        }
        else {
            m_ui.enterBootloader->setText("Enter bootloader");
            m_ui.selectFirmware->setEnabled(false);
            m_ui.protectButton->setEnabled(false);
        }
    });

    connect(m_flasher.get(), &flasher::Flasher::isReadProtectionEnabled, this, [&] (const auto& isEnabled)
    {
        m_isReadProtectionEnabled = isEnabled;
        if (isEnabled) {
            m_ui.protectButton->setText("Disable read protection");
        }
        else {
            m_ui.protectButton->setText("Enable read protection");
        }
    });

    connect(m_flasher.get(), &flasher::Flasher::disableAllButtons, this, &MainWindow::DisableAllButtons);
    connect(m_flasher.get(), &flasher::Flasher::enableLoadButton, this, [&] (void) { m_ui.loadFirmware->setEnabled(true); });
    connect(m_flasher.get(), &flasher::Flasher::enableRegisterButton, this, [&] (void) { m_ui.registerButton->setEnabled(true); });
}

MainWindow::~MainWindow() = default;

void MainWindow::ClearProgress()
{
    m_ui.progressBar->hide();
    m_ui.progressBar->setValue(0);
}

void MainWindow::DisableAllButtons()
{
    m_ui.enterBootloader->setEnabled(false);
    m_ui.selectFirmware->setEnabled(false);
    m_ui.loadFirmware->setEnabled(false);
    m_ui.protectButton->setEnabled(false);
    m_ui.registerButton->setEnabled(false);
}

void MainWindow::ConnectActions()
{
    connect(m_ui.actionConnect, &QAction::triggered, this, [&] (void)
    {
        m_ui.actionConnect->setEnabled(false);
        m_ui.actionDisconnect->setEnabled(true);
        m_flasher->SetState(flasher::FlasherStates::kTryToConnect);
    });

    connect(m_ui.actionDisconnect, &QAction::triggered, this, [&] (void)
    {
        m_ui.actionConnect->setEnabled(true);
        m_ui.actionDisconnect->setEnabled(false);
        DisableAllButtons();
        m_flasher->SetState(flasher::FlasherStates::kDisconnected);
    });

    connect(m_ui.actionQuit, &QAction::triggered, this, [&] (void) { close(); });

    connect(m_ui.actionAbout, &QAction::triggered, this, [&] (void) {
        QMessageBox::about(this,
                           tr("About IMFlasher"),
                           tr("The <b>IMFlasher</b> v1.2.0"));
    });
}

void MainWindow::InitActions()
{
    m_ui.actionConnect->setEnabled(true);
    m_ui.actionDisconnect->setEnabled(false);
    m_ui.actionQuit->setEnabled(true);
}

void MainWindow::ShowStatusMessage(const QString &message)
{
    m_ui.statusLabel->setText(message);
}

void MainWindow::on_selectFirmware_clicked()
{
    m_flasher->SetState(flasher::FlasherStates::kSelectFirmware);
}

void MainWindow::on_loadFirmware_clicked()
{
    m_ui.loadFirmware->setEnabled(false);
    m_ui.progressBar->show();
    m_flasher->SetState(flasher::FlasherStates::kFlash);
}

void MainWindow::on_registerButton_clicked()
{
    m_ui.registerButton->setEnabled(false);
    m_flasher->SetState(flasher::FlasherStates::kRegisterBoard);
}

void MainWindow::on_enterBootloader_clicked()
{
    if (m_flasher->IsBootloaderDetected()) {
        m_flasher->SetState(flasher::FlasherStates::kExitBootloader);
    }
    else {
        m_flasher->SetState(flasher::FlasherStates::kEnterBootloader);
    }
}

void MainWindow::on_protectButton_clicked()
{
    if (!m_isReadProtectionEnabled) {
        if (m_flasher->sendEnableFirmwareProtection()) {
            m_flasher->SetState(flasher::FlasherStates::kReconnect);
        }
    }
    else {
        if (ShowInfoMsg("Disable read protection", "Once disabled, complete flash will be erased including bootloader!")) {
            if (m_flasher->sendDisableFirmwareProtection()) {
                m_ui.protectButton->setEnabled(false);
            }
        }
    }
}

} // namespace gui
