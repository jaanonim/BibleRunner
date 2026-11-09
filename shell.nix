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

      just
      just-lsp
    ];

    # clang-scan-deps needs the flags on the command line (see default.nix)
    shellHook = ''export CXXFLAGS="${biblerunner.headerFlags}"'';
  }
