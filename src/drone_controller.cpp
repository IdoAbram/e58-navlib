#include "drone_controller.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>

namespace e58 {

namespace {

int _s_open_udp_socket(int local_port) {
    const int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }

    if (local_port > 0) {
        sockaddr_in local_addr{};
        std::memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr.sin_port = htons(static_cast<uint16_t>(local_port));

        if (::bind(sock, reinterpret_cast<const sockaddr *>(&local_addr), sizeof(local_addr)) !=
            0) {
            ::close(sock);
            return -1;
        }
    }

    return sock;
}

// Connecting a UDP socket only fixes the peer address, so plain send() and recv() can be used and
// datagrams from other hosts are dropped.
bool _s_connect(int sock, const std::string &ip, uint16_t port) {
    sockaddr_in addr{};
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        return false;
    }
    return ::connect(sock, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == 0;
}

uint8_t _s_clamp_motion(int value) {
    if (value < 0) {
        value = 0;
    }
    if (value > 255) {
        value = 255;
    }
    return static_cast<uint8_t>(value);
}

} // namespace

DroneController::DroneController(std::string drone_ip, uint16_t udp_port, int local_port)
    : m_drone_ip(std::move(drone_ip)), m_udp_port(udp_port), m_local_port(local_port) {
    if (m_drone_ip.empty()) {
        return;
    }

    const int sock = _s_open_udp_socket(m_local_port);
    if (sock < 0) {
        return;
    }

    if (!_s_connect(sock, m_drone_ip, m_udp_port)) {
        ::close(sock);
        return;
    }

    m_socket = sock;
}

DroneController::~DroneController() {
    if (m_socket >= 0) {
        ::close(m_socket);
    }
}

bool DroneController::is_open() const noexcept {
    return m_socket >= 0;
}

bool DroneController::send_raw(const uint8_t *data, std::size_t size) const {
    if (m_socket < 0) {
        return false;
    }
    const ssize_t sent = ::send(m_socket, data, size, 0);
    return sent == static_cast<ssize_t>(size);
}

bool DroneController::send_packet(const Packet &packet) const {
    return send_raw(packet.data(), packet.size());
}

bool DroneController::send_frame(const CommandFrame &frame) const {
    return send_packet(frame.encode());
}

bool DroneController::send_neutral() const {
    return send_frame(CommandFrame::s_neutral());
}

bool DroneController::send_heartbeat() const {
    return send_raw(Heartbeat.data(), Heartbeat.size());
}

bool DroneController::takeoff() const {
    CommandFrame frame = CommandFrame::s_neutral();
    frame.m_command = Command::TakeOff;
    return send_frame(frame);
}

bool DroneController::land() const {
    CommandFrame frame = CommandFrame::s_neutral();
    frame.m_command = Command::Land;
    return send_frame(frame);
}

bool DroneController::emergency() const {
    CommandFrame frame = CommandFrame::s_neutral();
    frame.m_command = Command::Emergency;
    return send_frame(frame);
}

bool DroneController::calibrate() const {
    CommandFrame frame = CommandFrame::s_neutral();
    frame.m_command = Command::Calibrate;
    return send_frame(frame);
}

bool DroneController::set_motion(int16_t roll, int16_t pitch, int16_t throttle, int16_t yaw,
                                 Command command) const {
    CommandFrame frame;
    frame.m_roll = _s_clamp_motion(static_cast<int>(roll) + Neutral);
    frame.m_pitch = _s_clamp_motion(static_cast<int>(pitch) + Neutral);
    frame.m_throttle = _s_clamp_motion(static_cast<int>(throttle) + Neutral);
    frame.m_yaw = _s_clamp_motion(static_cast<int>(yaw) + Neutral);
    frame.m_command = command;
    return send_frame(frame);
}

bool DroneController::hold(const CommandFrame &frame, std::chrono::milliseconds duration) const {
    using Clock = std::chrono::steady_clock;

    const auto start = Clock::now();
    auto next_heartbeat = start;
    auto next_frame = start;
    bool ok = true;

    while (Clock::now() - start < duration) {
        const auto now = Clock::now();

        if (now >= next_heartbeat) {
            ok = send_heartbeat() && ok;
            next_heartbeat += std::chrono::milliseconds(HeartbeatIntervalMs);
        }

        if (now >= next_frame) {
            // The official app sends every frame twice back to back.
            ok = send_frame(frame) && ok;
            ok = send_frame(frame) && ok;
            next_frame += std::chrono::milliseconds(FrameIntervalMs);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return ok;
}

bool DroneController::receive_status(std::array<uint8_t, StatusSize> &status,
                                     std::chrono::milliseconds timeout) const {
    if (m_socket < 0) {
        return false;
    }

    pollfd fd{};
    fd.fd = m_socket;
    fd.events = POLLIN;
    if (::poll(&fd, 1, static_cast<int>(timeout.count())) <= 0) {
        return false;
    }

    std::array<uint8_t, 64> buffer{};
    const ssize_t received = ::recv(m_socket, buffer.data(), buffer.size(), 0);
    if (received != static_cast<ssize_t>(StatusSize)) {
        return false;
    }

    std::memcpy(status.data(), buffer.data(), StatusSize);
    return true;
}

const std::string &DroneController::drone_ip() const noexcept {
    return m_drone_ip;
}

uint16_t DroneController::port() const noexcept {
    return m_udp_port;
}

int DroneController::local_port() const noexcept {
    return m_local_port;
}

bool send_command(const std::string &drone_ip, const CommandFrame &frame, uint16_t udp_port,
                  int local_port) {
    const DroneController controller(drone_ip, udp_port, local_port);
    return controller.send_frame(frame);
}

} // namespace e58

