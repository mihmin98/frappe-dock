#include "app/dockcontroller.h"

#include "core/config/configfacade.h"
#include "core/model/tilemodel.h"
#include "platform/iconprovider.h"
#include "platform/launcherbackend.h"
#include "platform/outputprovider.h"
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
    const frappe::IconProvider icons;
    frappe::OutputProvider outputs;

    frappe::TaskBackend tasks;

    frappe::TileModel model(config, &launcher, &tasks);
    frappe::DockController controller(&model, &launcher, &tasks);

    frappe::QuickViewSurfaceFactory factory(config, &model, &icons);
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

    // Settings changes can alter both which surfaces exist and how thick they
    // are, so both have to be reapplied.
    QObject::connect(config, &frappe::ConfigFacade::changed, &app, [&] {
        model.rebuild();
        surfaces.reconcile();
        factory.updateGeometry();
    });

    surfaces.reconcile();

#ifdef FRAPPE_TUNING_HARNESS
    const auto harness = openTuningHarness();
#endif

    return app.exec();
}
