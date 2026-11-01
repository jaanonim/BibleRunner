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

set(MODULES_FILES
    modules/query.cppm
    modules/config.cppm
    modules/books.cppm
)

# This takes care of building and installing the plugin
kcoreaddons_add_plugin(biblerunner
        SOURCES
        biblerunner.cpp biblerunner.h
        ${MODULES_FILES}
        INSTALL_NAMESPACE "kf6/krunner")
# Module interface units must be tracked in a CXX_MODULES file set
target_sources(biblerunner PRIVATE FILE_SET cxx_modules TYPE CXX_MODULES FILES ${MODULES_FILES})
# The plugin TU imports the module, so dependency scanning is required
set_target_properties(biblerunner PROPERTIES CXX_SCAN_FOR_MODULES ON)
# data/books.cppm embeds the JSON alias tables at compile time; clang-scan-deps
# does not report #embed files, so track them as explicit rebuild dependencies
set_source_files_properties(modules/books.cppm PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/data/books/de.json;"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/books/en.json;"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/books/nob.json;"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/books/pl.json;"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/books/pt-br.json")
# #embed is still a C23 extension in clang's C++ front end
target_compile_options(biblerunner PRIVATE
        "$<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-c23-extensions>")
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
