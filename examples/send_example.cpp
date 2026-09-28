#include "drone_controller.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void _s_usage(const char *program) {
    std::cerr << "Usage: " << program << " [ip] [port] [mode] [seconds]\n"
              << "Modes:\n"
              << "  neutral    send one centred frame (default)\n"
              << "  up         hold full throttle up for [seconds] (default 1)\n"
              << "  status     print one status packet from the drone\n"
              << "  takeoff | land | emergency | calibrate\n"
              << "             hold that command flag for [seconds] (default 1); flag values are\n"
              << "             unverified\n";
}

e58::CommandFrame _s_command_frame(e58::Command command) {
    e58::CommandFrame frame = e58::CommandFrame::s_neutral();
    frame.m_command = command;
    return frame;
}

} // namespace

int main(int argc, char **argv) {
    const std::string drone_ip = (argc > 1) ? argv[1] : e58::DefaultIp;
    const uint16_t port =
        (argc > 2) ? static_cast<uint16_t>(std::strtoul(argv[2], nullptr, 10)) : e58::DefaultPort;
    const std::string mode = (argc > 3) ? argv[3] : "neutral";
    const long seconds = (argc > 4) ? std::strtol(argv[4], nullptr, 10) : 1;

    e58::DroneController drone(drone_ip, port);
    if (!drone.is_open()) {
        std::cerr << "Could not open a socket to " << drone_ip << ':' << port << '\n';
        return 1;
    }

    const std::chrono::milliseconds duration(seconds * 1000);

    if (mode == "neutral") {
        std::cout << "Sending neutral frame to " << drone_ip << ':' << port << '\n';
        return drone.send_neutral() ? 0 : 1;
    }

    if (mode == "status") {
        // The drone answers the socket that talks to it, so send a heartbeat first.
        std::array<uint8_t, e58::StatusSize> status{};
        drone.send_heartbeat();
        if (!drone.receive_status(status, std::chrono::milliseconds(2000))) {
            std::cerr << "No status packet received.\n";
            return 1;
        }
        for (const uint8_t byte : status) {
            std::cout << std::hex << static_cast<int>(byte) << ' ';
        }
        std::cout << '\n';
        return 0;
    }

    e58::CommandFrame frame = e58::CommandFrame::s_neutral();
    if (mode == "up") {
        frame.m_throttle = 0xFFu;
    } else if (mode == "takeoff") {
        frame = _s_command_frame(e58::Command::TakeOff);
    } else if (mode == "land") {
        frame = _s_command_frame(e58::Command::Land);
    } else if (mode == "emergency") {
        frame = _s_command_frame(e58::Command::Emergency);
    } else if (mode == "calibrate") {
        frame = _s_command_frame(e58::Command::Calibrate);
    } else {
        _s_usage(argv[0]);
        return 1;
    }

    std::cout << "Holding '" << mode << "' for " << seconds << " s\n";
    const bool ok = drone.hold(frame, duration);

    // Always finish with a centred frame so the drone does not keep the last stick position.
    return (drone.send_neutral() && ok) ? 0 : 1;
}

