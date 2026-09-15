#!/bin/sh
set -eu

# The Flatpak creates this file on first GUI launch. Headless sessions need the
# same initialization when they run first.
exec flatpak run --command=sh org.ghidra_sre.Ghidra -c '
test -f /var/config/ghidra.properties || \
  cp /app/lib/ghidra/support/launch.properties.orig /var/config/ghidra.properties
cd /app/lib/ghidra
exec support/analyzeHeadless "$@"
' ghidra-headless "$@"
