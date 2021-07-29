#include <iostream>
#include <winsock2.h>
#include "common.h"

using std::cout;
using std::cerr;
using std::endl;
using std::flush;
using std::cin;
using std::string;


char buf[BUFFER_SIZE + 1];
char text[MAX_TEXT + 1];

/**
 * ���յ��ĵ�һ�����ݶΰ�����Ϣ�ĳ���, �������Ƿֶε���Ϣ
 * @param client_socket
 */
void handleServerRespond(SOCKET *client_socket) {
    TransportType type;
    int size;
    int result = receiveInfoSegment(client_socket, buf, &type, &size) > 0;
    if (result > 0) {
        if (type == MESSAGE)
            receiveTextSegment(client_socket, buf, size, true);
    } else if (result == 0)
        cerr << "��������ֹ: " << WSAGetLastError() << endl;
    else
        cerr << "���շ������Ϣʱ��������: " << WSAGetLastError() << endl;
}

void handleClientRequest(SOCKET *client_socket) {
    TransportType type;
    int size;
    string command, parameter;

    cout << ">" << flush;
    cin.getline(text, MAX_TEXT - 1);
    int len = (int) strlen(text);
    splitText(text, &command, &parameter);

    if (command == "upload") {
        std::ifstream in_file;
        in_file.open(parameter, std::ios::binary);
        if (in_file.fail()) {
            cerr << "�򿪱����ļ�: " << parameter << " ʧ��, �޷��ϴ�." << endl;
            return;
        }
        int file_size = (int) getFileSize(&in_file);
        // send upload request
        sendInfoSegment(client_socket, buf, len + 1, MESSAGE);
        sendTextSegment(client_socket, text, len + 1);
        sendInfoSegment(client_socket, buf, file_size, BINARY);
        // check respond
        receiveInfoSegment(client_socket, buf, &type, &size);
        if (size != file_size) {
            // server reject
            receiveInfoSegment(client_socket, buf, &type, &size);
            receiveTextSegment(client_socket, text, size, true);
            return;
        }
        // server accept, upload
        receiveInfoSegment(client_socket, buf, &type, &size);
        receiveTextSegment(client_socket, text, size, true);
        int result = sendFileSegment(client_socket, buf, &in_file, file_size, true);
        if (result > 0)
            cout << "�ɹ��ϴ��ļ��������." << endl;
        else if (result == 0)
            cerr << "�ϴ��ļ�ʧ��, ��Ϊ������Ѿ��ر�����: 0" << endl;
        else
            cerr << "�ϴ��ļ�ʧ��, �������: " << WSAGetLastError() << endl;
    } else if (command == "download") {
        std::ofstream out_file;
        out_file.open(parameter, std::ios::out | std::ios::binary);
        if (out_file.fail()) {
            cerr << "�򿪱����ļ�: " << parameter << " ʧ��, �޷�����." << endl;
            return;
        }
        // send download request
        sendInfoSegment(client_socket, buf, len + 1, MESSAGE);
        sendTextSegment(client_socket, text, len + 1);

        // check respond
        int file_size;
        receiveInfoSegment(client_socket, buf, &type, &file_size);
        receiveInfoSegment(client_socket, buf, &type, &size);
        receiveTextSegment(client_socket, text, size, true);
        if (file_size > 0)
            receiveFileSegment(client_socket, buf, &out_file, file_size, true);
        out_file.flush();
        out_file.close();
    } else if (command == "exit") {
        WSACleanup();
        exit(0);
    } else {
        sendInfoSegment(client_socket, buf, len + 1, MESSAGE);
        sendTextSegment(client_socket, text, len + 1);
        handleServerRespond(client_socket);
    }

}

int main() {
    // setup winsocket
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // create a socket
    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        cout << "���� socket ʧ��: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    // connect
    SOCKADDR_IN addr_server;
    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = inet_addr(ip);
    addr_server.sin_port = htons(port);

    if (connect(client_socket, reinterpret_cast<const sockaddr *>(&addr_server), sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {
        closesocket(client_socket);
        cerr << "���� Server ����: " << WSAGetLastError() << endl;
        WSACleanup();
    }

    handleServerRespond(&client_socket);

    while (true) {
        handleClientRequest(&client_socket);
    }
}

