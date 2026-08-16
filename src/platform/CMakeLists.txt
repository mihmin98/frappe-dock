# Everything that touches the outside world: the compositor, the desktop entry
# database, the icon theme.
add_library(frappe-platform STATIC)

target_sources(frappe-platform PRIVATE
    iconprovider.cpp
    launcherbackend.cpp
    outputprovider.cpp
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
    LayerShellQt::Interface
    PW::LibTaskManager
)

set_target_properties(frappe-platform PROPERTIES POSITION_INDEPENDENT_CODE ON)
