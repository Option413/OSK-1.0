const std = @import("std");

pub const Driver = extern struct {
    name: [32]u8,
    version_major: u16,
    version_minor: u16,
    initialized: bool,
};

const max_drivers = 32;

var drivers: [max_drivers]Driver = undefined;
var initialized = false;

fn ensureInitialized() void {
    if (initialized) return;

    for (&drivers) |*driver| {
        driver.* = std.mem.zeroes(Driver);
    }
    initialized = true;
}

fn writeName(dest: *[32]u8, src: [*:0]const u8) void {
    var index: usize = 0;
    dest.* = [_]u8{0} ** 32;
    while (index < dest.len - 1 and src[index] != 0) : (index += 1) {
        dest[index] = src[index];
    }
    dest[index] = 0;
}

fn nameMatches(driver_name: *const [32]u8, requested: [*:0]const u8) bool {
    var index: usize = 0;
    while (index < driver_name.len) : (index += 1) {
        const lhs = driver_name[index];
        const rhs = requested[index];
        if (lhs != rhs) return false;
        if (lhs == 0) return true;
    }
    return requested[index] == 0;
}

pub export fn OskDriverManagerInit() void {
    ensureInitialized();
}

pub export fn OskDriverRegister(name: [*:0]const u8, version_major: u16, version_minor: u16) bool {
    ensureInitialized();

    for (&drivers) |*driver| {
        if (!driver.initialized and driver.name[0] == 0) {
            writeName(&driver.name, name);
            driver.version_major = version_major;
            driver.version_minor = version_minor;
            driver.initialized = false;
            return true;
        }
    }
    return false;
}

pub export fn OskDriverStart(name: [*:0]const u8) bool {
    ensureInitialized();

    for (&drivers) |*driver| {
        if (nameMatches(&driver.name, name)) {
            driver.initialized = true;
            return true;
        }
    }
    return false;
}

pub export fn OskDriverCountActive() u32 {
    ensureInitialized();

    var count: u32 = 0;
    for (drivers) |driver| {
        if (driver.initialized) count += 1;
    }
    return count;
}
