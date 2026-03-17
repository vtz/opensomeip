#ifdef _WIN32
#include <winsock2.h>

namespace {
struct WinsockInit {
    WinsockInit() {
        WSADATA d;
        WSAStartup(MAKEWORD(2, 2), &d);
    }
    ~WinsockInit() { WSACleanup(); }
} winsock_init_;
}

#endif
