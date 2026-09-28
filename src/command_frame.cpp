#include "command_frame.hpp"

namespace e58 {

uint8_t CommandFrame::checksum() const {
    return static_cast<uint8_t>(m_roll ^ m_pitch ^ m_throttle ^ m_yaw ^
                                static_cast<uint8_t>(m_command));
}

Packet CommandFrame::encode() const {
    Packet packet{};
    packet[0] = Prefix;
    packet[1] = Header;
    packet[2] = m_roll;
    packet[3] = m_pitch;
    packet[4] = m_throttle;
    packet[5] = m_yaw;
    packet[6] = static_cast<uint8_t>(m_command);
    packet[7] = checksum();
    packet[8] = Footer;
    return packet;
}

CommandFrame CommandFrame::s_neutral() {
    return CommandFrame{};
}

bool CommandFrame::s_decode(const Packet &packet, CommandFrame &out) {
    if (packet[0] != Prefix || packet[1] != Header || packet[8] != Footer) {
        return false;
    }

    CommandFrame frame;
    frame.m_roll = packet[2];
    frame.m_pitch = packet[3];
    frame.m_throttle = packet[4];
    frame.m_yaw = packet[5];
    frame.m_command = static_cast<Command>(packet[6]);

    if (frame.checksum() != packet[7]) {
        return false;
    }

    out = frame;
    return true;
}

} // namespace e58

