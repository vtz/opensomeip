/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <cstdio>
#include <cstring>
#include <cerrno>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static constexpr int ECHO_PORT = 4242;
static constexpr int BUF_SIZE = 256;

int main() {
    printf("=== SOME/IP Zephyr Network Test ===\n");
    printf("UDP echo server starting on port %d...\n", ECHO_PORT);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        printf("ERROR: socket() failed: %d\n", errno);
        return 1;
    }

    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(ECHO_PORT);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
        printf("ERROR: bind() failed: %d\n", errno);
        close(sock);
        return 1;
    }

    printf("Listening for UDP packets...\n");

    char buf[BUF_SIZE];
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    for (int i = 0; i < 5; ++i) {
        ssize_t received = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                                    reinterpret_cast<sockaddr*>(&client_addr),
                                    &client_len);
        if (received < 0) {
            printf("ERROR: recvfrom() failed: %d\n", errno);
            break;
        }

        buf[received] = '\0';
        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, sizeof(addr_str));
        printf("Received %zd bytes from %s:%d: %s\n",
               received, addr_str, ntohs(client_addr.sin_port), buf);

        ssize_t sent = sendto(sock, buf, received, 0,
                              reinterpret_cast<sockaddr*>(&client_addr),
                              client_len);
        if (sent < 0) {
            printf("ERROR: sendto() failed: %d\n", errno);
            break;
        }
        printf("Echoed %zd bytes back\n", sent);
    }

    close(sock);
    printf("=== Network test complete ===\n");
    return 0;
}
