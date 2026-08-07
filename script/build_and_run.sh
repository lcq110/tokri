#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-run}"
APP_NAME="Tokri"
BUNDLE_ID="net.surajyadav.Tokri"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/Release"
BUILD_APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
RUNTIME_DIR="$(getconf DARWIN_USER_TEMP_DIR)tokri-codex"
APP_BUNDLE="$RUNTIME_DIR/$APP_NAME.app"
QT_PREFIX="$(brew --prefix qt)"
QT_PLUGIN_DIR="$("$QT_PREFIX/bin/qtpaths" --plugin-dir)"

pkill -x "$APP_NAME" >/dev/null 2>&1 || true

rm -rf "$BUILD_APP_BUNDLE" "$RUNTIME_DIR"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX"
cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure
mkdir -p "$RUNTIME_DIR"
ditto --norsrc "$BUILD_APP_BUNDLE" "$APP_BUNDLE"
"$QT_PREFIX/bin/macdeployqt" "$APP_BUNDLE" -no-plugins -no-codesign
mkdir -p "$APP_BUNDLE/Contents/PlugIns/platforms"
cp "$QT_PLUGIN_DIR/platforms/libqcocoa.dylib" \
    "$APP_BUNDLE/Contents/PlugIns/platforms/"
chmod -R u+w "$APP_BUNDLE"
xattr -cr "$APP_BUNDLE"
xattr -dr com.apple.FinderInfo "$APP_BUNDLE"
xattr -dr com.apple.ResourceFork "$APP_BUNDLE" 2>/dev/null || true
codesign --force --deep --sign - "$APP_BUNDLE"

open_app() {
    /usr/bin/open -n "$APP_BUNDLE"
}

case "$MODE" in
    run)
        open_app
        ;;
    --package|package)
        ;;
    --debug|debug)
        lldb -- "$APP_BUNDLE/Contents/MacOS/$APP_NAME"
        ;;
    --logs|logs)
        open_app
        /usr/bin/log stream --info --style compact --predicate "process == \"$APP_NAME\""
        ;;
    --telemetry|telemetry)
        open_app
        /usr/bin/log stream --info --style compact --predicate "subsystem == \"$BUNDLE_ID\""
        ;;
    --verify|verify)
        open_app
        sleep 1
        pgrep -x "$APP_NAME" >/dev/null
        ;;
    *)
        echo "usage: $0 [run|--package|--debug|--logs|--telemetry|--verify]" >&2
        exit 2
        ;;
esac
