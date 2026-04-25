#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace osk::fs {
constexpr std::size_t kMaxFiles = 64;
constexpr std::size_t kMaxNameLength = 48;
constexpr std::size_t kMaxFileSize = 8192;

struct FileEntry {
    bool used = false;
    char name[kMaxNameLength] {};
    std::size_t size = 0;
    std::array<std::uint8_t, kMaxFileSize> data {};
};

static std::array<FileEntry, kMaxFiles> g_files {};
static bool g_mounted = false;

static FileEntry* find_file(const char* name) {
    for (auto& file : g_files) {
        if (file.used && std::strncmp(file.name, name, kMaxNameLength) == 0) {
            return &file;
        }
    }
    return nullptr;
}

static FileEntry* allocate_file(const char* name) {
    for (auto& file : g_files) {
        if (!file.used) {
            file.used = true;
            std::strncpy(file.name, name, kMaxNameLength - 1);
            file.name[kMaxNameLength - 1] = '\0';
            file.size = 0;
            file.data.fill(0);
            return &file;
        }
    }
    return nullptr;
}
}  // namespace osk::fs

extern "C" {
void OskFsMount() {
    osk::fs::g_mounted = true;
}

bool OskFsCreateFile(const char* name) {
    using namespace osk::fs;
    if (!g_mounted || name == nullptr || *name == '\0') {
        return false;
    }

    if (find_file(name) != nullptr) {
        return false;
    }

    return allocate_file(name) != nullptr;
}

bool OskFsDeleteFile(const char* name) {
    using namespace osk::fs;
    if (!g_mounted || name == nullptr) {
        return false;
    }

    auto* file = find_file(name);
    if (file == nullptr) {
        return false;
    }

    *file = {};
    return true;
}

std::size_t OskFsWriteFile(const char* name, const void* data, std::size_t size) {
    using namespace osk::fs;
    if (!g_mounted || name == nullptr || data == nullptr) {
        return 0;
    }

    auto* file = find_file(name);
    if (file == nullptr) {
        file = allocate_file(name);
    }
    if (file == nullptr) {
        return 0;
    }

    const std::size_t bytes_to_write = size > kMaxFileSize ? kMaxFileSize : size;
    std::memcpy(file->data.data(), data, bytes_to_write);
    file->size = bytes_to_write;
    return bytes_to_write;
}

std::size_t OskFsReadFile(const char* name, void* out_buffer, std::size_t capacity) {
    using namespace osk::fs;
    if (!g_mounted || name == nullptr || out_buffer == nullptr) {
        return 0;
    }

    auto* file = find_file(name);
    if (file == nullptr) {
        return 0;
    }

    const std::size_t bytes_to_read = file->size < capacity ? file->size : capacity;
    std::memcpy(out_buffer, file->data.data(), bytes_to_read);
    return bytes_to_read;
}

std::size_t OskFsListFiles(char* out_buffer, std::size_t capacity) {
    using namespace osk::fs;
    if (!g_mounted || out_buffer == nullptr || capacity == 0) {
        return 0;
    }

    std::size_t offset = 0;
    for (const auto& file : g_files) {
        if (!file.used) {
            continue;
        }

        const std::size_t length = std::strnlen(file.name, kMaxNameLength);
        if (offset + length + 1 >= capacity) {
            break;
        }

        std::memcpy(out_buffer + offset, file.name, length);
        offset += length;
        out_buffer[offset++] = '\n';
    }

    if (offset < capacity) {
        out_buffer[offset] = '\0';
    } else {
        out_buffer[capacity - 1] = '\0';
    }
    return offset;
}
}
