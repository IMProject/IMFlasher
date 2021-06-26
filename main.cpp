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
#include "flasher.h"
#include "serial_port.h"

#include <QApplication>
#include <QThread>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    std::shared_ptr<Flasher> flasher = std::make_shared<Flasher>();
    MainWindow window(flasher);

    app.setWindowIcon(QIcon(":/images/capman.png"));

    // Run console solution
    if (argc >= 2) {
        QString actionString = argv[1];
        QString filePath = argv[2];

        //This is a blocking solution only for the console. No init needed.
        flasher->getSerialPort()->openConnBlocking();

        ManufacturerName manufactName = flasher->getSerialPort()->getManufactName();

        if (!(flasher->getSerialPort()->isBootloaderDetected())) {
            flasher->sendFlashCommandToApp();
            qInfo() << "Unplug USB run this app again and plug USB! ";

        } else if ((manufactName == ManufacturerName::IMBOOT) || (manufactName == ManufacturerName::MICROSOFT)) {

            if (flasher->collectBoardId()) {

                if (flasher->getBoardKey()) {

                    if (0 == QString::compare("erase", actionString, Qt::CaseInsensitive)) {

                        if (flasher->startErase()) {
                            qInfo() << "Erase success";

                        } else {
                            qInfo() << "Erase error";
                        }

                    } else if (0 == QString::compare("flash", actionString, Qt::CaseInsensitive)) {

                        if (flasher->openFirmwareFile(filePath)) {

                            std::tuple<bool, QString, QString> flashingInfo = flasher->startFlash();

                            if (std::get<0>(flashingInfo)) {
                                qInfo() << "Flash success";

                            } else {
                                qInfo() << "Flash error";
                            }

                        } else {
                            qInfo() << "Open firmware file error";
                        }

                    } else {
                         qInfo() << "Select flash or erase";
                    }
                }
            }

        } else {
            qInfo() << "Wrong board connected!";
        }

    //Run GUI solution
    } else {
        flasher->init();
        window.show();
        app.exec();
    }

    return 0;
}
