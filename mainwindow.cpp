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
#include "flasher_states.h"

namespace gui {

MainWindow::MainWindow(std::shared_ptr<flasher::Flasher> flasher) :
    flasher_(flasher) {
    ui_.setupUi(this);

    InitActions();
    ConnectActions();
    DisableAllButtons();
    ClearProgress();

    connect(flasher_.get(), &flasher::Flasher::UpdateProgressBarSignal, this, [&] (const quint8& progress_percentage) { // *NOPAD*
        ui_.progressBar->setValue(progress_percentage);
    }, Qt::DirectConnection);

    connect(flasher_.get(), &flasher::Flasher::ClearProgress, this, &MainWindow::ClearProgress);

    connect(flasher_.get(), &flasher::Flasher::ShowStatusMsg, this, [&] (const QString& text) { ShowStatusMessage(text); }); // *NOPAD*

    connect(flasher_.get(), &flasher::Flasher::ClearStatusMsg, this, [&] { ui_.statusLabel->clear(); });

    connect(flasher_.get(), &flasher::Flasher::EnableConnectButton, this, &MainWindow::EnableConnectButton);

    connect(flasher_.get(), &flasher::Flasher::EnableDisconnectButton, this, &MainWindow::EnableDisconnectButton);

    connect(flasher_.get(), &flasher::Flasher::FailedToConnect, this, [&] {
        ShowStatusMessage(tr("Failed to connect!"));
        ui_.actionConnect->setEnabled(true);
        ui_.actionDisconnect->setEnabled(false);
    });

    connect(flasher_.get(), &flasher::Flasher::ShowTextInBrowser, this, [&] (const auto& text) { ui_.textBrowser->append(text); }); // *NOPAD*
    connect(flasher_.get(), &flasher::Flasher::ClearTextInBrowser, this, [&] {  ui_.textBrowser->clear(); });

    connect(flasher_.get(), &flasher::Flasher::SetButtons, this, [&] (const auto& is_bootloader) { // *NOPAD*
        ui_.enterBootloader->setEnabled(true);

        if (is_bootloader) {
            ui_.enterBootloader->setText("Exit bootloader");
            ui_.browseFile->setEnabled(true);
            ui_.protectButton->setEnabled(true);
        } else {
            ui_.enterBootloader->setText("Enter bootloader");
            ui_.browseFile->setEnabled(false);
            ui_.protectButton->setEnabled(false);
        }
    });

    connect(flasher_.get(), &flasher::Flasher::SetReadProtectionButtonText, this, [&] (const auto& is_enabled) { // *NOPAD*
        if (is_enabled) {
            ui_.protectButton->setText("Disable read protection");
        } else {
            ui_.protectButton->setText("Enable read protection");
        }
    });

    connect(flasher_.get(), &flasher::Flasher::DisableAllButtons, this, &MainWindow::DisableAllButtons);
    connect(flasher_.get(), &flasher::Flasher::DisableBrowseFileButton, this, &MainWindow::DisableBrowseFileButton);
    connect(flasher_.get(), &flasher::Flasher::EnableLoadButton, this, [&] { ui_.loadFile->setEnabled(true); });
    connect(flasher_.get(), &flasher::Flasher::SetFileVersionsList, this, &MainWindow::SetFileList);
}

MainWindow::~MainWindow() = default;

void MainWindow::ClearProgress() {
    ui_.progressBar->hide();
    ui_.progressBar->setValue(0);
}

void MainWindow::DisableAllButtons() {
    ui_.enterBootloader->setEnabled(false);
    ui_.availableFileVersions->setEnabled(false);
    ui_.browseFile->setEnabled(false);
    ui_.loadFile->setEnabled(false);
    ui_.protectButton->setEnabled(false);
}

void MainWindow::DisableBrowseFileButton() {
    ui_.browseFile->setEnabled(false);
}

void MainWindow::SetFileList(const QJsonArray& product_info) {
    ui_.availableFileVersions->clear();
    foreach (const QJsonValue& value, product_info) {
        QJsonObject obj = value.toObject();
        ui_.availableFileVersions->addItem(obj["file_version"].toString());
    }

    ui_.availableFileVersions->show();
    ui_.availableFileVersions->setEnabled(true);
    ui_.loadFile->setEnabled(true);
}

void MainWindow::ConnectActions() {
    connect(ui_.actionConnect, &QAction::triggered, this, [&] (void) {
        EnableDisconnectButton();
        flasher_->SetState(flasher::FlasherStates::kTryToConnect);
    });

    connect(ui_.actionDisconnect, &QAction::triggered, this, [&] (void) {
        EnableConnectButton();
        DisableAllButtons();
        flasher_->SetState(flasher::FlasherStates::kDisconnected);
    });

    connect(ui_.actionQuit, &QAction::triggered, this, [&] (void) { close(); });

    connect(ui_.actionAbout, &QAction::triggered, this, [&] (void) {
        QMessageBox::about(this,
                           tr("About IMFlasher"),
                           tr(version_info_.c_str()));
    });
}

void MainWindow::EnableConnectButton() {
    ui_.actionConnect->setEnabled(true);
    ui_.actionDisconnect->setEnabled(false);
}

void MainWindow::EnableDisconnectButton() {
    ui_.actionConnect->setEnabled(false);
    ui_.actionDisconnect->setEnabled(true);
}

void MainWindow::InitActions() {
    EnableConnectButton();
    ui_.actionQuit->setEnabled(true);
    ui_.availableFileVersions->hide();
}

void MainWindow::ShowStatusMessage(const QString& message) {
    ui_.statusLabel->setText(message);
}

void MainWindow::on_browseFile_clicked() {
    flasher_->SetState(flasher::FlasherStates::kBrowseFile);
}

void MainWindow::on_loadFile_clicked() {
    flasher_->SetSelectedFileVersion(ui_.availableFileVersions->currentText());
    ui_.loadFile->setEnabled(false);
    ui_.progressBar->show();
    flasher_->SetState(flasher::FlasherStates::kLoadFile);
}

void MainWindow::on_enterBootloader_clicked() {
    if (flasher_->IsBootloaderDetected()) {
        flasher_->SetState(flasher::FlasherStates::kExitBootloader);
    } else {
        flasher_->SetState(flasher::FlasherStates::kEnterBootloader);
    }
}

void MainWindow::on_protectButton_clicked() {
    if (flasher_->IsReadProtectionEnabled()) {
        flasher_->SetState(flasher::FlasherStates::kDisableReadProtection);
    } else {
        flasher_->SetState(flasher::FlasherStates::kEnableReadProtection);
    }
}

} // namespace gui
