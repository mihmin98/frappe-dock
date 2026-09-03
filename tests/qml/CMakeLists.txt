add_executable(tst_qml tst_qml.cpp dragsimulator.h stackfixture.h)

target_link_libraries(tst_qml PRIVATE
    Qt6::QuickTest
    Qt6::Qml
    frappe-core
    frappe-qml
    frappe-qmlplugin
)

# frappe-qml is a static QML module, so its plugin has to be imported explicitly
# for the module to be findable at runtime.
qt_import_qml_plugins(tst_qml)

# stackfixture.h reaches tests/fakes/ for the folder backend the QML tests drive.
target_include_directories(tst_qml PRIVATE ${CMAKE_SOURCE_DIR}/tests)

# QtQuickTest scans this directory for tst_*.qml at runtime.
target_compile_definitions(tst_qml PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_test(NAME tst_qml COMMAND tst_qml)
set_tests_properties(tst_qml PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
