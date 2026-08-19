# The harness is a development tool and is not part of a release build: it is
# left out of the module's QML_FILES entirely rather than merely hidden, so a
# shipped dock cannot be talked into opening it.
if(FRAPPE_TUNING_HARNESS)
    set(FRAPPE_TUNING_HARNESS_QML TuningHarness.qml)
else()
    set(FRAPPE_TUNING_HARNESS_QML "")
endif()

# The QML module is a library rather than being folded into the executable so
# that tests/qml can import the same module the dock runs.
qt_add_library(frappe-qml STATIC)

qt_add_qml_module(frappe-qml
    URI org.kde.frappedock
    VERSION 1.0
    # Without this the module lands at qrc:/org/kde/frappedock, which is on no
    # import path, and every import of it fails with "Dock is not a type".
    RESOURCE_PREFIX "/qt/qml"
    # Must end in the target path, or qmllint and the QML tooling cannot find it.
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/org/kde/frappedock
    QML_FILES
        Dock.qml
        Tile.qml
        Indicator.qml
        TileMenu.qml
        ${FRAPPE_TUNING_HARNESS_QML}
    SOURCES
        ../app/qmlregistration.h
        ../app/qmlregistration.cpp
)

# The generated registration file includes <qmlregistration.h> by bare name.
target_include_directories(frappe-qml PRIVATE ${CMAKE_SOURCE_DIR}/src/app)

target_link_libraries(frappe-qml PUBLIC
    # DockGeometry lives here and is registered from qmlregistration.h.
    frappe-app
    frappe-core
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

set_target_properties(frappe-qml PROPERTIES POSITION_INDEPENDENT_CODE ON)
