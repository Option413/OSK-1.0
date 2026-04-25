const std = @import("std");

pub const TaskState = enum(u8) {
    empty = 0,
    ready = 1,
    running = 2,
    blocked = 3,
    terminated = 4,
};

pub const Task = extern struct {
    id: u32,
    entry: usize,
    stack_top: usize,
    quantum_ticks: u32,
    state: TaskState,
};

const max_tasks = 32;

var tasks: [max_tasks]Task = undefined;
var initialized = false;
var current_index: usize = 0;
var next_id: u32 = 1;

fn ensureInitialized() void {
    if (initialized) return;

    for (&tasks) |*task| {
        task.* = Task{
            .id = 0,
            .entry = 0,
            .stack_top = 0,
            .quantum_ticks = 0,
            .state = .empty,
        };
    }
    initialized = true;
}

fn findFreeSlot() ?usize {
    for (tasks, 0..) |task, index| {
        if (task.state == .empty or task.state == .terminated) {
            return index;
        }
    }
    return null;
}

pub export fn OskSchedulerInit() void {
    ensureInitialized();
    current_index = 0;
}

pub export fn OskSchedulerCreateTask(entry: usize, stack_top: usize, quantum_ticks: u32) u32 {
    ensureInitialized();

    const slot = findFreeSlot() orelse return 0;
    const id = next_id;
    next_id += 1;

    tasks[slot] = Task{
        .id = id,
        .entry = entry,
        .stack_top = stack_top,
        .quantum_ticks = if (quantum_ticks == 0) 1 else quantum_ticks,
        .state = .ready,
    };
    return id;
}

pub export fn OskSchedulerBlockTask(task_id: u32) bool {
    ensureInitialized();

    for (&tasks) |*task| {
        if (task.id == task_id and task.state != .empty and task.state != .terminated) {
            task.state = .blocked;
            return true;
        }
    }
    return false;
}

pub export fn OskSchedulerWakeTask(task_id: u32) bool {
    ensureInitialized();

    for (&tasks) |*task| {
        if (task.id == task_id and task.state == .blocked) {
            task.state = .ready;
            return true;
        }
    }
    return false;
}

pub export fn OskSchedulerTerminateTask(task_id: u32) bool {
    ensureInitialized();

    for (&tasks) |*task| {
        if (task.id == task_id and task.state != .empty) {
            task.state = .terminated;
            return true;
        }
    }
    return false;
}

pub export fn OskSchedulerNextTask() ?*Task {
    ensureInitialized();

    var scanned: usize = 0;
    while (scanned < max_tasks) : (scanned += 1) {
        current_index = (current_index + 1) % max_tasks;
        if (tasks[current_index].state == .ready or tasks[current_index].state == .running) {
            tasks[current_index].state = .running;
            return &tasks[current_index];
        }
    }
    return null;
}

pub export fn OskSchedulerYieldCurrent() void {
    ensureInitialized();
    if (tasks[current_index].state == .running) {
        tasks[current_index].state = .ready;
    }
}

pub export fn OskSchedulerGetCurrentTaskId() u32 {
    ensureInitialized();
    return tasks[current_index].id;
}
