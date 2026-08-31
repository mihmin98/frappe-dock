#pragma once

#include <QQmlEngine>

#include "app/dockgeometry.h"
#include "app/tuning.h"
#include "core/config/configfacade.h"
#include "core/input/dispatch.h"
#include "core/input/springloader.h"
#include "core/model/contextmenu.h"
#include "core/model/filedrop.h"
#include "core/model/regiondrop.h"
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

/// input::Command, mirrored so QML can decide which interactions it handles
/// itself. Static asserts below keep the mirror honest.
namespace DockCommandNamespace
{
Q_NAMESPACE
QML_NAMED_ELEMENT(DockCommand)

enum Value {
    LaunchOrActivate,
    RevealInFileManager,
    ActivateAndHidePrevious,
    ActivateAndHideOthers,
    NewInstance,
    ShowContextMenu,
    ShowJumpList,
};
Q_ENUM_NS(Value)

static_assert(static_cast<int>(input::Command::LaunchOrActivate) == LaunchOrActivate);
static_assert(static_cast<int>(input::Command::ShowContextMenu) == ShowContextMenu);
static_assert(static_cast<int>(input::Command::ShowJumpList) == ShowJumpList);
}

/// MenuItemKind, mirrored so the menu delegate can switch on it.
namespace MenuItemKindNamespace
{
Q_NAMESPACE
QML_NAMED_ELEMENT(MenuItemKind)

enum Value {
    Separator,
    Window,
    Action,
    Pin,
    Unpin,
    LaunchAtLogin,
    ShowInFileManager,
    Quit,
    ForceQuit,
};
Q_ENUM_NS(Value)

static_assert(static_cast<int>(MenuItemKind::Separator) == Separator);
static_assert(static_cast<int>(MenuItemKind::ForceQuit) == ForceQuit);
}

/// DropRejection, mirrored so the tile can word the refusal. The reason is
/// decided in C++ and the sentence is written in QML, which is where
/// translation lives.
namespace DropRejectionNamespace
{
Q_NAMESPACE
QML_NAMED_ELEMENT(DropRejection)

enum Value {
    None,
    NoFiles,
    UnsupportedType,
};
Q_ENUM_NS(Value)

static_assert(static_cast<int>(DropRejection::None) == None);
static_assert(static_cast<int>(DropRejection::UnsupportedType) == UnsupportedType);
}

/// Region, mirrored so the view can name the area a payload belongs in.
namespace RegionNamespace
{
Q_NAMESPACE
QML_NAMED_ELEMENT(TileRegion)

enum Value {
    Head,
    Pinned,
    Recent,
    Files,
    Minimized,
    Tail,
};
Q_ENUM_NS(Value)

static_assert(static_cast<int>(Region::Head) == Head);
static_assert(static_cast<int>(Region::Tail) == Tail);
}

/// DropItemKind and RegionDropRejection, mirrored for the same reason
/// DropRejection is: the reason is decided in C++, the sentence written in QML.
namespace DropItemKindNamespace
{
Q_NAMESPACE
QML_NAMED_ELEMENT(DropItemKind)

enum Value {
    Unknown,
    Application,
    File,
    Folder,
};
Q_ENUM_NS(Value)

static_assert(static_cast<int>(DropItemKind::Unknown) == Unknown);
static_assert(static_cast<int>(DropItemKind::Folder) == Folder);
}

namespace RegionDropRejectionNamespace
{
Q_NAMESPACE
QML_NAMED_ELEMENT(RegionDropRejection)

enum Value {
    None,
    WrongRegion,
    UnknownItem,
    MixedPayload,
    NotInstalled,
};
Q_ENUM_NS(Value)

static_assert(static_cast<int>(RegionDropRejection::None) == None);
static_assert(static_cast<int>(RegionDropRejection::NotInstalled) == NotInstalled);
}

/// The geometry engine, instantiable from QML so the view has exactly one
/// source of tile positions and needs nothing injected to get it.
struct DockGeometryForeign {
    Q_GADGET
    QML_FOREIGN(frappe::DockGeometry)
    QML_NAMED_ELEMENT(DockGeometry)
};

/// The spring-load countdown, instantiable from QML: the view feeds it drag
/// events and acts on springLoaded(), and the timing rules stay in C++ where
/// they are testable headless.
struct SpringLoaderForeign {
    Q_GADGET
    QML_FOREIGN(frappe::SpringLoader)
    QML_NAMED_ELEMENT(SpringLoader)
};

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

/// The non-persisted geometry knob the tuning harness moves. Registered in
/// every build, not just harness builds, so Dock.qml is the same file either
/// way — a view that differs between build types is a view that only works in
/// one of them.
struct TuningSingleton {
    Q_GADGET
    QML_FOREIGN(frappe::Tuning)
    QML_NAMED_ELEMENT(GeometryTuning)
    QML_SINGLETON

public:
    static Tuning *create(QQmlEngine *, QJSEngine *)
    {
        Tuning *tuning = Tuning::instance();
        QQmlEngine::setObjectOwnership(tuning, QQmlEngine::CppOwnership);
        return tuning;
    }
};

}
