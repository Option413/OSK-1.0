const std = @import("std");

pub const Message = extern struct {
    source_task_id: u32,
    target_task_id: u32,
    length: u32,
    payload: [128]u8,
};

const max_messages = 64;

var mailbox: [max_messages]Message = undefined;
var used: [max_messages]bool = [_]bool{false} ** max_messages;
var initialized = false;

fn ensureInitialized() void {
    if (initialized) return;

    for (&mailbox) |*message| {
        message.* = std.mem.zeroes(Message);
    }
    for (&used) |*slot| {
        slot.* = false;
    }
    initialized = true;
}

pub export fn OskIpcInit() void {
    ensureInitialized();
}

pub export fn OskIpcSend(source_task_id: u32, target_task_id: u32, payload_ptr: [*]const u8, payload_len: usize) bool {
    ensureInitialized();
    if (payload_len == 0 or payload_len > 128) return false;

    for (used, 0..) |is_used, index| {
        if (is_used) continue;

        mailbox[index] = std.mem.zeroes(Message);
        mailbox[index].source_task_id = source_task_id;
        mailbox[index].target_task_id = target_task_id;
        mailbox[index].length = @as(u32, @intCast(payload_len));
        @memcpy(mailbox[index].payload[0..payload_len], payload_ptr[0..payload_len]);
        used[index] = true;
        return true;
    }
    return false;
}

pub export fn OskIpcReceive(target_task_id: u32, out_message: *Message) bool {
    ensureInitialized();

    for (used, 0..) |is_used, index| {
        if (!is_used) continue;
        if (mailbox[index].target_task_id != target_task_id) continue;

        out_message.* = mailbox[index];
        mailbox[index] = std.mem.zeroes(Message);
        used[index] = false;
        return true;
    }
    return false;
}
