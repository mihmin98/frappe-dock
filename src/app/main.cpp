#include "app/dockcontroller.h"

#include "core/config/configfacade.h"
#include "core/model/tilemodel.h"
#include "platform/iconpipeline.h"
#include "platform/iconprovider.h"
#include "platform/launcherbackend.h"
#include "platform/outputprovider.h"
#include "platform/palette.h"
#include "platform/quickviewsurfacefactory.h"
#include "platform/surfacemanager.h"
#include "platform/taskbackend.h"

#include <QGuiApplication>

#ifdef FRAPPE_TUNING_HARNESS
#include <QQmlApplicationEngine>
#include <QUrl>

#include <memory>
#endif

namespace
{

#ifdef FRAPPE_TUNING_HARNESS
/// Opens the tuning harness when FRAPPE_TUNING=1 is set.
///
/// It gets its own engine rather than sharing a dock surface's: the surfaces
/// come and go as outputs change, and a debug window that vanished with the
/// display it happened to be created on would be worse than no window. The
/// engine is returned so the caller can keep it alive; everything it mutates is
/// process-wide, so nothing else has to be handed to it.
std::unique_ptr<QQmlApplicationEngine> openTuningHarness()
{
    if (qEnvironmentVariableIntValue("FRAPPE_TUNING") != 1) {
        return nullptr;
    }

    auto engine = std::make_unique<QQmlApplicationEngine>();
    engine->load(QUrl(QStringLiteral("qrc:/qt/qml/org/kde/frappedock/TuningHarness.qml")));
    if (engine->rootObjects().isEmpty()) {
        qWarning("frappe-dock: the tuning harness failed to load");
        return nullptr;
    }
    return engine;
}
#endif

}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("frappe-dock"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("kde.org"));
    // The dock is not the last window standing; closing a surface during a
    // display-mode change must not take the process with it.
    QGuiApplication::setQuitOnLastWindowClosed(false);

    frappe::ConfigFacade *config = frappe::ConfigFacade::instance();

    const frappe::LauncherBackend launcher;

    // The views ask the pipeline, never the raw provider: the treatment an icon
    // gets is part of resolving it, not a filter someone might forget to apply.
    const frappe::IconProvider themeIcons;
    // The singleton, not a local: QML reads its cache token, and a second
    // pipeline would hand the views a token for treatment they are not seeing.
    frappe::IconPipeline *icons = frappe::IconPipeline::instance();
    icons->setSource(&themeIcons);

    frappe::DockPalette *palette = frappe::DockPalette::instance();
    const auto followPalette = [icons, palette] { icons->setAccent(palette->accent()); };
    followPalette();
    QObject::connect(palette, &frappe::DockPalette::changed, icons, followPalette);

    // The appearance mode applies live: the pipeline drops its cache and the
    // views re-request as they redraw. Nothing restarts.
    static_assert(int(frappe::IconPipeline::Mode::Default) == frappe::ConfigFacade::Default);
    static_assert(int(frappe::IconPipeline::Mode::Tinted) == frappe::ConfigFacade::Tinted);
    const auto followConfig = [icons, config] {
        icons->setMode(static_cast<frappe::IconPipeline::Mode>(config->appearanceMode()));
    };
    followConfig();
    QObject::connect(config, &frappe::ConfigFacade::changed, icons, followConfig);

    frappe::OutputProvider outputs;

    frappe::TaskBackend tasks;

    frappe::TileModel model(config, &launcher, &tasks);
    frappe::DockController controller(&model, &launcher, &tasks);

    frappe::QuickViewSurfaceFactory factory(config, &model, icons);
    factory.setController(&controller);
    QObject::connect(&factory, &frappe::QuickViewSurfaceFactory::tileClicked,
                     &controller, &frappe::DockController::tileClicked);
    QObject::connect(&factory, &frappe::QuickViewSurfaceFactory::tileHeld,
                     &controller, &frappe::DockController::tileHeld);

    // A window opened, closed, or changed state: re-query and diff. The backend
    // has one change channel and two consumers, so both are called from here
    // rather than each registering and quietly replacing the other.
    tasks.setChangeCallback([&model, &controller] {
        model.rebuild();
        controller.windowsChanged();
    });

    frappe::SurfaceManager surfaces(config, &outputs, &factory);

    // Outputs come and go, and in FollowActive the active one changes constantly.
    // Both are the same event as far as the surface set is concerned.
    outputs.setChangeCallback([&surfaces] {
        surfaces.reconcile();
    });

    // Three narrow connections, not one broad one. Reordering the dock is a
    // drag, and it changes the *contents* — driving the surface set and the
    // layer-shell geometry from the same signal reconfigured every surface
    // several times a gesture, to record a change in the pinned order.
    QObject::connect(config, &frappe::ConfigFacade::contentsChanged, &app, [&model] {
        model.rebuild();
    });
    QObject::connect(config, &frappe::ConfigFacade::surfaceSetChanged, &app, [&surfaces] {
        surfaces.reconcile();
    });
    QObject::connect(config, &frappe::ConfigFacade::surfaceGeometryChanged, &app, [&factory] {
        factory.updateGeometry();
    });

    surfaces.reconcile();

#ifdef FRAPPE_TUNING_HARNESS
    const auto harness = openTuningHarness();
#endif

    return app.exec();
}
