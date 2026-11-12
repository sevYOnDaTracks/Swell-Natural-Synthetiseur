#!/bin/bash
set -euo pipefail
release=$(cd "$1" && pwd)
out="$2"
here=$(cd "$(dirname "$0")" && pwd)
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
product='Swell - Natural Synthetiser'
root="$stage/root"
mkdir -p "$root/Applications" "$root/Library/Audio/Plug-Ins"/{VST3,Components} "$root/Library/Application Support/Swell/Natural Synthetiser" "$stage/resources"
for item in "Standalone/$product.app|Applications" "VST3/$product.vst3|Library/Audio/Plug-Ins/VST3" "AU/$product.component|Library/Audio/Plug-Ins/Components"; do
    source_path=${item%|*}
    destination="$root/${item#*|}/${source_path##*/}"
    ditto --norsrc --noextattr "$release/$source_path" "$destination"
    if [[ -n "${APP_IDENTITY:-}" ]]; then
        codesign --force --options runtime --timestamp --sign "$APP_IDENTITY" "$destination"
    else
        codesign --force --sign - "$destination"
    fi
    codesign --verify --strict "$destination"
done
install -m 644 "$here/README.txt" "$root/Library/Application Support/Swell/Natural Synthetiser/README.txt"
cp "$here/README.txt" "$stage/resources/README.txt"
pkgbuild --analyze --root "$root" "$stage/components.plist"
/usr/bin/python3 - "$stage/components.plist" <<'PY'
import plistlib, sys
path = sys.argv[1]
with open(path, 'rb') as f:
    components = plistlib.load(f)
for item in components:
    item['BundleIsRelocatable'] = False
with open(path, 'wb') as f:
    plistlib.dump(components, f)
PY
pkgbuild --root "$root" --component-plist "$stage/components.plist" --ownership recommended --install-location / --identifier com.swell.naturalsynthetiser.installer --version 2.0.1 "$stage/instrument.pkg"
cat > "$stage/Distribution.xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>Swell - Natural Synthetiser 2.0.1</title>
  <readme file="README.txt" mime-type="text/plain"/>
  <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_localSystem="true" enable_currentUserHome="false" enable_anywhere="false"/>
  <volume-check><allowed-os-versions><os-version min="11.0"/></allowed-os-versions></volume-check>
  <choices-outline><line choice="instrument"/></choices-outline>
  <choice id="instrument" visible="false"><pkg-ref id="com.swell.naturalsynthetiser.installer"/></choice>
  <pkg-ref id="com.swell.naturalsynthetiser.installer" version="2.0.1">instrument.pkg</pkg-ref>
</installer-gui-script>
XML
sign_args=()
if [[ -n "${INSTALLER_IDENTITY:-}" ]]; then sign_args=(--sign "$INSTALLER_IDENTITY" --timestamp); fi
productbuild --distribution "$stage/Distribution.xml" --resources "$stage/resources" --package-path "$stage" ${sign_args[@]+"${sign_args[@]}"} "$out"
