#include "command_frame.hpp"

#include <cstdlib>
#include <iostream>

namespace {

int g_failures = 0;

void _s_check(bool condition, const char *what) {
    if (!condition) {
        std::cerr << "FAIL: " << what << '\n';
        ++g_failures;
    }
}

// Packets copied byte for byte from the Wireshark capture.
void _s_check_captured(const e58::Packet &captured, uint8_t throttle, uint8_t yaw,
                       const char *what) {
    e58::CommandFrame frame;
    frame.m_throttle = throttle;
    frame.m_yaw = yaw;
    _s_check(frame.encode() == captured, what);

    e58::CommandFrame decoded;
    _s_check(e58::CommandFrame::s_decode(captured, decoded), what);
    _s_check(decoded.m_throttle == throttle && decoded.m_yaw == yaw, what);
}

} // namespace

int main() {
    _s_check_captured({0x03, 0x66, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x99}, 0x80, 0x80,
                      "centred sticks");
    _s_check_captured({0x03, 0x66, 0x80, 0x80, 0xf3, 0x87, 0x00, 0x74, 0x99}, 0xf3, 0x87,
                      "throttle f3 yaw 87");
    _s_check_captured({0x03, 0x66, 0x80, 0x80, 0xff, 0x82, 0x00, 0x7d, 0x99}, 0xff, 0x82,
                      "throttle ff yaw 82");
    _s_check_captured({0x03, 0x66, 0x80, 0x80, 0xff, 0x81, 0x00, 0x7e, 0x99}, 0xff, 0x81,
                      "throttle ff yaw 81");
    _s_check_captured({0x03, 0x66, 0x80, 0x80, 0xe9, 0x85, 0x00, 0x6c, 0x99}, 0xe9, 0x85,
                      "throttle e9 yaw 85");

    e58::Packet bad_checksum = {0x03, 0x66, 0x80, 0x80, 0xff, 0x81, 0x00, 0x00, 0x99};
    e58::CommandFrame out;
    _s_check(!e58::CommandFrame::s_decode(bad_checksum, out), "bad checksum is rejected");

    e58::Packet bad_prefix = {0x00, 0x66, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x99};
    _s_check(!e58::CommandFrame::s_decode(bad_prefix, out), "bad prefix is rejected");

    _s_check(e58::Heartbeat[0] == 0x01 && e58::Heartbeat[1] == 0x01, "heartbeat bytes");
    _s_check(e58::DefaultPort == 7099, "default port");

    if (g_failures == 0) {
        std::cout << "All tests passed\n";
    }
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

