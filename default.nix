{
  clangStdenv,
  gcc,
  cmake,
  ninja,
  lib,
  kdePackages,
  qt6,
  boost,
}:
# clang, not gcc: gcc 16 rejects Qt headers inside C++20 module units, and
# clang-scan-deps only honors command-line flags (hence CXXFLAGS below).
# libstdc++ stays (Qt itself is built with gcc); gcc is on PATH for the C++26
# TU compiled by src/CMakeLists.txt (src/modules/books.cpp).
let
  gccVer = gcc.cc.version;
  headerFlags = lib.concatStringsSep " " [
    "-stdlib=libstdc++"
    "-isystem ${gcc.cc}/include/c++/${gccVer}"
    "-isystem ${gcc.cc}/include/c++/${gccVer}/x86_64-unknown-linux-gnu"
    "-isystem ${clangStdenv.cc.libc.dev}/include"
    "-isystem ${qt6.qtbase.out}/include"
    "-isystem ${qt6.qtbase.out}/mkspecs/linux-g++"
  ];
in
  clangStdenv.mkDerivation {
    name = "biblerunner";
    version = "1.0.0";

    env.CXXFLAGS = headerFlags;
    # link flags stay out of CXXFLAGS (which also hits compile jobs)
    env.NIX_LDFLAGS = "-L${gcc.cc.lib}/lib -rpath ${gcc.cc.lib}/lib";

    passthru.headerFlags = headerFlags;

    src = ./src;

    nativeBuildInputs = [
      cmake
      ninja
      gcc
      kdePackages.extra-cmake-modules
      qt6.wrapQtAppsHook
    ];

    buildInputs = [
      kdePackages.kcoreaddons
      kdePackages.ki18n
      kdePackages.kconfig
      kdePackages.krunner
      kdePackages.kio
      kdePackages.knotifications
      # kcm_biblerunner (KCModule / KPasswordLineEdit)
      kdePackages.kcmutils
      kdePackages.kwidgetsaddons
      qt6.qtbase
      # headers (dev) + shared lib (out) for the Boost.Test tests
      boost
    ];

    # run the unit tests (built only with BUILD_TESTING=ON, see
    # src/CMakeLists.txt); the cmake hook already sets CTEST_OUTPUT_ON_FAILURE=1
    doCheck = true;
    checkPhase = "ctest";

    cmakeFlags = ["-GNinja" "-DBUILD_TESTING=ON"]; # C++20 modules need Ninja
  }
