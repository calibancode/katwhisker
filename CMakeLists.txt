cmake_minimum_required(VERSION 3.25)
project(kradio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Multimedia Widgets DBus)
find_package(KF6 REQUIRED COMPONENTS CoreAddons I18n)
find_package(KF6Kirigami REQUIRED)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(kradio src/main.cpp)

qt_add_qml_module(kradio
    URI org.kde.kradio
    QML_FILES src/qml/Main.qml src/qml/StationDelegate.qml src/qml/PlayerBar.qml src/qml/StationIcon.qml src/qml/RecordingDelegate.qml src/qml/IconToolButton.qml
    SOURCES src/radiobrowser.h src/radiobrowser.cpp src/mpris.h src/mpris.cpp src/streamproxy.h src/streamproxy.cpp src/recordings.h src/recordings.cpp
)

target_include_directories(kradio PRIVATE src)

target_link_libraries(kradio PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Multimedia Qt6::Widgets Qt6::DBus
    KF6::CoreAddons KF6::I18n KF6::I18nQml
)

install(TARGETS kradio ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES org.kde.kradio.desktop DESTINATION ${KDE_INSTALL_APPDIR})
