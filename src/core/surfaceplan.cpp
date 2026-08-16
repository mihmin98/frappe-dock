#include "core/surfaceplan.h"

#include <algorithm>

namespace frappe
{

QString resolveTargetOutput(const QString &target, const std::vector<OutputInfo> &outputs)
{
    if (outputs.empty()) {
        return {};
    }

    const auto named = std::ranges::find_if(outputs, [&target](const OutputInfo &o) {
        return o.id == target;
    });
    if (named != outputs.end()) {
        return named->id;
    }

    const auto primary = std::ranges::find_if(outputs, [](const OutputInfo &o) {
        return o.isPrimary;
    });
    if (primary != outputs.end()) {
        return primary->id;
    }

    return outputs.front().id;
}

std::vector<QString> desiredSurfaces(DisplayMode mode,
                                     const std::vector<OutputInfo> &outputs,
                                     const QString &activeOutputId,
                                     const QString &targetOutput)
{
    if (outputs.empty()) {
        return {};
    }

    switch (mode) {
    case DisplayMode::AllScreens: {
        std::vector<QString> ids;
        ids.reserve(outputs.size());
        for (const OutputInfo &output : outputs) {
            ids.push_back(output.id);
        }
        return ids;
    }
    case DisplayMode::SingleScreen:
        return {resolveTargetOutput(targetOutput, outputs)};
    case DisplayMode::FollowActive:
        // Exactly one, always: resolveTargetOutput's fallback chain doubles as
        // the guard against an active output id that no longer exists. Returning
        // more than one element here is the surface leak this mode invites.
        return {resolveTargetOutput(activeOutputId, outputs)};
    }

    return {};
}

}
