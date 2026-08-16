cmake_minimum_required(VERSION 3.28)
project(frappe-dock VERSION 0.1 LANGUAGES CXX)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(ECMAddTests)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)   # degrade to 20 on GCC 13
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)    # CMake 4 scans for C++20 modules at
                                       # CXX_STANDARD >= 20; conflicts with AUTOMOC

# 6.10 rather than 6.9: LibTaskManager's own find_dependency() requires it.
find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Qml Quick QuickTest Test)
find_package(KF6 6.28 REQUIRED COMPONENTS Service KIO IconThemes Config
                                          WindowSystem GlobalAccel ColorScheme)
find_package(LayerShellQt REQUIRED)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)

# The desktop entry is not a packaging nicety: libtaskmanager sees no windows at
# all without it, because KWin 6.7 gates org_kde_plasma_window_management on the
# installed entry's X-KDE-Wayland-Interfaces. Exec= must canonically resolve to
# the exact binary KWin sees, so it is generated against the install prefix.
# See docs/decisions/2026-08-16-libtaskmanager-api-surface.md.
set(FRAPPE_DOCK_INSTALLED_BINARY "${CMAKE_INSTALL_PREFIX}/${KDE_INSTALL_BINDIR}/frappe-dock")
configure_file(org.kde.frappe-dock.desktop.in
               ${CMAKE_CURRENT_BINARY_DIR}/org.kde.frappe-dock.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.frappe-dock.desktop
        DESTINATION ${KDE_INSTALL_APPDIR})
