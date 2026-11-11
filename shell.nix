{pkgs ? import <nixpkgs> {}}: let
  biblerunner = pkgs.callPackage ./default.nix {};
in
  # Same toolchain as the package build (clang + libstdc++).
  (pkgs.mkShell.override {stdenv = pkgs.clangStdenv;}) {
    inputsFrom = [biblerunner];

    packages = with pkgs; [
      gcc
      clang
      gdb
      cmake
      ninja
      gnumake
      pkg-config

      # Boost.Test for the unit tests (see src/tests)
      boost.out
      boost.dev

      just
      just-lsp
    ];

    # clang-scan-deps needs the flags on the command line (see default.nix);
    # CC/CXX keep cmake from picking the g++ from PATH (cannot build the
    # module units)
    shellHook = ''
      export CXXFLAGS="${biblerunner.headerFlags}"
      export CC=clang
      export CXX=clang++
    '';
  }
