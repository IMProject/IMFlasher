#include "tst_socket.h"

#include <vector>
#include <QMessageAuthenticationCode>

constexpr char kDefaultAddress1[] {"127.0.0.1"};
constexpr char kDefaultAddress2[] {"127.0.0.2"}; // "localhost" doesn't work for some reason
constexpr int kDefaultPort = 5322;
constexpr char kDefaultKey[] {"NDQ4N2Y1YjFhZTg3ZGI3MTA1MjlhYmM3"};

void CreateServersArray(QJsonArray& json_array)
{
    QJsonObject json_object_server_1;
    QJsonObject json_object_server_2;

    json_object_server_1.insert("address", kDefaultAddress1);
    json_object_server_1.insert("port", kDefaultPort);
    json_object_server_1.insert("preshared_key", kDefaultKey);
    json_array.append(json_object_server_1);

    json_object_server_2.insert("address", kDefaultAddress2);
    json_object_server_2.insert("port", kDefaultPort);
    json_object_server_2.insert("preshared_key", kDefaultKey);
    json_array.append(json_object_server_2);
}

class MockSocket_1 : public socket::SocketClient
{
  public:
    MockSocket_1(const QJsonArray& servers_array) : SocketClient(servers_array)
    {}

    std::vector<QByteArray> read_data_;
    std::vector<QByteArray> send_data_;
  private:
    uint8_t read_array_index_ {0U};

    bool ReadAll(QByteArray& data_out) override;
    bool SendData(const QByteArray& in_data) override;
    bool waitForConnected(int msecs = 30000) override;
};

bool MockSocket_1::ReadAll(QByteArray& data_out)
{
    data_out = read_data_.at(read_array_index_++);
    return true;
}

bool MockSocket_1::SendData(const QByteArray& in_data)
{
    send_data_.emplace_back(in_data);
    return true;
}

bool MockSocket_1::waitForConnected(int msecs)
{
    if (msecs) {};
    return true;
}


class MockSocket_2 : public socket::SocketClient
{
  public:
    MockSocket_2(const QJsonArray& servers_array) : SocketClient(servers_array)
    {}

  private:
    bool ReadAll(QByteArray& data_out) override;
    bool waitForConnected(int msecs = 30000) override;
};

bool MockSocket_2::ReadAll(QByteArray& data_out)
{
    data_out = QByteArrayLiteral("ABCD");
    return false;
}

bool MockSocket_2::waitForConnected(int msecs)
{
    if (msecs) {};
    return true;
}


class MockSocket_3 : public socket::SocketClient
{
  public:
    MockSocket_3(const QJsonArray& servers_array) : SocketClient(servers_array)
    {}

  private:
    bool ReadAll(QByteArray& data_out) override;
    bool SendData(const QByteArray& in_data) override;
    bool waitForConnected(int msecs = 30000) override;
};

bool MockSocket_3::ReadAll(QByteArray& data_out)
{
    data_out = QByteArrayLiteral("ABCD");
    return true;
}

bool MockSocket_3::SendData(const QByteArray& in_data)
{
    if (in_data.isEmpty()) {};
    return false;
}

bool MockSocket_3::waitForConnected(int msecs)
{
    if (msecs) {};
    return true;
}


TestSocket::TestSocket() = default;
TestSocket::~TestSocket() = default;

