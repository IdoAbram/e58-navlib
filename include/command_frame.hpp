#ifndef COMMAND_FRAME_HPP
#define COMMAND_FRAME_HPP

#include "e58_protocol.hpp"

namespace e58 {

struct CommandFrame {
    uint8_t m_roll = Neutral;     // Left/right
    uint8_t m_pitch = Neutral;    // Forward/backward
    uint8_t m_throttle = Neutral; // Above Neutral is up
    uint8_t m_yaw = Neutral;      // Turn left/right
    Command m_command = Command::Null;

    Packet encode() const;

    // XOR of roll, pitch, throttle, yaw and flags.
    uint8_t checksum() const;

    static CommandFrame s_neutral();

    // Returns false if the prefix, header, footer or checksum is wrong.
    static bool s_decode(const Packet &packet, CommandFrame &out);
};

} // namespace e58

#endif

