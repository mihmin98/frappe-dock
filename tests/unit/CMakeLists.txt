# All unit tests run headless. Anything needing a compositor belongs in
# tests/manual/ instead.
set(FRAPPE_TEST_ENV QT_QPA_PLATFORM=offscreen)

function(frappe_add_unit_test name)
    cmake_parse_arguments(ARG "" "" "LINK" ${ARGN})

    add_executable(${name} ${name}.cpp)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_compile_definitions(${name} PRIVATE
        FRAPPE_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LINK})

    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "${FRAPPE_TEST_ENV}")
endfunction()

frappe_add_unit_test(test_scaffold LINK Qt6::Core)
frappe_add_unit_test(test_config LINK frappe-core)
frappe_add_unit_test(test_layout LINK frappe-core)
frappe_add_unit_test(test_magnification LINK frappe-core)
frappe_add_unit_test(test_hittest LINK frappe-core)
frappe_add_unit_test(test_tilemodel LINK frappe-core)
frappe_add_unit_test(test_tilemodel_merge LINK frappe-core)
frappe_add_unit_test(test_surfacemanager LINK frappe-core frappe-platform)
frappe_add_unit_test(test_launchermodel LINK frappe-core frappe-platform KF6::ConfigCore)
frappe_add_unit_test(test_minimized LINK frappe-core)
frappe_add_unit_test(test_contextmenu LINK frappe-core)
frappe_add_unit_test(test_dispatch LINK frappe-core)
frappe_add_unit_test(test_taskbackend_contract LINK frappe-core)
frappe_add_unit_test(test_dockcontroller LINK frappe-core frappe-app)
frappe_add_unit_test(test_dockgeometry LINK frappe-core frappe-app)
frappe_add_unit_test(test_commandrouting LINK frappe-core frappe-app)
