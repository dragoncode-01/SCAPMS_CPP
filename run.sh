#!/bin/sh
set -eu
cd "$(dirname "$0")"
if [ ! -x build/scapms ]; then
  make
fi
exec ./build/scapms "$@"
