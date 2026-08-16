#include "app/dockcontroller.h"

#include "core/interfaces/ilauncherbackend.h"
#include "core/model/tilemodel.h"

#include <QLoggingCategory>

using namespace frappe;

Q_LOGGING_CATEGORY(FRAPPE_DOCK, "frappe.dock")

DockController::DockController(TileModel *model, const ILauncherBackend *launcher, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_launcher(launcher)
{
}

void DockController::activateTile(const QString &tileId)
{
    if (tileId.isEmpty()) {
        return;
    }

    const auto result = m_launcher->launch(tileId);
    if (!result) {
        qCWarning(FRAPPE_DOCK) << "Could not launch" << tileId << "error" << static_cast<int>(result.error());
    }
}
