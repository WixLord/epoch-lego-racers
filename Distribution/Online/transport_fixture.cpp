// Headless integration fixture, never shipped in the mod.
#include <winsock2.h>
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
struct Packet {
    unsigned magic, version, kind, session, sequence, acknowledged;
    unsigned char build[557]; char padding[3];
    float position[3], direction[3], up[3], speed;
};
static_assert(sizeof(Packet) == 624);
int main() {
    unsigned slot = std::atoi(std::getenv("EPOCH_PLAYER_SLOT"));
    unsigned count = std::atoi(std::getenv("EPOCH_PLAYER_COUNT"));
    std::ofstream log("fixture-" + std::to_string(slot) + ".log");
    WSADATA data; WSAStartup(MAKEWORD(2,2), &data);
    SOCKET socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in bindAddress{}; bindAddress.sin_family = AF_INET; bindAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(socket, (sockaddr*)&bindAddress, sizeof(bindAddress));
    u_long nonblocking = 1; ioctlsocket(socket, FIONBIO, &nonblocking);
    sockaddr_in bridge = bindAddress; bridge.sin_port = htons((unsigned short)std::atoi(std::getenv("EPOCH_BRIDGE_PORT")));
    const char* cookie = std::getenv("EPOCH_BRIDGE_TOKEN");
    unsigned mask = 1u << slot, ack = mask, ready = mask, sequence = 0;
    std::array<unsigned, 6> poses{};
    auto began = GetTickCount(); unsigned last = began - 100;
    bool loaded = false; unsigned finishedAt = 0;
    while (GetTickCount() - began < 60000) {
        if (GetTickCount() - last >= 100) {
            last = GetTickCount();
            for (unsigned kind = 1; kind <= (loaded ? 3u : 1u); ++kind) {
                Packet packet{}; packet.magic = 0x45504f43; packet.version = 2; packet.kind = kind;
                packet.session = slot + 1; packet.sequence = ++sequence; packet.acknowledged = mask;
                memset(packet.build, 40 + slot, sizeof(packet.build));
                packet.position[0] = (float)(100 + slot); packet.direction[0] = 1; packet.up[2] = 1; packet.speed = 12;
                char envelope[656]; memcpy(envelope, cookie, 32); memcpy(envelope + 32, &packet, 624);
                sendto(socket, envelope, sizeof(envelope), 0, (sockaddr*)&bridge, sizeof(bridge));
            }
        }
        char envelope[657]; sockaddr_in sender{}; int size = sizeof(sender);
        int length = recvfrom(socket, envelope, sizeof(envelope), 0, (sockaddr*)&sender, &size);
        if (length == 656 && !memcmp(envelope, cookie, 32)) {
            Packet packet; memcpy(&packet, envelope + 32, 624);
            if (packet.session == 0 || packet.session > count || packet.session == slot + 1) { log << "FAIL sender mapping"; return 1; }
            unsigned remote = packet.session - 1;
            if (packet.kind == 1) {
                for (auto byte : packet.build) if (byte != 40 + remote) { log << "FAIL build payload"; return 1; }
                mask |= 1u << remote;
                if (packet.acknowledged & (1u << slot)) ack |= 1u << remote;
            } else if (packet.kind == 2) {
                if (packet.position[0] != 100 + remote || packet.direction[0] != 1 || packet.up[2] != 1 || packet.speed != 12) { log << "FAIL compressed pose"; return 1; }
                ++poses[remote];
            } else if (packet.kind == 3) ready |= 1u << remote;
        }
        loaded = mask == (1u << count)-1 && ack == mask;
        bool complete = loaded && ready == mask;
        for (unsigned i = 0; i < count; ++i) if (i != slot && poses[i] < 30) complete = false;
        if (complete && !finishedAt) finishedAt = GetTickCount();
        if (finishedAt && GetTickCount() - finishedAt > 1000) { log << "PASS builds=" << count-1 << " poses=" << count-1 << " barrier=ready" << std::endl; closesocket(socket); WSACleanup(); return 0; }
        Sleep(2);
    }
    log << "FAIL timeout mask=" << mask << " ack=" << ack << " ready=" << ready << std::endl;
    return 1;
}
