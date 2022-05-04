#include <QtTest/QtTest>
#include "tst_socket.h"
#include <QObject>

int main(int argc, char *argv[]) {
    int status = 0;
    status |= QTest::qExec(new TestSocket, argc, argv);
    //status |= QTest::qExec(new TestFlasher, argc, argv);

    return status;
}
