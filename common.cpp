//
// Created by colors_wind on 2021/6/19.
//

#include "common.h"

int receiveFileSegment(const SOCKET *receive_socket, char *buf, std::ofstream *out, int size, bool show) {
    int total = size;
    int result;
    while (size > 0) {
        int receive_size = minOf(BUFFER_SIZE, size);
        result = recv(*receive_socket, buf, receive_size, 0);
        if (result <= 0)
            return result;
        out -> write(buf, receive_size);
        size -= receive_size;
        if (show)
            progress(total, total - size, total - size - receive_size);
    }
    return result;
}

int sendFileSegment(const SOCKET *receive_socket, char *buf, std::ifstream *in, int size, bool show) {
    int total = size;
    int result;
    while (size > 0) {
        int send_size = minOf(BUFFER_SIZE, size);
        in->read(buf, send_size);
        result = send(*receive_socket, buf, send_size, 0);
        if (result <= 0)
            return result;
        size -= send_size;
        if (show)
            progress(total, total - size, total - size - send_size);
    }
    return result;
}

void progress(int total, int complete, int lastComplete) {
    int n = complete * 10 / total;
    int n_ = lastComplete * 10 / total;
    if (n == n_ && n != 10) return;
    cout << "[";
    for (int i = 0; i < n; i++) cout << "+";
    for (int i = n; i< 10; i++) cout << "-";
    cout << "] " << complete << "/" << total << " B" << endl;
}


int sendTextSegment(const SOCKET *send_socket, const char *text, int size) {
    while (size > 0) {
        int send_size = minOf(BUFFER_SIZE, size);
        send(*send_socket, text, send_size, 0);
        text += send_size; // move forward
        size -= send_size; // decrease rest to send
    }
    return 0;
}

int receiveTextSegment(const SOCKET *receive_socket, char *receiveText, int size, bool show) {
    if (size >= MAX_TEXT - 1)
        size = MAX_TEXT - 1;
    while (size > 0) {
        int receive_size = minOf(BUFFER_SIZE, size);
        recv(*receive_socket, receiveText, receive_size, 0);
        receiveText[receive_size] = '\0';
        if (show)
            cout << receiveText;
        size -= receive_size;
    }
    if (show)
        cout << endl;
    return 0;
}

int receiveInfoSegment(const SOCKET *receive_socket, char *buf, TransportType *type, int *size) {
    int result = recv(*receive_socket, buf, 2 * sizeof(u_long), 0);
    if (result <= 0) return result;
    *type = (TransportType) ntohl(*reinterpret_cast<u_long *>(buf));
    *size = (int) ntohl(*reinterpret_cast<u_long *>(buf + sizeof(u_long)));
    return result;
}

int sendInfoSegment(const SOCKET *send_socket, char *buf, int size, TransportType type) {
    *reinterpret_cast<u_long *>(buf) = htonl((u_long)type);
    *reinterpret_cast<u_long *>(buf + sizeof(u_long)) = htonl(size);
    return send(*send_socket, buf, 2 * sizeof(u_long), 0);
}



