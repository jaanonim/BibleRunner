cmake_minimum_required(VERSION 3.28)
project(biblerunner LANGUAGES CXX)

set(KF6_MIN_VERSION "5.90")

# Include the Extra-CMake-Modules project
find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS I18n Config Runner KIO Notifications)

# This takes care of building and installing the plugin
kcoreaddons_add_plugin(biblerunner
        SOURCES
        biblerunner.cpp biblerunner.h
        query.cppm 
        data/books.cppm
        INSTALL_NAMESPACE "kf6/krunner")
# Module interface units must be tracked in a CXX_MODULES file set
target_sources(biblerunner PRIVATE
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
        query.cppm data/books.cppm)
# The plugin TU imports the module, so dependency scanning is required
set_target_properties(biblerunner PROPERTIES CXX_SCAN_FOR_MODULES ON)
# We need to link the KRunner and other used libraries  to it
target_link_libraries(biblerunner
        KF6::I18n
        KF6::ConfigCore
        KF6::Runner
        KF6::KIOWidgets
        KF6::Notifications
)

set(KDE_INSTALL_ICONDIR "/usr/share/pixmaps/" CACHE STRING "Default icon directory")
install(FILES bible-icon.png DESTINATION ${KDE_INSTALL_ICONDIR})
