/*
 * Copyright (C) 2026 MarcusPy827
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <cstring>
#include <memory>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDebug>
#include <wayland-client.h>

#include "absl/log/log.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_cat.h"

#include "src/info_server/current_window/current_window_provider_factory.h"
#include "src/info_server/current_window/current_window_kwin_impl.h"
#include "src/info_server/current_window/current_window_wlroots_impl.h"
#include "src/info_server/current_window/current_window_xorg_impl.h"
#include "src/info_server/current_window/current_window_null_impl.h"
#include "src/utils/dbus_def.h"

namespace panel {
namespace backend {
namespace {

constexpr char kGxdeIdentifierInterface[] = "gxde_identifier_v1";

void RegistryGlobal(void* data, wl_registry*, uint32_t, const char* interface,
    uint32_t) {
  if (std::strcmp(interface, kGxdeIdentifierInterface) == 0) {
    *static_cast<bool*>(data) = true;
  }
}

void RegistryGlobalRemove(void*, wl_registry*, uint32_t) {}

constexpr wl_registry_listener kRegistryListener = {
  RegistryGlobal,
  RegistryGlobalRemove,
};

bool ProbeGxdeIdentifier() {
  wl_display* display = wl_display_connect(nullptr);
  if (display == nullptr) {
    return false;
  }

  wl_registry* registry = wl_display_get_registry(display);
  if (registry == nullptr) {
    wl_display_disconnect(display);
    return false;
  }

  bool detected = false;
  wl_registry_add_listener(registry, &kRegistryListener, &detected);
  if (wl_display_roundtrip(display) < 0) {
    detected = false;
  }
  wl_registry_destroy(registry);
  wl_display_disconnect(display);
  return detected;
}

}  // namespace

bool IsGxdeWlcom() {
  static const bool detected = !qEnvironmentVariable("WAYLAND_DISPLAY")
      .isEmpty() && ProbeGxdeIdentifier();
  return detected;
}

std::unique_ptr<CurrentWindowProvider>InitCurrentWindowInfoServer(
    QObject *parent) {
  bool is_wayland = !qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty();
  bool is_xorg = !qEnvironmentVariable("DISPLAY").isEmpty();

  QDBusConnectionInterface * session_bus = QDBusConnection::sessionBus()
    .interface();
  bool is_kwin = session_bus && session_bus->isServiceRegistered(
    DBUS_KWIN_SERVICE);

  // GXDE-Wlcom may expose KWin-compatible services. Its identifier protocol is
  // authoritative, so always use the wlroots foreign-toplevel implementation.
  if (IsGxdeWlcom()) {
    LOG(INFO) << absl::StrCat("GXDE-Wlcom session detected via ",
      "gxde-identifier-v1. Using wlr-foreign-toplevel.");
    return std::make_unique<CurrentWindowWlrootsImpl>(parent);
  }

  // The KWin D-Bus script path is only needed on Wayland + KWin.
  // For X11 + KWin then just use X11.
  if (is_wayland && is_kwin) {
    LOG(INFO) << absl::StrCat("KWin Wayland session detected. ",
      "Using KWin D-Bus app-bridge script.");
    return std::make_unique<CurrentWindowKwinImpl>(parent);
  }

  if (is_wayland) {
    LOG(ERROR) << absl::StrCat("Non-KWin Wayland session detected. ",
      "Using wlr-foreign-toplevel.");
    return std::make_unique<CurrentWindowWlrootsImpl>(parent);
  }

  if (is_xorg) {
    LOG(INFO) << absl::StrCat("X11/XOrg session detected. ",
      "Using _NET_ACTIVE_WINDOW via XCB.");
    return std::make_unique<CurrentWindowXorgImpl>(parent);
  }

  LOG(ERROR) << absl::StrCat("Current WM is NOT supported. ",
    "Current window info server could NOT be initialized.");
  return std::make_unique<CurrentWindowNullImpl>(parent);
}

}  // namespace backend
}  // namespace panel
