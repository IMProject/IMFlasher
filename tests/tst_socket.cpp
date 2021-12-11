#include "tst_socket.h"
#include <QMessageAuthenticationCode>

using namespace socket;

QHostAddress mock_address;
quint16 mock_port;

QByteArray read_all_data[10];
QByteArray send_data[10];

static uint8_t send_array_index = 0;
static uint8_t read_array_index = 0;

class MockSocket : public SocketClient
{
    void connectToHost(const QHostAddress &address, quint16 port, OpenMode mode = ReadWrite) override;
    bool waitForReadyRead(int msecs = 30000) override;
    bool ReadAll(QByteArray &data_out) override;
    bool SendData(const QByteArray &in_data) override;
};

void MockSocket::connectToHost(const QHostAddress &address, quint16 port, OpenMode mode)
{
    mock_address = address;
    mock_port = port;
    mode = ReadWrite;
}

bool MockSocket::waitForReadyRead(int msecs)
{
    if(msecs) {}
    return true;
}

bool MockSocket::ReadAll(QByteArray &data_out)
{
    data_out = read_all_data[read_array_index++];
    return true;
}

bool MockSocket::SendData(const QByteArray &in_data)
{
    send_data[send_array_index++] = in_data;
    return true;
}

TestSocket::TestSocket()
{
}

TestSocket::~TestSocket()
{
}

void TestSocket::TestSendBoardInfo()
{
    MockSocket socket;
    //Socket socket;

    send_array_index = 0;
    read_array_index = 0;

    QString bl_git_branch = "master";
    QString bl_git_hash = "be387ad0b2ba6dc0877e8e255e872ee310a9127c";
    QString bl_git_tag = "v1.1.0";

    QJsonObject tx_json_bl_version;
    tx_json_bl_version.insert("git_branch", bl_git_branch);
    tx_json_bl_version.insert("git_hash", bl_git_hash);
    tx_json_bl_version.insert("git_tag", bl_git_tag);

    QString fw_git_branch = "development";
    QString fw_git_hash = "877e8e255e872ee310a9127cbe387ad0b2ba6dc0";
    QString fw_git_tag = "v2.1.0";

    QJsonObject tx_json_fw_version;
    tx_json_fw_version.insert("git_branch", fw_git_branch);
    tx_json_fw_version.insert("git_hash", fw_git_hash);
    tx_json_fw_version.insert("git_tag", fw_git_tag);

    QString board_id = "test_board_id";
    QString manufacturer_id = "test_manufacturer_id";
    QString product_type = "test_product_type";

    QJsonObject tx_json_board_info;
    tx_json_board_info.insert("board_id", board_id);
    tx_json_board_info.insert("manufacturer_id", manufacturer_id);
    tx_json_board_info.insert("product_type", product_type);

    QByteArray token = "ABCD";
    read_all_data[0] = token;

    bool success = socket.SendBoardInfo(tx_json_board_info, tx_json_bl_version, tx_json_fw_version);
    QVERIFY2(success, "Send board data failed");

    QVERIFY2(send_data[0].toHex() == "f7e39d16c639f50c8e4882502c10697add67f1eef119b6acb60d1a224e3d4300", "Hash");

    QJsonDocument json_data = QJsonDocument::fromJson(send_data[1]);
    QJsonObject packet_object = json_data.object();

    QVERIFY2(packet_object.value("header") == kHeaderClientBoardInfo, "Sending board data header failed");

    QJsonObject rx_json_board_info = packet_object.value("board_info").toObject();
    QVERIFY2(rx_json_board_info.value("board_id") == board_id, "Sending board id failed");
    QVERIFY2(rx_json_board_info.value("manufacturer_id") == manufacturer_id, "Sending owner id failed");
    QVERIFY2(rx_json_board_info.value("product_type") == product_type, "Sending product type failed");

    QJsonObject rx_json_bl_version;
    rx_json_bl_version = packet_object.value("bl_version").toObject();
    QVERIFY2(rx_json_bl_version.value("git_branch") == bl_git_branch, "Sending bl version failed");
    QVERIFY2(rx_json_bl_version.value("git_hash") == bl_git_hash, "Sending bl version failed");
    QVERIFY2(rx_json_bl_version.value("git_tag") == bl_git_tag, "Sending bl version failed");

    QJsonObject rx_json_fw_version;
    rx_json_fw_version = packet_object.value("fw_version").toObject();
    QVERIFY2(rx_json_fw_version.value("git_branch") == fw_git_branch, "Sending fw version failed");
    QVERIFY2(rx_json_fw_version.value("git_hash") == fw_git_hash, "Sending fw version failed");
    QVERIFY2(rx_json_fw_version.value("git_tag") == fw_git_tag, "Sending fw version failed");
}

void TestSocket::TestReceiveProductType()
{
    MockSocket socket;
    //SocketClient socket;

    send_array_index = 0;
    read_array_index = 0;

    QString board_id = "test_board_id";
    QString manufacturer_id = "test_manufacturer_id";
    QString product_type = "test_product_type";

    QJsonObject tx_json_board_info;
    tx_json_board_info.insert("board_id", board_id);
    tx_json_board_info.insert("manufacturer_id", manufacturer_id);
    tx_json_board_info.insert("product_type", product_type);


    QJsonArray json_array;

    QString fw_version_1 = "v1.0.0";
    QString url_1 ="https://test.com/firmware1.bin";
    QString fw_version_2 = "v2.0.0";
    QString url_2 ="https://test.com/firmware2.bin";

    QJsonObject object1;
    object1.insert("fw_version", fw_version_1);
    object1.insert("url", url_1);
    json_array.append(object1);
    QJsonObject object2;
    object2.insert("fw_version", fw_version_2);
    object2.insert("url", url_2);
    json_array.append(object2);

    QJsonObject rx_packet_object;
    rx_packet_object.insert("header", kHeaderServerProductInfo);
    rx_packet_object.insert("product_info", json_array);

    QJsonDocument json_data;
    json_data.setObject(rx_packet_object);
    read_all_data[1] = json_data.toJson();

    QByteArray token = "ABCD";
    read_all_data[0] = token;

    QJsonArray rx_product_info;
    bool success = socket.ReceiveProductInfo(tx_json_board_info, rx_product_info);
    QVERIFY2(success, "Receive product info failed");

    json_data = QJsonDocument::fromJson(send_data[1]);
    QJsonObject packet_object = json_data.object();

    QVERIFY2(packet_object.value("header").toString() == kHeaderClientProductInfo, "Sending board data header failed");

    QJsonObject rx_json_board_info = packet_object.value("board_info").toObject();
    QVERIFY2(rx_json_board_info.value("board_id") == board_id, "Sending board id failed");
    QVERIFY2(rx_json_board_info.value("manufacturer_id") == manufacturer_id, "Sending owner id failed");
    QVERIFY2(rx_json_board_info.value("product_type") == product_type, "Sending product type failed");

    QString fw_version_test[10];
    QString url_test[10];
    foreach (const QJsonValue &value, rx_product_info)
    {
        static uint32_t index = 0;
        QJsonObject obj = value.toObject();
        fw_version_test[index] = obj["fw_version"].toString();
        url_test[index] = obj["url"].toString();
        index++;
    }

    QVERIFY(fw_version_test[0] == fw_version_1);
    QVERIFY(fw_version_test[1] == fw_version_2);
    QVERIFY(url_test[0] == url_1);
    QVERIFY(url_test[1] == url_2);
}


