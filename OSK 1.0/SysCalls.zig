const std = @import("std");

pub const SyscallNumber = enum(u32) {
    noop = 0,
    alloc = 1,
    free = 2,
    create_file = 3,
    write_file = 4,
    read_file = 5,
    net_send = 6,
    ipc_send = 7,
};

extern fn OskMemoryAllocate(size: usize, alignment: usize) ?*anyopaque;
extern fn OskMemoryFree(ptr: ?*anyopaque) void;
extern fn OskFsCreateFile(name: [*:0]const u8) bool;
extern fn OskFsWriteFile(name: [*:0]const u8, data: ?*const anyopaque, size: usize) usize;
extern fn OskFsReadFile(name: [*:0]const u8, out_buffer: ?*anyopaque, capacity: usize) usize;
extern fn OskNetSend(data: ?*const anyopaque, size: usize) bool;
extern fn OskIpcSend(source_task_id: u32, target_task_id: u32, payload_ptr: [*]const u8, payload_len: usize) bool;

pub export fn OskSyscallDispatch(number: u32, arg0: usize, arg1: usize, arg2: usize, arg3: usize) isize {
    const syscall = std.meta.intToEnum(SyscallNumber, number) catch return -1;

    return switch (syscall) {
        .noop => 0,
        .alloc => blk: {
            const ptr = OskMemoryAllocate(arg0, arg1);
            if (ptr) |resolved| {
                break :blk @as(isize, @intCast(@intFromPtr(resolved)));
            }
            break :blk -5;
        },
        .free => blk: {
            OskMemoryFree(@as(?*anyopaque, @ptrFromInt(arg0)));
            break :blk 0;
        },
        .create_file => if (OskFsCreateFile(@ptrFromInt(arg0))) 0 else -2,
        .write_file => blk: {
            const count = OskFsWriteFile(@ptrFromInt(arg0), @ptrFromInt(arg1), arg2);
            break :blk @as(isize, @intCast(count));
        },
        .read_file => blk: {
            const count = OskFsReadFile(@ptrFromInt(arg0), @ptrFromInt(arg1), arg2);
            break :blk @as(isize, @intCast(count));
        },
        .net_send => if (OskNetSend(@ptrFromInt(arg0), arg1)) 0 else -3,
        .ipc_send => if (OskIpcSend(@as(u32, @intCast(arg0)), @as(u32, @intCast(arg1)), @ptrFromInt(arg2), arg3)) 0 else -4,
    };
}
