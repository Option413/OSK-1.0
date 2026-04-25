#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace osk::memory {
constexpr std::size_t kPageSize = 4096;
constexpr std::size_t kArenaSize = 1024 * 1024;
constexpr std::size_t kMaxBlocks = 256;

struct Block {
    std::size_t offset = 0;
    std::size_t size = 0;
    bool used = false;
};

struct Stats {
    std::size_t total_bytes = 0;
    std::size_t used_bytes = 0;
    std::size_t free_bytes = 0;
    std::size_t allocation_count = 0;
};

static alignas(16) std::array<std::uint8_t, kArenaSize> g_arena {};
static std::array<Block, kMaxBlocks> g_blocks {};
static Stats g_stats {};
static bool g_ready = false;

static std::size_t align_up(std::size_t value, std::size_t alignment) {
    const std::size_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

static void reset_block_table() {
    for (auto& block : g_blocks) {
        block = {};
    }

    g_blocks[0] = {
        .offset = 0,
        .size = kArenaSize,
        .used = false,
    };
}

static void refresh_stats() {
    g_stats.used_bytes = 0;
    g_stats.free_bytes = 0;
    g_stats.allocation_count = 0;

    for (const auto& block : g_blocks) {
        if (block.size == 0) {
            continue;
        }

        if (block.used) {
            g_stats.used_bytes += block.size;
            ++g_stats.allocation_count;
        } else {
            g_stats.free_bytes += block.size;
        }
    }
}

static bool split_block(std::size_t index, std::size_t requested_size) {
    auto& block = g_blocks[index];
    if (block.size <= requested_size) {
        return true;
    }

    std::size_t empty_slot = kMaxBlocks;
    for (std::size_t i = 0; i < g_blocks.size(); ++i) {
        if (g_blocks[i].size == 0) {
            empty_slot = i;
            break;
        }
    }

    if (empty_slot == kMaxBlocks) {
        return false;
    }

    g_blocks[empty_slot] = {
        .offset = block.offset + requested_size,
        .size = block.size - requested_size,
        .used = false,
    };
    block.size = requested_size;
    return true;
}

static void merge_free_blocks() {
    for (std::size_t i = 0; i < g_blocks.size(); ++i) {
        auto& left = g_blocks[i];
        if (left.size == 0 || left.used) {
            continue;
        }

        for (std::size_t j = 0; j < g_blocks.size(); ++j) {
            if (i == j) {
                continue;
            }

            auto& right = g_blocks[j];
            if (right.size == 0 || right.used) {
                continue;
            }

            if (left.offset + left.size == right.offset) {
                left.size += right.size;
                right = {};
                j = 0;
            }
        }
    }
}
}  // namespace osk::memory

extern "C" {
struct OskMemoryStats {
    std::size_t total_bytes;
    std::size_t used_bytes;
    std::size_t free_bytes;
    std::size_t allocation_count;
};

void OskMemoryInit() {
    using namespace osk::memory;
    std::memset(g_arena.data(), 0, g_arena.size());
    reset_block_table();
    g_stats.total_bytes = kArenaSize;
    g_ready = true;
    refresh_stats();
}

void* OskMemoryAllocate(std::size_t size, std::size_t alignment) {
    using namespace osk::memory;
    if (!g_ready) {
        OskMemoryInit();
    }

    if (size == 0) {
        return nullptr;
    }

    if (alignment == 0) {
        alignment = alignof(std::max_align_t);
    }

    size = align_up(size, alignment);

    for (std::size_t i = 0; i < g_blocks.size(); ++i) {
        auto& block = g_blocks[i];
        if (block.size == 0 || block.used) {
            continue;
        }

        const std::size_t aligned_offset = align_up(block.offset, alignment);
        const std::size_t padding = aligned_offset - block.offset;
        if (block.size < padding + size) {
            continue;
        }

        if (padding != 0) {
            if (!split_block(i, padding)) {
                return nullptr;
            }
            return OskMemoryAllocate(size, alignment);
        }

        if (!split_block(i, size)) {
            return nullptr;
        }

        block.used = true;
        refresh_stats();
        return g_arena.data() + block.offset;
    }

    return nullptr;
}

void OskMemoryFree(void* ptr) {
    using namespace osk::memory;
    if (ptr == nullptr) {
        return;
    }

    auto* raw = static_cast<std::uint8_t*>(ptr);
    if (raw < g_arena.data() || raw >= g_arena.data() + g_arena.size()) {
        return;
    }

    const std::size_t offset = static_cast<std::size_t>(raw - g_arena.data());
    for (auto& block : g_blocks) {
        if (block.size != 0 && block.offset == offset && block.used) {
            block.used = false;
            std::memset(g_arena.data() + block.offset, 0, block.size);
            merge_free_blocks();
            refresh_stats();
            return;
        }
    }
}

OskMemoryStats OskMemoryGetStats() {
    using namespace osk::memory;
    if (!g_ready) {
        OskMemoryInit();
    }

    refresh_stats();
    return {
        .total_bytes = g_stats.total_bytes,
        .used_bytes = g_stats.used_bytes,
        .free_bytes = g_stats.free_bytes,
        .allocation_count = g_stats.allocation_count,
    };
}
}
