#pragma once

#include <QString>

#include <vector>

#include "core/interfaces/ioutputprovider.h"

namespace frappe
{

/// Which outputs should carry a dock. Mirrors ConfigFacade::DisplayMode.
enum class DisplayMode {
    FollowActive,
    AllScreens,
    SingleScreen,
};

/// Resolves the configured SingleScreen target to an output that actually
/// exists: the named one if connected, otherwise the primary, otherwise the
/// first output. Returns an empty string when nothing is connected.
QString resolveTargetOutput(const QString &target, const std::vector<OutputInfo> &outputs);

/// The output ids that should carry a dock, given the mode and the current
/// state of the world. This is the whole of the reconciliation decision — the
/// SurfaceManager only applies the difference between this and what it owns.
std::vector<QString> desiredSurfaces(DisplayMode mode,
                                     const std::vector<OutputInfo> &outputs,
                                     const QString &activeOutputId,
                                     const QString &targetOutput);

}
