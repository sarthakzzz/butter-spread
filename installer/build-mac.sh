#!/bin/bash
# Builds Butter Spread (AU + VST3, universal) and packages it as a macOS installer:
#   installer/build-mac.sh   -> dist/Butter-Spread-<version>-mac.pkg
#
# Mac AU + VST3.
# Optional signing (needs an Apple Developer account):
#   DEVELOPER_ID_APP="Developer ID Application: Name (TEAMID)"
#   DEVELOPER_ID_INSTALLER="Developer ID Installer: Name (TEAMID)"
#   NOTARY_PROFILE=<keychain profile created with `xcrun notarytool store-credentials`>
# Without them, the bundles are ad-hoc signed and the .pkg is unsigned.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="ButterSpread"; PRODUCT="Butter Spread"; BUNDLE_ID="com.pluginsforbreakfast.butterspread"; PKG_NAME="Butter-Spread"
BUILD="$ROOT/build-release"
DIST="$ROOT/dist"
STAGE="$BUILD/pkgroot-butterspread"
VERSION="$(sed -n 's/^project(ButterSpread VERSION \([0-9.]*\))$/\1/p' "$ROOT/CMakeLists.txt")"
PKG="$DIST/$PKG_NAME-$VERSION-mac.pkg"
ARTEFACTS="$BUILD/${TARGET}_artefacts/Release"

echo "==> Building $PRODUCT $VERSION"
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBS_BUILD_TOOLS=OFF
cmake --build "$BUILD" --target "${TARGET}_AU" "${TARGET}_VST3"

echo "==> Staging"
rm -rf "$STAGE"
mkdir -p "$STAGE/Library/Audio/Plug-Ins/Components" "$STAGE/Library/Audio/Plug-Ins/VST3"
ditto "$ARTEFACTS/AU/$PRODUCT.component" "$STAGE/Library/Audio/Plug-Ins/Components/$PRODUCT.component"
ditto "$ARTEFACTS/VST3/$PRODUCT.vst3" "$STAGE/Library/Audio/Plug-Ins/VST3/$PRODUCT.vst3"

# Smaller download: strip local symbols from the plug-in binaries (keeps exported entry points)
find "$STAGE" -path "*/Contents/MacOS/*" -type f -exec strip -x {} \;
xattr -cr "$STAGE"   # no extended attributes (they'd show up as ._ files in the package)

echo "==> Signing plug-ins"
SIGN_ID="${DEVELOPER_ID_APP:--}"
for bundle in "$STAGE/Library/Audio/Plug-Ins/Components/$PRODUCT.component" "$STAGE/Library/Audio/Plug-Ins/VST3/$PRODUCT.vst3"; do
    if [ "$SIGN_ID" = "-" ]; then
        codesign --force --deep --sign - "$bundle"
    else
        codesign --force --deep --options runtime --timestamp --sign "$SIGN_ID" "$bundle"
    fi
done

echo "==> Packaging"
mkdir -p "$DIST"
# Always install into /Library, even if a copy of the bundle exists elsewhere
pkgbuild --analyze --root "$STAGE" "$BUILD/butterspread-components.plist"
count=$(plutil -extract "" raw -expect array "$BUILD/butterspread-components.plist" 2>/dev/null || echo 2)
for ((i = 0; i < count; i++)); do plutil -replace "$i.BundleIsRelocatable" -bool NO "$BUILD/butterspread-components.plist"; done

COPYFILE_DISABLE=1 pkgbuild --root "$STAGE" \
         --component-plist "$BUILD/butterspread-components.plist" \
         --identifier "$BUNDLE_ID" \
         --version "$VERSION" \
         --install-location / \
         --scripts "$ROOT/installer/scripts" \
         "$BUILD/$TARGET-component.pkg"

if [ -n "${DEVELOPER_ID_INSTALLER:-}" ]; then
    productbuild --package "$BUILD/$TARGET-component.pkg" --sign "$DEVELOPER_ID_INSTALLER" "$PKG"
else
    productbuild --package "$BUILD/$TARGET-component.pkg" "$PKG"
fi

if [ -n "${NOTARY_PROFILE:-}" ] && [ -n "${DEVELOPER_ID_INSTALLER:-}" ]; then
    echo "==> Notarizing"
    xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$PKG"
fi

echo "==> Done: $PKG"
