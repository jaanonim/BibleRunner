{
  clangStdenv,
  gcc,
  cmake,
  ninja,
  lib,
  kdePackages,
  qt6,
}:
# GCC 16 hard-errors on Qt 6.12 headers used inside C++20 module units
# (P1815 "exposes TU-local entity" diagnostics that cannot be turned off),
# so the plugin is built with Clang. clang-scan-deps only honors flags from
# the command line (it skips the nix cc-wrapper's own flags), so the header
# roots are passed explicitly via CXXFLAGS, which CMake reads into
# CMAKE_CXX_FLAGS on first configure. libstdc++ is kept (instead of libc++)
# because Qt itself is built with GCC - std::string symbols must match.
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
  pname = "biblerunner";
  version = "1.0.0";

  env.CXXFLAGS = headerFlags;
  # Link-time flags are kept out of CXXFLAGS (which also applies to plain
  # compile jobs) and are consumed only by the cc-wrapper during linking.
  env.NIX_LDFLAGS = "-L${gcc.cc.lib}/lib -rpath ${gcc.cc.lib}/lib";

  passthru.headerFlags = headerFlags;

  src = ./src;

  nativeBuildInputs = [
    cmake
    ninja
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
    qt6.qtbase
  ];

  doCheck = false;

  # C++20 module target requires the Ninja generator
  cmakeFlags = ["-GNinja"];
}
