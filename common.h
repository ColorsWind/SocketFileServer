//
// Created by colors_wind on 2021/6/19.
//

#ifndef SOCKET_COMMON_H
#define SOCKET_COMMON_H

#include <winsock.h>
#include <iostream>
#include <fstream>
#include <string>

using std::cout;
using std::string;
using std::endl;

const unsigned short port = 20210;
const char ip[] = "127.0.0.1";

enum TransportType {
    MESSAGE, BINARY
};
const unsigned int BUFFER_SIZE = 30;
const unsigned int MAX_TEXT = 1 << 16;

inline int minOf(int x, int y) {
    if (x < y) return x;
    else return y;
}

inline long long getFileSize(std::ifstream *in) {
    in->seekg(0, std::ios_base::end);
    long long size = in->tellg();
    in->clear();
    in->seekg(0, std::ios::beg);
    return size;
}

inline void splitText(const char *str, string *command, string *parameter) {
    string raw_str(str);
    if (!raw_str.empty()) {
        raw_str.erase(0, raw_str.find_first_not_of(' '));
        raw_str.erase(raw_str.find_last_not_of(' ') + 1);
    }
    unsigned int space = raw_str.find_first_of(' ');
    if (space == string::npos) {
        *command = raw_str;
    } else {
        *command = raw_str.substr(0, space);
        if (space < raw_str.length())
            *parameter = raw_str.substr(space + 1);
    }
}

void progress(int total, int complete, int lastComplete);

int sendInfoSegment(const SOCKET *send_socket, char *buf, int size, TransportType type);

int sendTextSegment(const SOCKET *send_socket, const char *text, int size);

int receiveTextSegment(const SOCKET *receive_socket, char *receiveText, int size, bool show);

int receiveInfoSegment(const SOCKET *receive_socket, char *buf, TransportType *type, int *size);

int receiveFileSegment(const SOCKET *receive_socket, char *buf, std::ofstream *out, int size, bool show);

int sendFileSegment(const SOCKET *receive_socket, char *buf, std::ifstream *in, int size, bool show);


#endif //SOCKET_COMMON_H