void TestSocket::TestSendBoardInfo()
{
    QJsonArray servers_array;
    CreateServersArray(servers_array);
    MockSocket_1 socket(servers_array);

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
    socket.read_data_.emplace_back(token);

    bool success = socket.SendBoardInfo(tx_json_board_info, tx_json_bl_version, tx_json_fw_version);
    QVERIFY2(success, "Send board data failed");

    QVERIFY2(socket.send_data_.at(0).toHex() == "f7e39d16c639f50c8e4882502c10697add67f1eef119b6acb60d1a224e3d4300", "Hash");

    QJsonDocument json_data = QJsonDocument::fromJson(socket.send_data_.at(1));
    QJsonObject packet_object = json_data.object();

    QVERIFY2(packet_object.value("header") == socket::kHeaderClientBoardInfo, "Sending board data header failed");

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
    QJsonArray servers_array;
    CreateServersArray(servers_array);
    MockSocket_1 socket(servers_array);

    QString board_id = "test_board_id";
    QString manufacturer_id = "test_manufacturer_id";
    QString product_type = "test_product_type";

    QJsonObject tx_json_board_info;
    tx_json_board_info.insert("board_id", board_id);
    tx_json_board_info.insert("manufacturer_id", manufacturer_id);
    tx_json_board_info.insert("product_type", product_type);

    QJsonArray json_array;

    QString fw_version_1 = "v1.0.0";
    QString url_1 = "https://test.com/firmware1.bin";
    QString fw_version_2 = "v2.0.0";
    QString url_2 = "https://test.com/firmware2.bin";

    QJsonObject object1;
    object1.insert("fw_version", fw_version_1);
    object1.insert("url", url_1);
    json_array.append(object1);
    QJsonObject object2;
    object2.insert("fw_version", fw_version_2);
    object2.insert("url", url_2);
    json_array.append(object2);

    QJsonObject rx_packet_object;
    rx_packet_object.insert("header", socket::kHeaderServerProductInfo);
    rx_packet_object.insert("product_info", json_array);

    QJsonDocument json_data;
    json_data.setObject(rx_packet_object);

    QByteArray token = "ABCD";
    socket.read_data_.emplace_back(token);
    socket.read_data_.emplace_back(json_data.toJson());

    QJsonArray rx_product_info;
    bool success = socket.ReceiveProductInfo(tx_json_board_info, rx_product_info);
    QVERIFY2(success, "Receive product info failed");

    json_data = QJsonDocument::fromJson(socket.send_data_.at(1));
    QJsonObject packet_object = json_data.object();

    QVERIFY2(packet_object.value("header").toString() == socket::kHeaderClientProductInfo, "Sending board data header failed");

    QJsonObject rx_json_board_info = packet_object.value("board_info").toObject();
    QVERIFY2(rx_json_board_info.value("board_id") == board_id, "Sending board id failed");
    QVERIFY2(rx_json_board_info.value("manufacturer_id") == manufacturer_id, "Sending owner id failed");
    QVERIFY2(rx_json_board_info.value("product_type") == product_type, "Sending product type failed");

    std::vector<QString> fw_version_test;
    std::vector<QString> url_test;
    foreach (const QJsonValue& value, rx_product_info)
    {
        QJsonObject obj = value.toObject();
        fw_version_test.emplace_back(obj["fw_version"].toString());
        url_test.emplace_back(obj["url"].toString());
    }

    QVERIFY(fw_version_test.at(0) == fw_version_1);
    QVERIFY(fw_version_test.at(1) == fw_version_2);
    QVERIFY(url_test.at(0) == url_1);
    QVERIFY(url_test.at(1) == url_2);
}

void TestSocket::TestReadFail()
{
    QJsonArray servers_array;
    CreateServersArray(servers_array);
    MockSocket_2 socket(servers_array);

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

    bool success = socket.SendBoardInfo(tx_json_board_info, tx_json_bl_version, tx_json_fw_version);
    QVERIFY2(!success, "Read data did not fail");
}

void TestSocket::TestSendFail()
{
    QJsonArray servers_array;
    CreateServersArray(servers_array);
    MockSocket_3 socket(servers_array);

    QString board_id = "test_board_id";
    QString manufacturer_id = "test_manufacturer_id";
    QString product_type = "test_product_type";

    QJsonObject tx_json_board_info;
    tx_json_board_info.insert("board_id", board_id);
    tx_json_board_info.insert("manufacturer_id", manufacturer_id);
    tx_json_board_info.insert("product_type", product_type);

    QJsonArray rx_product_info;

    bool success = socket.ReceiveProductInfo(tx_json_board_info, rx_product_info);
    QVERIFY2(!success, "Send data did not fail");
}
