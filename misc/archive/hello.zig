// --- T2-COPYRIGHT-NOTE-BEGIN ---
// This copyright note is auto-generated by ./scripts/Create-CopyPatch.
//
// T2 SDE: misc/archive/hello.zig
// Copyright (C) 2019 - 2024 The T2 SDE Project
//
// More information can be found in the files COPYING and README.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; version 2 of the License. A copy of the
// GNU General Public License can be found in the file COPYING.
// --- T2-COPYRIGHT-NOTE-END ---

const std = @import("std");

pub fn main() void {
    std.debug.print("Hello from {s}.\n", .{"Zig"});
}
