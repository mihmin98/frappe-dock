# Pure logic. Links Qt6::Core and KF6::ConfigCore and nothing else — in
# particular not Qt6::Gui or Qt6::Quick, which is what makes the layering rule
# from plan.md §2.1 enforce itself at compile time rather than by convention.
add_library(frappe-core STATIC)

target_sources(frappe-core PRIVATE
    config/configfacade.cpp
    desktopid.cpp
    geometry/layout.cpp
    geometry/hittest.cpp
    geometry/magnification.cpp
    geometry/stackplacement.cpp
    input/dispatch.cpp
    input/springloader.cpp
    model/contextmenu.cpp
    model/filedrop.cpp
    model/regiondrop.cpp
    model/stackmodel.cpp
    model/stacksettings.cpp
    model/tilemodel.cpp
    surfaceplan.cpp
)

kconfig_add_kcfg_files(frappe-core GENERATE_MOC config/frappeconfig.kcfgc)

target_include_directories(frappe-core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    # The generated frappeconfig.h lands next to the .kcfgc, in the binary dir.
    ${CMAKE_CURRENT_BINARY_DIR}/config
)

target_link_libraries(frappe-core PUBLIC
    Qt6::Core
    KF6::ConfigCore
)

set_target_properties(frappe-core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Without this, qmltyperegistrar cannot see ConfigFacade's or TileModel's
# meta-objects and silently registers them as empty types: QML then resolves
# FrappeConfig.tileSize to undefined and every binding on it reads NaN.
qt_extract_metatypes(frappe-core)
