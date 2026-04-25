#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace osk::net {
constexpr std::size_t kMaxPackets = 32;
constexpr std::size_t kPacketSize = 1500;

struct Packet {
    bool used = false;
    std::size_t length = 0;
    std::array<std::uint8_t, kPacketSize> payload {};
};

struct InterfaceState {
    bool up = false;
    std::uint32_t ipv4 = 0;
    std::uint32_t gateway = 0;
    std::uint32_t netmask = 0;
};

static std::array<Packet, kMaxPackets> g_rx_queue {};
static std::array<Packet, kMaxPackets> g_tx_queue {};
static InterfaceState g_interface {};

static Packet* reserve_packet(std::array<Packet, kMaxPackets>& queue) {
    for (auto& packet : queue) {
        if (!packet.used) {
            packet.used = true;
            packet.length = 0;
            packet.payload.fill(0);
            return &packet;
        }
    }
    return nullptr;
}

static Packet* next_packet(std::array<Packet, kMaxPackets>& queue) {
    for (auto& packet : queue) {
        if (packet.used) {
            return &packet;
        }
    }
    return nullptr;
}
}  // namespace osk::net

extern "C" {
void OskNetInit() {
    osk::net::g_interface = {};
}

void OskNetConfigure(std::uint32_t ipv4, std::uint32_t gateway, std::uint32_t netmask) {
    osk::net::g_interface.up = true;
    osk::net::g_interface.ipv4 = ipv4;
    osk::net::g_interface.gateway = gateway;
    osk::net::g_interface.netmask = netmask;
}

bool OskNetSend(const void* data, std::size_t size) {
    using namespace osk::net;
    if (!g_interface.up || data == nullptr || size == 0 || size > kPacketSize) {
        return false;
    }

    auto* packet = reserve_packet(g_tx_queue);
    if (packet == nullptr) {
        return false;
    }

    std::memcpy(packet->payload.data(), data, size);
    packet->length = size;
    return true;
}

bool OskNetInjectRx(const void* data, std::size_t size) {
    using namespace osk::net;
    if (data == nullptr || size == 0 || size > kPacketSize) {
        return false;
    }

    auto* packet = reserve_packet(g_rx_queue);
    if (packet == nullptr) {
        return false;
    }

    std::memcpy(packet->payload.data(), data, size);
    packet->length = size;
    return true;
}

std::size_t OskNetReceive(void* out_buffer, std::size_t capacity) {
    using namespace osk::net;
    if (!g_interface.up || out_buffer == nullptr || capacity == 0) {
        return 0;
    }

    auto* packet = next_packet(g_rx_queue);
    if (packet == nullptr) {
        return 0;
    }

    const std::size_t bytes = packet->length < capacity ? packet->length : capacity;
    std::memcpy(out_buffer, packet->payload.data(), bytes);
    *packet = {};
    return bytes;
}

std::size_t OskNetDrainTx(void* out_buffer, std::size_t capacity) {
    using namespace osk::net;
    if (out_buffer == nullptr || capacity == 0) {
        return 0;
    }

    auto* packet = next_packet(g_tx_queue);
    if (packet == nullptr) {
        return 0;
    }

    const std::size_t bytes = packet->length < capacity ? packet->length : capacity;
    std::memcpy(out_buffer, packet->payload.data(), bytes);
    *packet = {};
    return bytes;
}
}
