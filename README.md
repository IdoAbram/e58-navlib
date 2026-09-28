# e58-navlib

A small C++ library for talking to an E58-style Wi-Fi drone over UDP.

Everything in this document is split into two kinds of statements:

- **Observed**: seen directly in a Wireshark capture of the official phone app talking to the
  drone (19 s, 2586 packets, throttle-up pressed twice).
- **Unverified**: taken from other projects or guessed. Treat these as hypotheses until a capture
  confirms them.

## Network layout (observed)

| Role  | Address         | Notes                                    |
| ----- | --------------- | ---------------------------------------- |
| Drone | `192.168.1.1`   | Wi-Fi access point, also the gateway     |
| Phone | `192.168.1.100` | Gets its address from the drone via DHCP |

| Traffic                              | Protocol | Purpose                                         |
| ------------------------------------ | -------- | ----------------------------------------------- |
| phone -> drone `:7099`               | UDP      | **Flight control** (this library)               |
| drone `:7099` -> phone               | UDP      | 5-byte status packets, sent to the phone's port |
| phone -> drone `:7070`               | TCP      | RTSP session setup for video                    |
| drone (random port) -> phone (RTP)   | UDP      | Video, RTP payload type 26 (MJPEG)              |

The phone also generated unrelated traffic in the capture (TCP SYNs to ports 143, 443, 5900 and to
`8.8.8.8:853`, DNS queries). None of it is part of the drone protocol.

## Control packet (observed)

Every control packet from the phone is **9 bytes**:

```
offset  0     1     2      3        4         5      6      7          8
       0x03  0x66  roll  pitch  throttle   yaw   flags  checksum    0x99
```

| Offset | Field    | Meaning                                                                  |
| ------ | -------- | ------------------------------------------------------------------------ |
| 0      | prefix   | Always `0x03` in the capture. Purpose unknown (see hypotheses below).    |
| 1      | header   | Always `0x66`.                                                           |
| 2      | roll     | Left/right. `0x80` = centred.                                            |
| 3      | pitch    | Forward/backward. `0x80` = centred.                                      |
| 4      | throttle | `0x80` = centred, `> 0x80` = up, `< 0x80` = down.                        |
| 5      | yaw      | Turn left/right. `0x80` = centred.                                       |
| 6      | flags    | Command bitmask. Always `0x00` in the capture.                           |
| 7      | checksum | XOR of bytes 2..6 (roll, pitch, throttle, yaw, flags).                   |
| 8      | footer   | Always `0x99`.                                                           |

Notes on what was and was not confirmed:

- The **checksum rule** was verified on all 83 control packets in the capture.
- **Throttle** is confirmed: pressing "up" produced `0xF3`, `0xFF` and `0xE9` in offset 4, and
  nothing else moved.
- **Yaw** is very likely offset 5. It jittered by a few counts (`0x81` to `0x87`) while the
  throttle stick was pushed, which is what a thumb on the same on-screen stick would do.
- **Roll and pitch** stayed at `0x80` for the whole capture, so their order (roll before pitch)
  is **unverified**. It is assumed from other E58 documentation.
- Full deflection is `0xFF` on the top side. The bottom of the range (`0x00`) has not been seen.

### Real packets from the capture

| Meaning                 | Bytes (hex)                 |
| ----------------------- | --------------------------- |
| All sticks centred      | `03 66 80 80 80 80 00 00 99` |
| Throttle up, about 90 % | `03 66 80 80 f3 87 00 74 99` |
| Throttle up, 100 %      | `03 66 80 80 ff 81 00 7e 99` |
| Throttle up, about 80 % | `03 66 80 80 e9 85 00 6c 99` |

## Keepalive packet (observed, purpose partly guessed)

A 2-byte packet, `01 01`, is sent to the same port about **once per second** (at 6.6 s, 7.6 s,
8.7 s, 9.7 s, 10.6 s and 11.7 s). It is sent even while the sticks are centred. It is likely a
heartbeat or "app is alive" signal, but the capture cannot prove what the drone does with it.

