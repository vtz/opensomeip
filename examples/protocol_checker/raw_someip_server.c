/*
 * Raw SOME/IP Echo Server
 * Simple UDP server that responds to SOME/IP REQUEST messages with RESPONSE
 * Used for protocol-level interoperability testing
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SOMEIP_HEADER_SIZE 16
#define MAX_BUFFER_SIZE 4096
#define DEFAULT_PORT 30509

/* SOME/IP Message Types */
#define SOMEIP_MSG_REQUEST      0x00
#define SOMEIP_MSG_RESPONSE     0x80
#define SOMEIP_MSG_ERROR        0x81

/* SOME/IP Return Codes */
#define SOMEIP_RC_OK            0x00

void dump_hex(const char* label, const uint8_t* data, size_t len) {
    printf("%s (%zu bytes): ", label, len);
    for (size_t i = 0; i < len && i < 64; i++) {
        printf("%02x ", data[i]);
    }
    if (len > 64) printf("...");
    printf("\n");
}

int main(int argc, char* argv[]) {
    /* Disable stdout buffering for Docker logs */
    setvbuf(stdout, NULL, _IONBF, 0);

    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    printf("=== Raw SOME/IP Echo Server ===\n");
    printf("Listening on UDP port %d\n\n", port);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }

    printf("Server ready. Waiting for SOME/IP messages...\n\n");

    uint8_t buffer[MAX_BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len;

    while (1) {
        client_len = sizeof(client_addr);
        ssize_t recv_len = recvfrom(sock, buffer, sizeof(buffer), 0,
                                     (struct sockaddr*)&client_addr, &client_len);

        if (recv_len < 0) {
            perror("recvfrom");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("Received from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
        dump_hex("  RX", buffer, recv_len);

        if (recv_len < SOMEIP_HEADER_SIZE) {
            printf("  ERROR: Message too short (need %d bytes, got %zd)\n\n",
                   SOMEIP_HEADER_SIZE, recv_len);
            continue;
        }

        /* Parse SOME/IP header */
        uint16_t service_id = (buffer[0] << 8) | buffer[1];
        uint16_t method_id = (buffer[2] << 8) | buffer[3];
        uint32_t length = (buffer[4] << 24) | (buffer[5] << 16) |
                          (buffer[6] << 8) | buffer[7];
        uint16_t client_id = (buffer[8] << 8) | buffer[9];
        uint16_t session_id = (buffer[10] << 8) | buffer[11];
        uint8_t protocol_ver = buffer[12];
        uint8_t interface_ver = buffer[13];
        uint8_t msg_type = buffer[14];
        uint8_t return_code = buffer[15];

        printf("  SOME/IP Header:\n");
        printf("    Service: 0x%04x, Method: 0x%04x\n", service_id, method_id);
        printf("    Length: %u, Client: 0x%04x, Session: 0x%04x\n",
               length, client_id, session_id);
        printf("    Protocol: %d, Interface: %d, Type: 0x%02x, RC: 0x%02x\n",
               protocol_ver, interface_ver, msg_type, return_code);

        /* Only respond to REQUEST messages */
        if (msg_type != SOMEIP_MSG_REQUEST) {
            printf("  INFO: Not a REQUEST (type=0x%02x), ignoring\n\n", msg_type);
            continue;
        }

        /* Build RESPONSE: copy message, change type to RESPONSE */
        uint8_t response[MAX_BUFFER_SIZE];
        memcpy(response, buffer, recv_len);
        response[14] = SOMEIP_MSG_RESPONSE;  /* Message type = RESPONSE */
        response[15] = SOMEIP_RC_OK;         /* Return code = OK */

        /* Echo payload back (already in buffer) */
        ssize_t sent = sendto(sock, response, recv_len, 0,
                               (struct sockaddr*)&client_addr, client_len);

        if (sent < 0) {
            perror("  sendto");
        } else {
            dump_hex("  TX", response, sent);
            printf("  Sent RESPONSE (%zd bytes)\n\n", sent);
        }
    }

    close(sock);
    return 0;
}
