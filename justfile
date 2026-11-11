set shell := ["bash", "-euo", "pipefail", "-c"]

# Build the KRunner plugin against the system nixpkgs
build:
    nix-build -E 'with import <nixpkgs> {}; callPackage ./default.nix {}' --option substitute false
    
# Build and (re)start KRunner with the plugin loaded
run: build
    -systemctl --user stop plasma-krunner.service
    -pkill -x krunner
    setsid env QT_PLUGIN_PATH="{{justfile_directory()}}/result/lib/qt-6/plugins:${QT_PLUGIN_PATH:-}" krunner --daemon --replace >/dev/null 2>&1 < /dev/null &
    @echo "KRunner restarted."

# Check that KRunner can discover the plugin
status:
    QT_PLUGIN_PATH="{{justfile_directory()}}/result/lib/qt-6/plugins:${QT_PLUGIN_PATH:-}" krunner --list | grep -i bible

# Build and run the unit tests (compiler: CC/CXX from the shell, see shell.nix)
test:
    cmake -B build -S src -G Ninja -DBUILD_TESTING=ON
    cmake --build build
    ctest --test-dir build --output-on-failure
