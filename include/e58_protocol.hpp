#ifndef E58_PROTOCOL_HPP
#define E58_PROTOCOL_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace e58 {

// Observed in a capture of the official app: drone is 192.168.1.1, control is UDP port 7099.
constexpr const char *DefaultIp = "192.168.1.1";
constexpr uint16_t DefaultPort = 7099u;
constexpr uint16_t RtspPort = 7070u; // Video only, not used by this library.

// Observed frame layout: prefix, header, roll, pitch, throttle, yaw, flags, checksum, footer.
constexpr uint8_t Prefix = 0x03u; // Purpose unknown, always 0x03 in the capture.
constexpr uint8_t Header = 0x66u;
constexpr uint8_t Footer = 0x99u;
constexpr uint8_t Neutral = 0x80u;
constexpr std::size_t FrameSize = 9u;

using Packet = std::array<uint8_t, FrameSize>;

// Observed keepalive, sent about once per second.
constexpr std::array<uint8_t, 2> Heartbeat = {0x01u, 0x01u};

// Observed pacing of the official app.
constexpr int FrameIntervalMs = 130; // about 8 Hz
constexpr int HeartbeatIntervalMs = 1000;

// The drone streams 5-byte status packets (always 54 00 00 00 00 in the capture).
constexpr std::size_t StatusSize = 5u;

// UNVERIFIED: values come from other E58 projects. The flags byte was always 0x00 in the capture.
enum class Command : uint8_t {
    Null = 0x00u,      // No command
    TakeOff = 0x01u,   // Take off
    Land = 0x02u,      // Land
    Emergency = 0x04u, // Emergency stop
    Roll = 0x08u,      // Perform a roll/flip
    Headless = 0x10u,  // Toggle headless mode
    Lock = 0x20u,      // Lock the motors
    Unlock = 0x40u,    // Unlock the motors
    Calibrate = 0x80u, // Calibrate the gyro
};

} // namespace e58

#endif

