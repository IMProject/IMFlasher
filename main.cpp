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

#include "mainwindow.h"
#include "flasher.h"

#include <QApplication>
#include <QThread>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    Flasher* flasher = window.getFlasherPtr();

    app.setWindowIcon(QIcon(":/images/capman.png"));

    //Console solution
    if(argc>=2)
    {
        QString actionString = argv[1];
        QString filePath = argv[2];

        //This is a blocking solution only for the console. No init needed.
        flasher->getSerialPort()->openConnBlocking();

        manufact_name manufactName = flasher->getSerialPort()->getManufactName();

        bool isBootloader = flasher->getSerialPort()->isBootloaderDetected();

        if(!isBootloader) {
            flasher->sendFlashCommandToApp();
            qInfo() << "Unplug USB run this app again and plug USB! ";

        } else if((manufactName == MANUFACT_NAME_IMBOOT) || (manufactName == MANUFACT_NAME_MICROSOFT)) {

            bool success = true;

            if(success) {
                success = flasher->collectBoardId();
            }

            if(success) {
                success = flasher->getBoardKey();
            }

            if(success ) {

                if(actionString == "erase")
                {
                    flasher->startErase();

                } else if(actionString == "flash"){

                    flasher->openFirmwareFile(filePath);
                    flasher->startFlash();

                } else {
                     qInfo() << "Select flash or erase";
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
