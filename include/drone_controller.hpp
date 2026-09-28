#ifndef DRONE_CONTROLLER_HPP
#define DRONE_CONTROLLER_HPP

#include "command_frame.hpp"

#include <chrono>
#include <cstdint>
#include <string>

namespace e58 {

// Owns one UDP socket for the whole session. The drone replies to the source port of that
// socket, and the official app also uses a single socket, so packets are not sent from a fresh
// socket each time.
class DroneController {
  public:
    explicit DroneController(std::string drone_ip = DefaultIp, uint16_t udp_port = DefaultPort,
                             int local_port = 0);
    ~DroneController();

    DroneController(const DroneController &) = delete;
    DroneController &operator=(const DroneController &) = delete;

    bool is_open() const noexcept;

    bool send_raw(const uint8_t *data, std::size_t size) const;
    bool send_packet(const Packet &packet) const;
    bool send_frame(const CommandFrame &frame) const;
    bool send_neutral() const;
    bool send_heartbeat() const;

    // Single-frame commands. The command flag values are unverified, and the official app repeats
    // frames, so use hold() if one packet is not enough.
    bool takeoff() const;
    bool land() const;
    bool emergency() const;
    bool calibrate() const;

    // Stick values are -128..127 around centre, clamped to the byte range.
    bool set_motion(int16_t roll, int16_t pitch, int16_t throttle, int16_t yaw,
                    Command command = Command::Null) const;

    // Repeats a frame the way the official app does: every FrameIntervalMs, each frame sent twice,
    // plus a heartbeat every HeartbeatIntervalMs. Blocks for the whole duration.
    bool hold(const CommandFrame &frame, std::chrono::milliseconds duration) const;

    // Waits for one 5-byte status packet. Returns false on timeout or if the size is wrong.
    bool receive_status(std::array<uint8_t, StatusSize> &status,
                        std::chrono::milliseconds timeout) const;

    const std::string &drone_ip() const noexcept;
    uint16_t port() const noexcept;
    int local_port() const noexcept;

  private:
    int m_socket = -1;
    std::string m_drone_ip;
    uint16_t m_udp_port;
    int m_local_port;
};

// One-shot helper. Opens and closes a socket, so the source port is different every call.
bool send_command(const std::string &drone_ip, const CommandFrame &frame,
                  uint16_t udp_port = DefaultPort, int local_port = 0);

} // namespace e58

#endif

