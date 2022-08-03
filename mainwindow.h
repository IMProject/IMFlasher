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

#ifndef MAINWINDOW_H_
#define MAINWINDOW_H_

#include <QMainWindow>
#include <QJsonArray>

#include "ui_mainwindow.h"

namespace flasher {
class Flasher;
} // namespace flasher

namespace gui {

/*!
 * \brief The MainWindow class
 */
class MainWindow : public QMainWindow {

    Q_OBJECT

  public:
    /*!
     * \brief MainWindow constructor
     * \param flasher - Shared pointer to the Flasher object
     */
    explicit MainWindow(std::shared_ptr<flasher::Flasher> flasher);

    /*!
     * \brief MainWindow destructor
     */
    ~MainWindow();

  private slots:
    /*!
     * \brief on_browseFile_clicked
     */
    void on_browseFile_clicked();

    /*!
     * \brief on_loadFile_clicked
     */
    void on_loadFile_clicked();

    /*!
     * \brief on_enterBootloader_clicked
     */
    void on_enterBootloader_clicked();

    /*!
     * \brief on_protectButton_clicked
     */
    void on_protectButton_clicked();

  private:
    Ui::MainWindow ui_;                                         //!< Ui::MainWindow
    std::shared_ptr<flasher::Flasher> flasher_;                 //!< Shared pointer to the flasher object

    //! Version information constant, contains git tag, git branch and git hash
    const std::string version_info_ =
        std::string("The <b>IMFlasher</b> ") + GIT_TAG + "<br>" +
        "Branch: " + GIT_BRANCH + "<br>" +
        "Hash: " + GIT_HASH;

    /*!
     * \brief Method used to connect actions
     */
    void ConnectActions();

    /*!
     * \brief Method used to clear progress
     */
    void ClearProgress();

    /*!
     * \brief Method used to disable all buttons
     */
    void DisableAllButtons();

    /*!
     * \brief Method used to enable connect button and disable disconnect button
     */
    void EnableConnectButton();

    /*!
     * \brief Method used to enable disconnect button and disable connect button
     */
    void EnableDisconnectButton();

    /*!
     * \brief Method used to initialize actions
     */
    void InitActions();

    /*!
     * \brief Method used to show status message
     * \param message - Message that will be shown
     */
    void ShowStatusMessage(const QString& message);

    /*!
     * \brief Method used to set file list
     * \param product_info - Json array that presents product information
     */
    void SetFileList(const QJsonArray& product_info);
};

} // namespace gui
#endif // MAINWINDOW_H_
