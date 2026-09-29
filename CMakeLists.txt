cmake_minimum_required(VERSION 3.25)
project(katwhisker VERSION 0.1.0 LANGUAGES CXX)

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

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Multimedia Widgets DBus)
find_package(KF6 REQUIRED COMPONENTS CoreAddons I18n)
find_package(KF6Kirigami REQUIRED)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(katwhisker src/main.cpp)

qt_add_qml_module(katwhisker
    URI io.github.calibancode.katwhisker
    QML_FILES src/qml/Main.qml src/qml/StationDelegate.qml src/qml/PlayerBar.qml src/qml/StationIcon.qml src/qml/RecordingDelegate.qml src/qml/IconToolButton.qml
    SOURCES src/radiobrowser.h src/radiobrowser.cpp src/mpris.h src/mpris.cpp src/streamproxy.h src/streamproxy.cpp src/recordings.h src/recordings.cpp
)

target_include_directories(katwhisker PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(katwhisker PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Multimedia Qt6::Widgets Qt6::DBus
    KF6::CoreAddons KF6::I18n KF6::I18nQml
)

install(TARGETS katwhisker ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES io.github.calibancode.katwhisker.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES io.github.calibancode.katwhisker.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
