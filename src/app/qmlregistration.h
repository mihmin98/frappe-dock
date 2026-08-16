#pragma once

#include <QQmlEngine>

#include "core/config/configfacade.h"
#include "core/model/tile.h"
#include "core/model/tilemodel.h"

/*
 * QML registration for the C++ types.
 *
 * These are foreign declarations rather than QML_ELEMENT macros on the classes
 * themselves, because QML_ELEMENT lives in Qt6::Qml and core links only
 * Qt6::Core. Keeping the registration here is what lets the layering rule in
 * plan.md §2.1 stay enforced by the linker.
 */

namespace frappe
{

/// TileKind, mirrored so QML can name the values. Static asserts below keep the
/// mirror honest.
namespace TileKindNamespace
{
Q_NAMESPACE
QML_NAMED_ELEMENT(TileKind)

enum Value {
    Application,
    Folder,
    MinimizedWindow,
    Trash,
    Separator,
};
Q_ENUM_NS(Value)

static_assert(static_cast<int>(TileKind::Application) == Application);
static_assert(static_cast<int>(TileKind::Separator) == Separator);
}

/// TileModel, so QML can name the type of an injected model.
struct TileModelForeign {
    Q_GADGET
    QML_FOREIGN(frappe::TileModel)
    QML_NAMED_ELEMENT(TileModel)
    QML_UNCREATABLE("TileModel is constructed by the application")
};

/// ConfigFacade as a type, for its enums: ConfigFacade.Bottom and friends.
struct ConfigFacadeForeign {
    Q_GADGET
    QML_FOREIGN(frappe::ConfigFacade)
    QML_NAMED_ELEMENT(ConfigFacade)
    QML_UNCREATABLE("Use the FrappeConfig singleton")
};

/// The process-wide settings, as the singleton QML binds dimensions to.
struct FrappeConfigSingleton {
    Q_GADGET
    QML_FOREIGN(frappe::ConfigFacade)
    QML_NAMED_ELEMENT(FrappeConfig)
    QML_SINGLETON

public:
    static ConfigFacade *create(QQmlEngine *, QJSEngine *)
    {
        ConfigFacade *config = ConfigFacade::instance();
        // The singleton outlives every engine, so the engine must not take it.
        QQmlEngine::setObjectOwnership(config, QQmlEngine::CppOwnership);
        return config;
    }
};

}
