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
            cout << "������������: " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << endl;
        } else {
            cout << "������������: unknown" << endl;
        }
        char welcome[1024];
        stringstream ss;
        time_t now = time(nullptr);
        ctime_s(welcome, sizeof(welcome), &now);

        ss << "��ӭ!\t�����û�:" << (int)(atomic_fetch_add(&online, 1) + 1) << "\t������ʱ��: " << (char*)welcome << endl;
        ss.getline(welcome, sizeof(welcome));
        int len = (int)strnlen_s(welcome, sizeof(welcome)) + 1;
        sendInfoSegment(&client_socket, buf, len, MESSAGE);
        sendTextSegment(&client_socket, welcome, len);
    }

    void handleClientRequest(int len) {
        receiveTextSegment(&client_socket, text, len, true);
        string command, parameter;
        splitText(text, &command, &parameter);
        cout << "���յ�����:" << command << "   \t����:" << parameter << endl;
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
                char message[] = "��Զ���ļ������ʧ��, �޷��ϴ�.";
                sendInfoSegment(&client_socket, buf, sizeof(message), MESSAGE);
                sendTextSegment(&client_socket, message, sizeof(message));
                cout << "�ͻ����ϴ��ļ�: " << parameter << "\tʧ��." << endl;
            } else {
                sendInfoSegment(&client_socket, buf, file_size, BINARY);
                char message[] = "�ɹ���Զ���ļ������,��ʼ�ϴ��ļ�...";
                sendInfoSegment(&client_socket, buf, sizeof(message), MESSAGE);
                sendTextSegment(&client_socket, message, sizeof(message));
                cout << "�ͻ����ϴ��ļ�: " << parameter << "\t�ļ���С: " << file_size << endl;
            }
            int result = receiveFileSegment(&client_socket, buf, &out_file, file_size, false);
            if (result == 0)
                cerr << "�ļ��ϴ�ʧ��, �ͻ��˹ر�������, ����: 0" << endl;
            else if (result == SOCKET_ERROR)
                cerr << "�ļ��ϴ�ʧ��, �������: " << WSAGetLastError() << endl;
            else
                cout << "�ϴ��ļ��ɹ�" << endl;
        } else if (command == "download") {
            std::ifstream in_file;
            in_file.open(parameter, std::ios_base::binary);
            if (in_file.fail()) {
                char message[] = "��Զ���ļ�������ʧ��, �޷�����.";
                sendInfoSegment(&client_socket, buf, 0, MESSAGE); // �ļ���С 0 ����ʧ��
                sendInfoSegment(&client_socket, buf, sizeof(message), MESSAGE);
                sendTextSegment(&client_socket, message, sizeof(message));
                cout << "�ͻ��������ļ�: " << parameter << "\tʧ��." << endl;
            } else {
                int size = (int) getFileSize(&in_file);
                sendInfoSegment(&client_socket, buf, size, BINARY);
                char message[] = "�ɹ���Զ�������,׼�������ļ�...";
                sendInfoSegment(&client_socket, buf, sizeof(message), MESSAGE);
                sendTextSegment(&client_socket, message, sizeof(message));
                cout << "�ͻ��������ļ�: " << parameter << "\t�ļ���С: " << size << endl;
                int result = sendFileSegment(&client_socket, buf, &in_file, size, false);
                if (result == 0)
                    cerr << "�ͻ��������ļ�ʧ��, ��Ϊ�ͻ��˹ر�������, ����: 0" << endl;
                else if (result == SOCKET_ERROR)
                    cerr << "�ͻ��������ļ�ʧ��, ��Ϊ���ִ���, ����: " << WSAGetLastError() << endl;
                else
                    cout << "�ͻ��������ļ��ɹ�" << endl;
            }
        } else {
            char message[] = "δ֪����.";
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
            cout << "�ر�����" << endl;
            break;
        } else if (count == SOCKET_ERROR) {
            cout << "���� Client ���ݳ���: " << WSAGetLastError() << endl;
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
        cout << "���� socket ʧ��: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    // bind
    SOCKADDR_IN addr_server;
    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_server.sin_port = htons(port);
    if (bind(server, reinterpret_cast<const sockaddr *>(&addr_server), sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {
        cout << "bind ʧ��: " << WSAGetLastError();
        closesocket(server);
        WSACleanup();
        return 2;
    }

    // listen
    if (listen(server, 5) == SOCKET_ERROR) {
        cout << "listen ʧ��: " << WSAGetLastError();
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
            cerr << "�յ�һ����Ч�� Socket ����." << endl;
            continue;
        }

        HANDLE hThread;
        DWORD threadId;
        hThread = CreateThread(nullptr, 0, handleThread, &accept_socket, 0, &threadId);
        if (!hThread) {
            closesocket(accept_socket);
            cerr << "�����߳�ʧ��..." << endl;
            break;
        }
        CloseHandle(hThread);
    }
}
