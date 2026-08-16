#pragma once

namespace frappe
{

/// The failure modes every fallible core operation reports through std::expected.
enum class Error {
    NotFound,
    InvalidDesktopEntry,
    LaunchFailed,
    PermissionDenied,
    IoFailed,
};

}
