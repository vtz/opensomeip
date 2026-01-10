/*
 * Raw SOME/IP Test Client
 * Sends a SOME/IP REQUEST and waits for RESPONSE
 * Used for protocol-level interoperability testing
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <netdb.h>

#define SOMEIP_HEADER_SIZE 16
#define MAX_BUFFER_SIZE 4096
#define DEFAULT_PORT 30509
#define DEFAULT_HOST "host.docker.internal"
#define TIMEOUT_SEC 5

/* SOME/IP Message Types */
#define SOMEIP_MSG_REQUEST      0x00
#define SOMEIP_MSG_RESPONSE     0x80

void dump_hex(const char* label, const uint8_t* data, size_t len) {
    printf("%s (%zu bytes): ", label, len);
    for (size_t i = 0; i < len && i < 64; i++) {
        printf("%02x ", data[i]);
    }
    if (len > 64) printf("...");
    printf("\n");
}

int main(int argc, char* argv[]) {
    /* Disable stdout buffering */
    setvbuf(stdout, NULL, _IONBF, 0);

    const char* host = getenv("SERVER_HOST");
    if (!host) host = DEFAULT_HOST;

    const char* port_str = getenv("SERVER_PORT");
    int port = port_str ? atoi(port_str) : DEFAULT_PORT;

    /* Service/Method IDs - same as vsomeip_test_client */
    uint16_t service_id = 0x1234;
    uint16_t method_id = 0x0421;

    const char* svc_str = getenv("SERVICE_ID");
    if (svc_str) service_id = (uint16_t)strtol(svc_str, NULL, 0);

    const char* mtd_str = getenv("METHOD_ID");
    if (mtd_str) method_id = (uint16_t)strtol(mtd_str, NULL, 0);

    printf("=== Raw SOME/IP Test Client ===\n\n");
    printf("Target: %s:%d\n", host, port);
    printf("Service: 0x%04x, Method: 0x%04x\n\n", service_id, method_id);

    /* Create UDP socket */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    /* Set receive timeout */
    struct timeval tv = { .tv_sec = TIMEOUT_SEC, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* Resolve server address */
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &server_addr.sin_addr) != 1) {
        /* Try DNS resolution */
        struct hostent* he = gethostbyname(host);
        if (!he) {
            fprintf(stderr, "Cannot resolve host: %s\n", host);
            close(sock);
            return 1;
        }
        memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    /* Build SOME/IP REQUEST message */
    uint8_t request[SOMEIP_HEADER_SIZE];

    /* Message ID: Service ID (16-bit) + Method ID (16-bit) */
    request[0] = (service_id >> 8) & 0xFF;
    request[1] = service_id & 0xFF;
    request[2] = (method_id >> 8) & 0xFF;
    request[3] = method_id & 0xFF;

    /* Length: 8 (header remainder, no payload) */
    request[4] = 0x00;
    request[5] = 0x00;
    request[6] = 0x00;
    request[7] = 0x08;

    /* Request ID: Client ID (16-bit) + Session ID (16-bit) */
    request[8] = 0x00;   /* Client ID high */
    request[9] = 0x01;   /* Client ID low */
    request[10] = 0x00;  /* Session ID high */
    request[11] = 0x01;  /* Session ID low */

    /* Protocol version, interface version, message type, return code */
    request[12] = 0x01;  /* Protocol version */
    request[13] = 0x01;  /* Interface version */
    request[14] = SOMEIP_MSG_REQUEST;  /* Message type: REQUEST */
    request[15] = 0x00;  /* Return code: E_OK */

    printf("Sending REQUEST...\n");
    dump_hex("TX", request, sizeof(request));

    ssize_t sent = sendto(sock, request, sizeof(request), 0,
                          (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (sent < 0) {
        perror("sendto");
        close(sock);
        return 1;
    }
    printf("Sent %zd bytes\n\n", sent);

    /* Wait for response */
    printf("Waiting for RESPONSE (timeout: %ds)...\n", TIMEOUT_SEC);

    uint8_t response[MAX_BUFFER_SIZE];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    ssize_t recv_len = recvfrom(sock, response, sizeof(response), 0,
                                 (struct sockaddr*)&from_addr, &from_len);

    if (recv_len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("\n=== TIMEOUT ===\n");
            printf("No response received within %d seconds\n", TIMEOUT_SEC);
            close(sock);
            return 1;
        }
        perror("recvfrom");
        close(sock);
        return 1;
    }

    printf("\nReceived response!\n");
    dump_hex("RX", response, recv_len);

    if (recv_len >= SOMEIP_HEADER_SIZE) {
        uint8_t msg_type = response[14];
        uint8_t return_code = response[15];

        printf("\nMessage Type: 0x%02x (%s)\n", msg_type,
               msg_type == SOMEIP_MSG_RESPONSE ? "RESPONSE" :
               msg_type == 0x81 ? "ERROR" : "OTHER");
        printf("Return Code:  0x%02x (%s)\n", return_code,
               return_code == 0x00 ? "E_OK" : "ERROR");

        if (msg_type == SOMEIP_MSG_RESPONSE && return_code == 0x00) {
            printf("\n=== SUCCESS ===\n");
            close(sock);
            return 0;
        }
    }

    printf("\n=== UNEXPECTED RESPONSE ===\n");
    close(sock);
    return 1;
}