## Status packet from the drone (observed)

The drone sends a 5-byte packet from port `7099` to the phone's source port about **every 52 ms**
(roughly 19 per second):

```
54 00 00 00 00
```

It never changed during the capture (232 packets to one port, 130 to an earlier one), so its fields
are unknown. Battery, altitude or flight state may live in there and simply stayed at zero.
The drone sent these even in the first 6.6 s, before any control packet from the phone was
captured, so it streams status regardless of what the phone sends.

## Timing (observed)

- The phone sends a control frame roughly every **130 ms** (about 8 Hz), not every 50 ms.
- Almost every frame is sent **twice** back to back (41 of the 42 frames in the capture), probably
  to survive Wi-Fi packet loss.
- The phone uses **one UDP socket** for the whole session, and the drone replies to that socket's
  source port. Opening a fresh socket for each packet changes the source port every time.
- The app opened a new socket (source port `57491` -> `57910`) at about 6.6 s. At about 12.2 s it
  restarted the video stream, and no control packets appear after 11.7 s.

## Video (observed, not implemented)

The video is a normal RTSP stream:

```
OPTIONS  rtsp://192.168.1.1:7070/webcam
DESCRIBE rtsp://192.168.1.1:7070/webcam
SETUP    rtsp://192.168.1.1:7070/webcam/track0   (RTP/AVP/UDP, unicast)
PLAY     rtsp://192.168.1.1:7070/webcam/
```

The app's user agent is `Lavf57.71.100`, so it is FFmpeg-based. Something like
`ffplay rtsp://192.168.1.1:7070/webcam` should open it.

## Unverified

These are carried over from earlier versions of this library and from public E58 reverse
engineering (<https://blog.horner.tj/hacking-chinese-drones-for-fun-and-no-profit/>,
<https://github.com/martin-ger/ESP_E58-Drone>). **None appeared in our capture**, because the
flags byte stayed `0x00`:

| Bit value | Assumed command |
| --------- | --------------- |
| `0x01`    | take off        |
| `0x02`    | land            |
| `0x04`    | emergency stop  |
| `0x08`    | roll / flip     |
| `0x10`    | headless mode   |
| `0x20`    | lock motors     |
| `0x40`    | unlock motors   |
| `0x80`    | calibrate gyro  |

Other open questions:

- The role of the `0x03` prefix. One guess is that it is a packet type (`0x03` = control,
  `0x01` = heartbeat), but no other type has been seen.
- The meaning of the drone's `54 00 00 00 00` status packet.
- Whether a command flag must be held for several frames (the real app repeats frames, so a single
  packet is probably not enough).
- Whether other E58 variants use the older 8-byte frame (no `0x03` prefix) on `192.168.0.1:50000`.
  That is what earlier versions of this library assumed. Our drone does not use it.

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests encode and decode the exact packets from the capture.

## Example

```bash
./build/e58_example                       # one centred packet to 192.168.1.1:7099
./build/e58_example 192.168.1.1 7099 up 2 # hold throttle up for 2 seconds
```

`up` sends frames at the app's cadence (every 130 ms, each twice) and a `01 01` keepalive once per
second, then finishes with a centred frame.

## C++ API

```cpp
#include "drone_controller.hpp"

int main() {
    e58::DroneController drone; // 192.168.1.1:7099, one socket for the whole session

    e58::CommandFrame frame = e58::CommandFrame::s_neutral();
    frame.m_throttle = 0xFF; // full throttle up

    drone.hold(frame, std::chrono::seconds(1)); // repeated at the app's cadence
    drone.send_neutral();
    return 0;
}
```

## Safety notes

- This is a low-level command sender. Remove the propellers, or keep the drone tethered, while
  testing.
- Stopping the stream is not a landing. Send centred frames when you are done.
- Flag values are unverified, so do not rely on them for anything safety-critical such as
  emergency stop.
