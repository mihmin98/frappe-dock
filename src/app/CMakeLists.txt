# The wiring lives in a library rather than in main.cpp so it can be linked into
# a test. main() itself stays too thin to be worth testing.
add_library(frappe-app STATIC
    dockcontroller.cpp
)

target_link_libraries(frappe-app PUBLIC
    frappe-core
    Qt6::Core
)

set_target_properties(frappe-app PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(frappe-dock main.cpp)

target_link_libraries(frappe-dock PRIVATE
    frappe-app
    frappe-platform
    frappe-qml
    frappe-qmlplugin
    Qt6::Gui
    Qt6::Quick
)

install(TARGETS frappe-dock ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
