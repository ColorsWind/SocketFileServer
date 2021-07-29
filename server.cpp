#include <iostream>
#include <winsock2.h>
#include <string>
#include <sstream>
#include <windows.h>
#include <atomic>
#include <ctime>
#include "common.h"

using std::cout;
using std::cerr;
using std::string;
using std::stringstream;
using std::endl;

std::atomic_int online{0};

class ClientSession {
private:
    SOCKET client_socket;
    char buf[BUFFER_SIZE + 1]{};
    char text[MAX_TEXT]{};

public:
    explicit ClientSession(SOCKET clientSocket) : client_socket(clientSocket) {}

    void sendWelcome() {
        sockaddr_in client_addr{};
        int addr_size = sizeof(sockaddr_in);
        if (getpeername(client_socket, reinterpret_cast<sockaddr *>(&client_addr), &addr_size) == 0) {
            cout << "接收连接请求: " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << endl;
        } else {
            cout << "接收连接请求: unknown" << endl;
        }
        char welcome[1024];
        stringstream ss;
        time_t now = time(nullptr);
        ctime_s(welcome, sizeof(welcome), &now);

        ss << "欢迎!\t在线用户:" << (int)(atomic_fetch_add(&online, 1) + 1) << "\t服务器时间: " << (char*)welcome << endl;
        ss.getline(welcome, sizeof(welcome));
        int len = (int)strnlen_s(welcome, sizeof(welcome)) + 1;
        sendInfoSegment(&client_socket, buf, len, MESSAGE);
        sendTextSegment(&client_socket, welcome, len);
    }

    void handleClientRequest(int len) {
        receiveTextSegment(&client_socket, text, len, true);
        string command, parameter;
        splitText(text, &command, &parameter);
        cout << "接收到命令:" << command << "   \t参数:" << parameter << endl;
        if (command == "echo") {
            int size = (int) parameter.length() + 1;
            sendInfoSegment(&client_socket, buf, size, MESSAGE);
            sendTextSegment(&client_socket, parameter.c_str(), size);
        } else if (command == "list") {
            stringstream ss;
            WIN32_FIND_DATAA findData;
            HANDLE hFind = INVALID_HANDLE_VALUE;

            hFind = FindFirstFileA(".\\*", &findData);

            if (hFind == INVALID_HANDLE_VALUE)
                throw std::runtime_error("Invalid handle value! Please check your path...");

            while (FindNextFileA(hFind, &findData) != 0)
            {
                if (strcmp("..", findData.cFileName) == 0)
                    continue;
                ss << findData.cFileName << "\t";
            }

            FindClose(hFind);
            ss << endl;
            ss.getline(text, sizeof(text));
            int size = (int) strlen(text) + 1;
            // send respond
            sendInfoSegment(&client_socket, buf, size, MESSAGE);
            sendTextSegment(&client_socket, text, size);
        } else if (command == "upload") {
            TransportType type;
            int file_size;
            std::ofstream out_file;

            receiveInfoSegment(&client_socket, buf, &type, &file_size);

            out_file.open(parameter, std::ios::out | std::ios::binary);
            if (out_file.fail()) {
                // check
                sendInfoSegment(&client_socket, buf, 0, MESSAGE);
                char message[] = "打开远程文件输出流失败, 无法上传.";
                sendInfoSegment(&client_socket, buf, sizeof(message), MESSAGE);
                sendTextSegment(&client_socket, message, sizeof(message));
                cout << "客户端上传文件: " << parameter << "\t失败." << endl;
            } else {
                sendInfoSegment(&client_socket, buf, file_size, BINARY);
                char message[] = "成功打开远程文件输出流,开始上传文件...";
                sendInfoSegment(&client_socket, buf, sizeof(message), MESSAGE);
                sendTextSegment(&client_socket, message, sizeof(message));
                cout << "客户端上传文件: " << parameter << "\t文件大小: " << file_size << endl;
            }
            int result = receiveFileSegment(&client_socket, buf, &out_file, file_size, false);
            if (result == 0)
                cerr << "文件上传失败, 客户端关闭了连接, 代码: 0" << endl;
            else if (result == SOCKET_ERROR)
                cerr << "文件上传失败, 错误代码: " << WSAGetLastError() << endl;
            else
                cout << "上传文件成功" << endl;
        } else if (command == "download") {
            std::ifstream in_file;
            in_file.open(parameter, std::ios_base::binary);
            if (in_file.fail()) {
                char message[] = "打开远程文件输入流失败, 无法下载.";
                sendInfoSegment(&client_socket, buf, 0, MESSAGE); // 文件大小 0 代表失败
                sendInfoSegment(&client_socket, buf, sizeof(message), MESSAGE);
                sendTextSegment(&client_socket, message, sizeof(message));
                cout << "客户端下载文件: " << parameter << "\t失败." << endl;
            } else {
                int size = (int) getFileSize(&in_file);
                sendInfoSegment(&client_socket, buf, size, BINARY);
                char message[] = "成功打开远程输出流,准备下载文件...";
                sendInfoSegment(&client_socket, buf, sizeof(message), MESSAGE);
                sendTextSegment(&client_socket, message, sizeof(message));
                cout << "客户端下载文件: " << parameter << "\t文件大小: " << size << endl;
                int result = sendFileSegment(&client_socket, buf, &in_file, size, false);
                if (result == 0)
                    cerr << "客户端下载文件失败, 因为客户端关闭了连接, 代码: 0" << endl;
                else if (result == SOCKET_ERROR)
                    cerr << "客户端下载文件失败, 因为出现错误, 代码: " << WSAGetLastError() << endl;
                else
                    cout << "客户端下载文件成功" << endl;
            }
        } else {
            char message[] = "未知命令.";
            sendInfoSegment(&client_socket, buf, sizeof(message), MESSAGE);
            sendTextSegment(&client_socket, message, sizeof(message));
        }
    }

