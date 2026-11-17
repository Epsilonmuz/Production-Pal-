#!/bin/bash
# Builds "Production Pal Installer.pkg": no compiling needed on the user's Mac.
# It installs:
#   /Applications/Production Pal.app
#   /Library/Audio/Plug-Ins/VST3/Pal Ears.vst3
#   /Library/Audio/Plug-Ins/Components/Pal Ears.component
#   the FL Studio script into ~/Documents/Image-Line/FL Studio/Settings/Hardware/Production Pal
#
# Usage: installer/build_pkg.sh <cmake-build-dir> <version> <output-dir>
set -euo pipefail

BUILD="$1"; VERSION="$2"; OUT="$3"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="$(mktemp -d)"
SCRIPTS="$(mktemp -d)"

APP="$BUILD/ProductionPalApp_artefacts/Release/Production Pal.app"
VST3="$BUILD/PalEars_artefacts/Release/VST3/Pal Ears.vst3"
AU="$BUILD/PalEars_artefacts/Release/AU/Pal Ears.component"

mkdir -p "$STAGE/Applications" \
         "$STAGE/Library/Audio/Plug-Ins/VST3" \
         "$STAGE/Library/Audio/Plug-Ins/Components" \
         "$STAGE/Library/Application Support/Production Pal/FL Studio"

ditto "$APP"  "$STAGE/Applications/Production Pal.app"
ditto "$VST3" "$STAGE/Library/Audio/Plug-Ins/VST3/Pal Ears.vst3"
ditto "$AU"   "$STAGE/Library/Audio/Plug-Ins/Components/Pal Ears.component"
ditto "$ROOT/FLStudio/Production Pal" "$STAGE/Library/Application Support/Production Pal/FL Studio/Production Pal"

# Ad-hoc sign so Apple Silicon will load everything.
for b in "$STAGE/Applications/Production Pal.app" \
         "$STAGE/Library/Audio/Plug-Ins/VST3/Pal Ears.vst3" \
         "$STAGE/Library/Audio/Plug-Ins/Components/Pal Ears.component"; do
  codesign --force --deep --sign - "$b"
done

cat > "$SCRIPTS/postinstall" <<'EOF'
#!/bin/bash
# Copy the FL Studio script into the logged-in user's FL Studio Hardware folder.
USER_NAME=$(stat -f%Su /dev/console)
USER_HOME=$(dscl . -read "/Users/$USER_NAME" NFSHomeDirectory | awk '{print $2}')
SRC="/Library/Application Support/Production Pal/FL Studio/Production Pal"
DEST="$USER_HOME/Documents/Image-Line/FL Studio/Settings/Hardware/Production Pal"
if [ -d "$SRC" ] && [ -n "$USER_HOME" ]; then
  mkdir -p "$DEST"
  cp -f "$SRC/"*.py "$DEST/"
  chown -R "$USER_NAME" "$USER_HOME/Documents/Image-Line/FL Studio/Settings/Hardware/Production Pal"
fi
# Clear quarantine so the plugins load without a Gatekeeper prompt.
xattr -dr com.apple.quarantine "/Applications/Production Pal.app" 2>/dev/null || true
xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Pal Ears.vst3" 2>/dev/null || true
xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/Components/Pal Ears.component" 2>/dev/null || true
# Make Logic/GarageBand rescan Audio Units.
killall -9 AudioComponentRegistrar 2>/dev/null || true
exit 0
EOF
chmod +x "$SCRIPTS/postinstall"

# Bundles must install exactly where we put them (not "relocated" to an old copy).
pkgbuild --analyze --root "$STAGE" "$SCRIPTS/components.plist"
i=0
while /usr/libexec/PlistBuddy -c "Print :$i" "$SCRIPTS/components.plist" >/dev/null 2>&1; do
  /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$SCRIPTS/components.plist" 2>/dev/null || true
  /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$SCRIPTS/components.plist"
  i=$((i + 1))
done

mkdir -p "$OUT"
pkgbuild --root "$STAGE" \
         --component-plist "$SCRIPTS/components.plist" \
         --scripts "$SCRIPTS" \
         --identifier com.epsilonsounds.productionpal.pkg \
         --version "$VERSION" \
         --install-location / \
         "$OUT/Production Pal Installer.pkg"

cp "$ROOT/installer/INSTALL.txt" "$OUT/INSTALL.txt"
echo "Built $OUT/Production Pal Installer.pkg"
