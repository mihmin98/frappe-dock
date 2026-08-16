#include "app/dockcontroller.h"

#include "core/config/configfacade.h"
#include "core/model/tilemodel.h"
#include "platform/iconprovider.h"
#include "platform/launcherbackend.h"
#include "platform/outputprovider.h"
#include "platform/quickviewsurfacefactory.h"
#include "platform/surfacemanager.h"

#include <QGuiApplication>

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

    frappe::TileModel model(config, &launcher);
    frappe::DockController controller(&model, &launcher);

    frappe::QuickViewSurfaceFactory factory(config, &model, &icons);
    QObject::connect(&factory, &frappe::QuickViewSurfaceFactory::launchRequested,
                     &controller, &frappe::DockController::activateTile);

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

    return app.exec();
}