    int receiveClientRequest(TransportType *type, int *size) {
        return receiveInfoSegment(&client_socket, buf, type, size);
    }

};


DWORD WINAPI handleThread(LPVOID p) {
    auto *client_socket = static_cast<SOCKET *>(p);
    ClientSession session(*client_socket);
    TransportType type;
    int size;
    session.sendWelcome();
    while (true) {
        int count = session.receiveClientRequest(&type, &size);
        if (count == 0) {
            cout << "关闭连接" << endl;
            break;
        } else if (count == SOCKET_ERROR) {
            cout << "接收 Client 数据出错: " << WSAGetLastError() << endl;
            break;
        }

        session.handleClientRequest(size);
    }
    online--;
    return 0;
}

int main() {
    // setup winsocket
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // create a socket
    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        cout << "创建 socket 失败: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    // bind
    SOCKADDR_IN addr_server;
    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_server.sin_port = htons(port);
    if (bind(server, reinterpret_cast<const sockaddr *>(&addr_server), sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {
        cout << "bind 失败: " << WSAGetLastError();
        closesocket(server);
        WSACleanup();
        return 2;
    }

    // listen
    if (listen(server, 5) == SOCKET_ERROR) {
        cout << "listen 失败: " << WSAGetLastError();
        closesocket(server);
        WSACleanup();
        return 3;
    }

    // loop accept socket
    SOCKADDR addr_client;
    int len = sizeof(SOCKADDR);
    while (true) {
        SOCKET accept_socket = accept(server, reinterpret_cast<sockaddr *>(&addr_client), &len);
        if (accept_socket == INVALID_SOCKET) {
            cerr << "收到一个无效的 Socket 连接." << endl;
            continue;
        }

        HANDLE hThread;
        DWORD threadId;
        hThread = CreateThread(nullptr, 0, handleThread, &accept_socket, 0, &threadId);
        if (!hThread) {
            closesocket(accept_socket);
            cerr << "创建线程失败..." << endl;
            break;
        }
        CloseHandle(hThread);
    }
}
