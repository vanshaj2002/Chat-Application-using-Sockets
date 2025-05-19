#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

class UDPServer {
public:
    UDPServer(const string& ip, int port);
    ~UDPServer();
    bool init();
    bool bindSocket();
    bool recvData();
    bool sendData(const string& data);

private:
    string ip;
    int port;
    SOCKET serverSocket;
    sockaddr_in serverAddr;
    sockaddr_in lastClientAddr;  // Store the address of the last client
    int lastClientAddrSize;      // Size of the last client's address
    char buffer[1400];
    bool isValidIpAddress(const string& ip);
    bool isValidPort(int port);
    bool isValidMessage(const string& message);
};

UDPServer::UDPServer(const string& ip, int port) : ip(ip), port(port), serverSocket(INVALID_SOCKET) {
    memset(&serverAddr, 0, sizeof(serverAddr));
    memset(&lastClientAddr, 0, sizeof(lastClientAddr));
    lastClientAddrSize = sizeof(lastClientAddr);
}

UDPServer::~UDPServer() {
    closesocket(serverSocket);
    WSACleanup();
}

bool UDPServer::isValidIpAddress(const string& ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 0;
}

bool UDPServer::isValidPort(int port) {
    return port > 0 && port <= 65535;
}

bool UDPServer::isValidMessage(const string& message) {
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

bool UDPServer::init() {
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

    serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed: " << WSAGetLastError() << endl;
        return false;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    // Set receive timeout to 20 seconds (20000 milliseconds)
    int timeout = 20000;  // Updated timeout value
    setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    return true;
}

bool UDPServer::bindSocket() {
    int result = bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result == SOCKET_ERROR) {
        cerr << "Bind failed: " << WSAGetLastError() << endl;
        return false;
    }
    return true;
}

bool UDPServer::recvData() {
    memset(buffer, 0, sizeof(buffer));

    int bytesReceived = recvfrom(serverSocket, buffer, sizeof(buffer), 0, (sockaddr*)&lastClientAddr, &lastClientAddrSize);
    if (bytesReceived == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAETIMEDOUT) {
            cout << "No data received in the last 20 seconds. Waiting for the next client." << endl;
            return true; // Continue waiting
        }
        else {
            cerr << "Receive failed: " << error << endl;
            return false;
        }
    }

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(lastClientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);

    string receivedMessage(buffer, bytesReceived);
    cout << "Received from " << clientIP << ":" << ntohs(lastClientAddr.sin_port) << " [" << bytesReceived << " bytes]: " << receivedMessage << endl;

    if (!isValidMessage(receivedMessage)) {
        cout << "Invalid message received. Exiting." << endl;
        return false;
    }

    return true;
}

bool UDPServer::sendData(const string& data) {
    if (data.empty()) {
        cerr << "Send failed: Message is empty" << endl;
        return false;
    }

    if (data.size() > 1400) {
        cerr << "Send failed: Message exceeds 1400 characters" << endl;
        return false;
    }

    int bytesSent = sendto(serverSocket, data.c_str(), data.size(), 0, (sockaddr*)&lastClientAddr, lastClientAddrSize);
    if (bytesSent == SOCKET_ERROR) {
        cerr << "Send failed: " << WSAGetLastError() << endl;
        return false;
    }

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(lastClientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);

    cout << "Sent to " << clientIP << ":" << ntohs(lastClientAddr.sin_port) << " [" << bytesSent << " bytes]: " << data << endl;
    return true;
}

int main() {
    UDPServer server("127.0.0.1", 8080);
    if (!server.init()) return 1;
    if (!server.bindSocket()) return 1;

    cout << "Server listening on 127.0.0.1:8080" << endl;

    while (true) {
        if (!server.recvData()) break;

        // Prompt the user to enter a response and send it to the client
        cout << "Enter your response to the client: ";
        string response;
        getline(cin, response);

        // Ensure the response is valid before sending
        if (!server.sendData(response)) {
            cerr << "Failed to send data to client." << endl;
            break;
        }
    }

    return 0;
}

