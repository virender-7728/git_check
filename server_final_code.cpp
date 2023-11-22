#include <iostream>
#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include "json-1.hpp"
#include <sstream>
#include <stdexcept>
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <fstream>
#define EXAMPLE_HOST "localhost"
#define EXAMPLE_USER "Virender"
#define EXAMPLE_PASS "password"
#define EXAMPLE_DB "systeminfo"
using namespace std;
using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

class DatabaseManager
{
public:
    void insertData(const string &timeStampStr, double cpuUtilization, double hddUtilization, double ramUsage, int rxPackets, double systemIdleTime, const string &systemName)
    {
        try
        {
            const string url = EXAMPLE_HOST;
            const string user = EXAMPLE_USER;
            const string pass = EXAMPLE_PASS;
            const string database = EXAMPLE_DB;
            sql::Driver *driver = get_driver_instance();
            std::unique_ptr<sql::Connection> con(driver->connect(url, user, pass));
            con->setSchema(database);
            sql::PreparedStatement *pstmt;
            pstmt = con->prepareStatement("INSERT INTO System_Information_2 (TIME_STAMP, CPU_UTILIZATION, HDD_UTILIZATION, RAM_USAGE, RX_PACKETS, SYSTEM_IDLE_TIME,SYSTEM_NAME) VALUES (?, ?, ?, ?, ?, ?,?)");

            pstmt->setString(1, timeStampStr);
            pstmt->setDouble(2, cpuUtilization);
            pstmt->setDouble(3, hddUtilization);
            pstmt->setDouble(4, ramUsage);
            pstmt->setInt(5, rxPackets);
            pstmt->setDouble(6, systemIdleTime);
            pstmt->setString(7, systemName);


            pstmt->execute();
            delete pstmt;
        }
        catch (sql::SQLException &e)
        {
            cerr << "MySQL Error: " << e.what() << endl;
        }
    }
};

class ClientHandler : public enable_shared_from_this<ClientHandler>
{
private:
    websocket::stream<tcp::socket> ws_;
    boost::beast::flat_buffer buffer_;
    tcp::endpoint remote_endpoint_;
    DatabaseManager db_manager;

public:
    explicit ClientHandler(tcp::socket socket)
        : ws_(move(socket))
    {
        remote_endpoint_ = ws_.next_layer().remote_endpoint();
        cout << endl
             << "Client connected: " << remote_endpoint_ << endl;
        cout << endl;
    }

    ~ClientHandler()
    {
        cout << endl
             << "Client disconnected: " << remote_endpoint_ << endl;
    }

    void start()
    {
        ws_.async_accept(
            boost::asio::bind_executor(
                ws_.get_executor(),
                bind(
                    &ClientHandler::onAccept,
                    shared_from_this(),
                    placeholders::_1)));
    }

private:
    void onAccept(boost::beast::error_code ec)
    {
        if (ec)
        {
            cerr << "WebSocket accept error: " << ec.message() << endl;
            return;
        }

        doRead();
    }

    void doRead()
    {
        ws_.async_read(
            buffer_,
            boost::asio::bind_executor(
                ws_.get_executor(),
                bind(
                    &ClientHandler::onRead,
                    shared_from_this(),
                    placeholders::_1,
                    placeholders::_2)));
    }
    void saveDataToCSV(const json &data)
    {
        ofstream csvFile("client_data.csv", std::ios::app); // Open the CSV file for appending
        if (csvFile.is_open())
        {
            if (csvFile.tellp() == 0)
            {
                csvFile << "Time Stamp,CPU Utilization,HDD Utilization,RAM Usage,RX Packets,System Idle Window,System Name" << endl; // Add your column names here
            }

        string systemName = data["system_name"];
        systemName.erase(remove(systemName.begin(), systemName.end(), '\n'), systemName.end());

    csvFile << data["Time_Stamp"] << "," 
            << data["cpu_utilization"] << "," 
            << data["hdd_utilization"] << ","
            << data["ram_usage"] << "," 
            << data["rx_packets"] << "," 
            << data["system_idle_time"]  <<  "," 
            << systemName << endl;

            csvFile.close();
        }
        else
        {
            cerr << "Failed to open CSV file for writing." << std::endl;
        }


        
    }

    void onRead(boost::beast::error_code ec, size_t)
    {
        if (ec == websocket::error::closed)
            return;

        if (ec)
        {
            cerr << endl
                 << "WebSocket read message: " << ec.message() << endl;
            return;
        }

        auto now = chrono::system_clock::now();
        time_t timestamp = std::chrono::system_clock::to_time_t(now);
        tm tm = *localtime(&timestamp);

        // Format the timestamp as a string
        stringstream ss;
        ss << put_time(&tm, "%H:%M:%S");
        string timestampStr = ss.str();

        // Echo the received message back to the client
        string message = boost::beast::buffers_to_string(buffer_.data());
        cout << endl;
        //<< "Received message from " << remote_endpoint_ << endl;

        cout << "Time Stamp: " << timestampStr << endl;

        try
        {
            json jsonData = json::parse(message);
            // Access the individual fields
            double cpuUtilization = jsonData["cpu_utilization"];
            double hddUtilization = jsonData["hdd_utilization"];
            double ramUsage = jsonData["ram_usage"];
            int rxPackets = jsonData["rx_packets"];
            double systemIdleTime = jsonData["system_idle_time"];
            string systemName = jsonData["system_name"];
            cout << "System Name: " << systemName;
            cout << "CPU Utilization: " << cpuUtilization << "%" << endl;
            cout << "RAM Usage: " << ramUsage << " MB" << endl;
            cout << "System Idle Time: " << systemIdleTime << " seconds" << endl;
            cout << "HDD Utilization: " << hddUtilization << "%" << endl;
            cout << "Recieved Packets: " << rxPackets << endl;

            jsonData["Time_Stamp"] = timestampStr;
            saveDataToCSV(jsonData);
            //  Insert data into the database
            db_manager.insertData(timestampStr, cpuUtilization, hddUtilization, ramUsage, rxPackets, systemIdleTime, systemName);
        }
        catch (const exception &e)
        {
            cerr << "Exception: " << e.what() << endl;
        }
        // Clear the buffer and start a new read operation
        buffer_.consume(buffer_.size());
        doRead();
    }
};

// The WebSocketServer and main function remain the same as in your original code
class WebSocketServer
{
private:
    boost::asio::io_context &io_context_;
    tcp::acceptor acceptor_;

public:
    WebSocketServer(boost::asio::io_context &io_context, short port)
        : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {}

    void startAccept()
    {
        acceptor_.async_accept(
            [this](boost::beast::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    make_shared<ClientHandler>(move(socket))->start();
                }
                startAccept();
            });
    }
};

int main()
{
    try
    {
        boost::asio::io_context io_context;

        const short port = 8080;
        WebSocketServer server(io_context, port);
        server.startAccept();

        cout << "Server listening on port " << port << "..." << endl;
        cout << endl;

        io_context.run();
    }
    catch (const exception &e)
    {
        cerr << "Exception: " << e.what() << endl;
    }
    return 0;
}