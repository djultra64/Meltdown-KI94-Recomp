#!/bin/sh
set -eu

exec flatpak run --command=chdman org.mamedev.MAME "$@"
