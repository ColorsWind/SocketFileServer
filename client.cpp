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
 * 接收到的第一个数据段包含消息的长度, 接下来是分段的消息
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
        cerr << "连接已终止: " << WSAGetLastError() << endl;
    else
        cerr << "接收服务端信息时发生错误: " << WSAGetLastError() << endl;
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
            cerr << "打开本地文件: " << parameter << " 失败, 无法上传." << endl;
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
            cout << "成功上传文件到服务端." << endl;
        else if (result == 0)
            cerr << "上传文件失败, 因为服务端已经关闭连接: 0" << endl;
        else
            cerr << "上传文件失败, 错误代码: " << WSAGetLastError() << endl;
    } else if (command == "download") {
        std::ofstream out_file;
        out_file.open(parameter, std::ios::out | std::ios::binary);
        if (out_file.fail()) {
            cerr << "打开本地文件: " << parameter << " 失败, 无法下载." << endl;
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
        cout << "创建 socket 失败: " << WSAGetLastError() << endl;
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
        cerr << "连接 Server 出错: " << WSAGetLastError() << endl;
        WSACleanup();
    }

    handleServerRespond(&client_socket);

    while (true) {
        handleClientRequest(&client_socket);
    }
}

