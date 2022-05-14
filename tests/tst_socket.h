#pragma once

#include <QtTest>
#include "socket_client.h"

class TestSocket : public QObject {

    Q_OBJECT

  public:
    TestSocket();
    ~TestSocket();

  private slots:
    void TestSendBoardInfo();
    void TestReceiveProductType();
    void TestReadFail();
    void TestSendFail();
};
