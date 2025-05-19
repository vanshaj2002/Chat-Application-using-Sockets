#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

class UDPClient {
public:
    UDPClient(const string& ip, int port);
    ~UDPClient();
    bool init();
    bool sendData(const string& data);
    bool recvData();

private:
    string ip;
    int port;
    SOCKET clientSocket;
    sockaddr_in serverAddr;
    char buffer[1400];
    bool isValidIpAddress(const string& ip);
    bool isValidPort(int port);
    bool isValidMessage(const string& message);
};

UDPClient::UDPClient(const string& ip, int port) : ip(ip), port(port), clientSocket(INVALID_SOCKET) {
    memset(&serverAddr, 0, sizeof(serverAddr));
}

UDPClient::~UDPClient() {
    closesocket(clientSocket);
    WSACleanup();
}

bool UDPClient::isValidIpAddress(const string& ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 0;
}

bool UDPClient::isValidPort(int port) {
    return port > 0 && port <= 65535;
}

bool UDPClient::isValidMessage(const string& message) {
    if (message.empty()) {
        cerr << "Empty message received." << endl;
        return false;
    }

    // Check if the message is all lowercase
    for (char c : message) {
        if (!islower(c) && !isspace(c)) {
            cerr << "Invalid message format: '" << message << "'" << endl;
            return false;
        }
    }

    return true;
}

bool UDPClient::init() {
    if (!isValidIpAddress(ip)) {
        cerr << "Invalid IP address." << endl;
        return false;
    }

    if (!isValidPort(port)) {
        cerr << "Invalid port number. Must be in the range 1-65535." << endl;
        return false;
    }

    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cerr << "WSAStartup failed: " << result << endl;
        return false;
    }

    clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed: " << WSAGetLastError() << endl;
        return false;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    // Set receive timeout to 20 seconds (20000 milliseconds)
    int timeout = 20000;  // Updated timeout value
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    return true;
}

bool UDPClient::sendData(const string& data) {
    if (data.empty()) {
        cerr << "Send failed: Message is empty" << endl;
        return false;
    }

    if (data.size() > 1400) {
        cerr << "Send failed: Message exceeds 1400 characters" << endl;
        return false;
    }

    int bytesSent = sendto(clientSocket, data.c_str(), data.size(), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (bytesSent == SOCKET_ERROR) {
        cerr << "Send failed: " << WSAGetLastError() << endl;
        return false;
    }

    cout << "Sent to " << ip << ":" << port << " [" << bytesSent << " bytes]: " << data << endl;
    return true;
}

bool UDPClient::recvData() {
    memset(buffer, 0, sizeof(buffer));

    int bytesReceived = recvfrom(clientSocket, buffer, sizeof(buffer), 0, nullptr, nullptr);
    if (bytesReceived == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAETIMEDOUT) {
            cout << "No data received in the last 20 seconds. Waiting for the next message." << endl;
            return true; // Continue waiting
        }
        else {
            cerr << "Receive failed: " << error << endl;
            return false;
        }
    }

    string receivedMessage(buffer, bytesReceived);
    cout << "Received: " << receivedMessage << endl;

    if (!isValidMessage(receivedMessage)) {
        cout << "Invalid message format received. Exiting." << endl;
        return false;
    }

    return true;
}

int main() {
    UDPClient client("127.0.0.1", 8080);
    if (!client.init()) return 1;

    while (true) {
        // Prompt the user to enter a message and send it to the server
        cout << "Enter your message: ";
        string message;
        getline(cin, message);

        if (!client.sendData(message)) break;

        // Receive and display the server's response
        if (!client.recvData()) break;
    }

    return 0;
}
