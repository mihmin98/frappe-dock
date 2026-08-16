#include "platform/surfacemanager.h"

#include "core/config/configfacade.h"

#include <algorithm>

using namespace frappe;

namespace
{
bool contains(const std::vector<QString> &haystack, const QString &needle)
{
    return std::ranges::find(haystack, needle) != haystack.end();
}

const OutputInfo *findOutput(const std::vector<OutputInfo> &outputs, const QString &id)
{
    const auto it = std::ranges::find_if(outputs, [&id](const OutputInfo &o) {
        return o.id == id;
    });
    return it == outputs.end() ? nullptr : &*it;
}
}

SurfaceManager::SurfaceManager(ConfigFacade *config, IOutputProvider *outputs, ISurfaceFactory *factory)
    : m_config(config)
    , m_outputs(outputs)
    , m_factory(factory)
{
}

DisplayMode SurfaceManager::mode() const
{
    switch (m_config->displayMode()) {
    case ConfigFacade::FollowActive:
        return DisplayMode::FollowActive;
    case ConfigFacade::SingleScreen:
        return DisplayMode::SingleScreen;
    case ConfigFacade::AllScreens:
    default:
        return DisplayMode::AllScreens;
    }
}

void SurfaceManager::reconcile()
{
    const std::vector<OutputInfo> outputs = m_outputs->outputs();
    const std::vector<QString> desired =
        desiredSurfaces(mode(), outputs, m_outputs->activeOutput().id, m_config->targetOutput());
    const std::vector<QString> current = m_factory->surfaces();

    std::vector<QString> obsolete;
    for (const QString &id : current) {
        if (!contains(desired, id)) {
            obsolete.push_back(id);
        }
    }

    std::vector<QString> missing;
    for (const QString &id : desired) {
        if (!contains(current, id)) {
            missing.push_back(id);
        }
    }

    // The one-out, one-in case is a surface changing output, which is what
    // FollowActive does constantly. Moving keeps the scene graph and avoids the
    // flash a destroy/create pair produces.
    if (obsolete.size() == 1 && missing.size() == 1) {
        if (const OutputInfo *target = findOutput(outputs, missing.front())) {
            m_factory->moveSurface(obsolete.front(), *target);
            return;
        }
    }

    for (const QString &id : obsolete) {
        m_factory->destroySurface(id);
    }
    for (const QString &id : missing) {
        if (const OutputInfo *output = findOutput(outputs, id)) {
            m_factory->createSurface(*output);
        }
    }
}
