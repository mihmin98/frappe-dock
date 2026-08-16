#pragma once

#include <QString>

#include <vector>

#include "core/interfaces/ioutputprovider.h"
#include "core/surfaceplan.h"

namespace frappe
{

class ConfigFacade;

/// Creation and destruction of the actual dock surfaces.
///
/// Exists so SurfaceManager's reconciliation can be tested headless: the real
/// factory makes layer-shell QQuickViews, the test one counts calls.
class ISurfaceFactory
{
public:
    virtual ~ISurfaceFactory() = default;

    virtual void createSurface(const OutputInfo &output) = 0;
    virtual void destroySurface(const QString &outputId) = 0;
    /// Re-targets an existing surface at another output. Preferred over
    /// destroy-then-create in FollowActive: cheaper, and avoids a visible flash.
    virtual void moveSurface(const QString &fromOutputId, const OutputInfo &to) = 0;
    /// The output ids that currently have a surface.
    virtual std::vector<QString> surfaces() const = 0;
};

/// Keeps the set of dock surfaces in step with the display mode and the
/// connected outputs.
///
/// There is one entry point, reconcile(): it computes the surfaces that *should*
/// exist and applies the difference. Nothing mutates the surface set
/// incrementally, for the same reason TileModel does not track deltas — the
/// incremental version has states the reconciling version cannot reach.
class SurfaceManager
{
public:
    SurfaceManager(ConfigFacade *config, IOutputProvider *outputs, ISurfaceFactory *factory);

    /// Recomputes the desired surface set and creates, destroys or moves to match.
    void reconcile();

private:
    DisplayMode mode() const;

    ConfigFacade *m_config;
    IOutputProvider *m_outputs;
    ISurfaceFactory *m_factory;
};

}
