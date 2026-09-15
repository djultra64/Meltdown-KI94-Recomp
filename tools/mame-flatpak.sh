#!/bin/sh
set -eu

exec flatpak run org.mamedev.MAME "$@"
