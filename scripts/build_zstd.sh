#!/usr/bin/env bash

# ---------------------------------------------------------------
# Usage:
#   ./scripts/build_zstd.sh            # prepare only
#   ./scripts/build_zstd.sh --build    # prepare + makepkg
# ---------------------------------------------------------------
set -e

log() {
  # $1: level
  # $2: content
  case $1 in
    "info")
      echo -e "\x1b[39;1m[\x1b[34;1mINFO\x1b[39;1m]\x1b[0m $2\x1b[0m"
      ;;
    "warn")
      echo -e "\x1b[39;1m[\x1b[33;1mWARN\x1b[39;1m]\x1b[0m $2\x1b[0m"
      ;;
    "ok")
      echo -e "\x1b[39;1m[\x1b[32;1m OK \x1b[39;1m]\x1b[0m $2\x1b[0m"
      ;;
    "error")
      echo -e "\x1b[39;1m[\x1b[31;1mERROR\x1b[39;1m]\x1b[0m $2\x1b[0m"
      ;;
    *)
      echo -e "\x1b[39;1m[\x1b[36;1mDEBUG\x1b[39;1m]\x1b[0m $2\x1b[0m"
      ;;
  esac
}

log_tag() {
  # $1: level
  # $2: tag
  # $3: content
  log "$1" "\x1b[1;4m$2\x1b[0m: $3\x1b[0m"
}

bold() {
  # $1: content
  echo -e "\x1b[1m$1\x1b[0m"
}

# ======================== Variables ========================

APP_NAME="husky-panel"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

APP_VERSION="$(grep -m1 '^pkgver=' "${PROJECT_ROOT}/arch/PKGBUILD" | cut -d= -f2)"

CI_BUILD_DIR="${PROJECT_ROOT}/ci_build"

DO_BUILD=false
for arg in "$@"; do
  case "$arg" in
    --build) DO_BUILD=true ;;
    *)
      log "error" "Unknown argument: $arg"
      log "info" "Usage: $0 [--build]"
      exit 1
      ;;
  esac
done

# ======================== Banner ========================

bold "HuskyPanel Arch Package Build Helper"
echo "===================================="
log "info" "App: ${APP_NAME}  Version: ${APP_VERSION}"
if $DO_BUILD; then
  log "info" "Mode: prepare + build .pkg.tar.zst"
else
  log "info" "Mode: prepare only (pass --build to also run makepkg)"
fi

# ======================== Clean ci_build ========================

log "info" "\n"
if [ -d "${CI_BUILD_DIR}" ]; then
  if [ "$(ls -A "${CI_BUILD_DIR}" 2>/dev/null)" ]; then
    log "warn" "Cleaning previous ci_build artifacts in \"${CI_BUILD_DIR}\"..."
    rm -rf "${CI_BUILD_DIR:?}"/*
    log "ok" "ci_build directory cleaned."
  fi
else
  mkdir -p "${CI_BUILD_DIR}"
  log "ok" "Created ci_build directory: ${CI_BUILD_DIR}"
fi

# ======================== Check PKGBUILD ========================

# arch/PKGBUILD builds directly from this repository's source tree, so no
# source tarball is needed. makepkg must run from arch/ so that the PKGBUILD
# can locate the repository root via "$startdir/..".
PKGBUILD_DIR="${PROJECT_ROOT}/arch"

if [ ! -f "${PKGBUILD_DIR}/PKGBUILD" ]; then
  log "error" "PKGBUILD not found at ${PKGBUILD_DIR}/PKGBUILD"
  exit 1
fi

# ======================== Build pkg ========================

if $DO_BUILD; then
  log "info" "\n"
  log "info" "Setting up makepkg tree..."

  if ! command -v makepkg >/dev/null 2>&1; then
    log "error" "makepkg is not installed. This script must run on an Arch-based system (package 'pacman')."
    exit 1
  fi

  MAKEPKG_DIR="${CI_BUILD_DIR}/archbuild"
  mkdir -p "${MAKEPKG_DIR}"

  log "info" "Running makepkg... this may take a while."
  (
    cd "${PKGBUILD_DIR}"
    BUILDDIR="${MAKEPKG_DIR}" PKGDEST="${MAKEPKG_DIR}" \
      makepkg -f --cleanbuild --noconfirm
  )

  log "ok" "Arch package build finished!"
  log "info" "Packages:"
  find "${MAKEPKG_DIR}" -maxdepth 1 -name '*.pkg.tar.zst' -exec echo "  {}" \;
fi

# ======================== Summary ========================

log "info" "\n"
log "ok" "All done! Artifacts in ${CI_BUILD_DIR}/:"
ls -lh "${CI_BUILD_DIR}/" | tail -n +2

if ! $DO_BUILD; then
  log "info" "\n"
  log "info" "To build the package, either re-run with --build:"
  log "info" "  $0 --build"
  log "info" "Or manually (from arch/):"
  log "info" "  cd ${PKGBUILD_DIR} && makepkg -f"
fi
