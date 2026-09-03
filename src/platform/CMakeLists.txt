# Everything that touches the outside world: the compositor, the desktop entry
# database, the icon theme.
add_library(frappe-platform STATIC)

target_sources(frappe-platform PRIVATE
    blurregion.cpp
    folderbackend.cpp
    iconpipeline.cpp
    iconprovider.cpp
    launcherbackend.cpp
    outputprovider.cpp
    palette.cpp
    quickviewsurfacefactory.cpp
    surfacemanager.cpp
    taskbackend.cpp
)

target_link_libraries(frappe-platform PUBLIC
    frappe-core
    Qt6::Gui
    Qt6::Quick
    KF6::Service
    KF6::KIOGui
    KF6::IconThemes
    KF6::WindowSystem
    # The colour scheme is an outside-world resource like the icon theme, which
    # is why the palette lives here rather than in core.
    KF6::ColorScheme
    LayerShellQt::Interface
    PW::LibTaskManager
)

set_target_properties(frappe-platform PROPERTIES POSITION_INDEPENDENT_CODE ON)

# DockPalette is registered with QML from qmlregistration.h. Without this,
# qmltyperegistrar cannot see its meta-object and registers an empty type, so
# every colour binding in QML silently reads undefined.
qt_extract_metatypes(frappe-platform)
