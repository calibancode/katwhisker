# SPDX-FileCopyrightText: 2026 calibancode
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(katwhisker VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)

# The one place the version is set; code gets it from version.h.
ecm_setup_version(${PROJECT_VERSION} VARIABLE_PREFIX KATWHISKER VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/version.h)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Multimedia Widgets DBus)
find_package(KF6 REQUIRED COMPONENTS CoreAddons DBusAddons I18n)
set(CMAKE_AUTOMOC ON)
qt_add_executable(katwhisker
    src/main.cpp src/mainwindow.h src/mainwindow.cpp src/aboutdialog.h src/aboutdialog.cpp src/actionlistwidget.h src/actionlistwidget.cpp
    src/playerbar.h src/playerbar.cpp src/stationdelegate.h src/stationdelegate.cpp
    src/songdelegate.h src/songdelegate.cpp src/stationicons.h src/stationicons.cpp
    src/radiobrowser.h src/radiobrowser.cpp src/mpris.h src/mpris.cpp
    src/streamproxy.h src/streamproxy.cpp)

# Built in as the window icon's fallback, for running without installing.
qt_add_resources(katwhisker "icon" PREFIX "/" FILES icons/io.github.calibancode.katwhisker.svg)

target_include_directories(katwhisker PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(katwhisker PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Multimedia Qt6::Widgets Qt6::DBus
    KF6::CoreAddons KF6::DBusAddons KF6::I18n
)

install(TARGETS katwhisker ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

# Translations: po/<lang>/katwhisker.po, updated with po/update.sh.
ki18n_install(po)
install(FILES io.github.calibancode.katwhisker.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES io.github.calibancode.katwhisker.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES icons/io.github.calibancode.katwhisker.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(widgetstest tests/widgetstest.cpp
        src/mainwindow.h src/mainwindow.cpp src/aboutdialog.h src/aboutdialog.cpp src/actionlistwidget.h src/actionlistwidget.cpp src/playerbar.h src/playerbar.cpp
        src/stationdelegate.h src/stationdelegate.cpp src/songdelegate.h src/songdelegate.cpp src/stationicons.h src/stationicons.cpp
        src/radiobrowser.h src/radiobrowser.cpp src/streamproxy.h src/streamproxy.cpp
        src/mpris.h src/mpris.cpp)
    qt_add_resources(widgetstest "testicon" PREFIX "/" FILES icons/io.github.calibancode.katwhisker.svg)
    target_include_directories(widgetstest PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(widgetstest PRIVATE Qt6::Test Qt6::Widgets Qt6::Network Qt6::Multimedia
        Qt6::DBus KF6::CoreAddons KF6::I18n)
    add_test(NAME widgets COMMAND widgetstest)
    set_tests_properties(widgets PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
