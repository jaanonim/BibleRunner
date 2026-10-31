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

    # clang-scan-deps ignores the cc-wrapper's own include flags, so the
    # header roots must be on the command line, like in default.nix.
    shellHook = ''export CXXFLAGS="${biblerunner.headerFlags}"'';
  }
