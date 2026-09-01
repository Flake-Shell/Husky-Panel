/*
 * Copyright (C) 2025 MarcusPy827
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

#include <QDBusInterface>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

#include "absl/log/log.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_cat.h"

#include "src/info_server/user_info/user_info.h"
#include "src/info_server/current_window/current_window_provider_factory.h"
#include "src/utils/dbus_def.h"

namespace panel {
namespace backend {

namespace {

QString NormalizeAvatarPath(const QString& avatar) {
  const QUrl url(avatar);
  if (url.isLocalFile()) {
    return url.toLocalFile();
  }
  if (url.scheme().isEmpty()) {
    return avatar;
  }
  return {};
}

QString ReadAvatarPath(const QString& service, const QString& path,
    const QString& interface_name) {
  QDBusInterface interface(service, path, interface_name,
    QDBusConnection::systemBus());
  if (!interface.isValid()) {
    return {};
  }

  const QVariant avatar_read = interface.property("IconFile");
  if (!avatar_read.isValid()) {
    return {};
  }

  const QString avatar_path = NormalizeAvatarPath(avatar_read.toString());
  if (!avatar_path.isEmpty() && QFile::exists(avatar_path)) {
    return avatar_path;
  }
  return {};
}

}  // namespace

UserInfo::UserInfo(QObject* parent) : QObject(parent) {
  LOG(INFO) << absl::StrCat("UserInfo constructer invoked!!");
  LOG(WARNING) << absl::StrCat("Only invoke this constructor in main.cc while ",
    "injecting this class to QML. Otherwise there is NO NEED to create an ",
    "instance as all methods in this class are static.");
}

QString UserInfo::GetUserName() {
  uid_t uid = getuid();
  QString result = "User Profile 1";
  QString user_path = QString("/org/freedesktop/Accounts/User%1").arg(uid);
  QDBusInterface interface(DBUS_ACCOUNTS_SERVICE, user_path,
    DBUS_ACCOUNTS_USER_INTERFACE, QDBusConnection::systemBus());
  if (interface.isValid()) {
    QVariant name_read = interface.property("RealName");
    if (name_read.isValid() && !name_read.toString().isEmpty()) {
      result = name_read.toString();
      LOG(INFO) << absl::StrCat(absl::StrFormat(
        "Successfully got user display name from D-Bus: %s.",
        result.toStdString()));
      return result;
    }
  }

  LOG(WARNING) << absl::StrCat("Failed to get user display name from D-Bus. ",
    "Now falling back to login name...");
  result = QString::fromLocal8Bit(qgetenv("USER"));
  return result;
}

QString UserInfo::GetUserAvatarPath() {
  uid_t uid = getuid();
  QString result;

  if (IsGxdeWlcom()) {
    result = ReadAvatarPath(DBUS_DEEPIN_ACCOUNTS_SERVICE,
      QStringLiteral(DBUS_DEEPIN_ACCOUNTS_USER_PATH) + QString::number(uid),
      DBUS_DEEPIN_ACCOUNTS_USER_INTERFACE);
    if (!result.isEmpty()) {
      LOG(INFO) << absl::StrCat(absl::StrFormat(
        "Successfully got GXDE user avatar path from D-Bus: %s.",
        result.toStdString()));
      return result;
    }
  }

  result = ReadAvatarPath(DBUS_ACCOUNTS_SERVICE,
    QString("/org/freedesktop/Accounts/User%1").arg(uid),
    DBUS_ACCOUNTS_USER_INTERFACE);
  if (!result.isEmpty()) {
    LOG(INFO) << absl::StrCat(absl::StrFormat(
      "Successfully got user avatar path from D-Bus: %s.",
      result.toStdString()));
    return result;
  }

  LOG(WARNING) << absl::StrCat("Failed to get user avatar path from D-Bus. ",
    "Now falling back to the user face files...");
  const QString user_home = QDir::homePath();
  const QStringList fallback_paths = {
    user_home + "/.face.icon",
    user_home + "/.face",
  };
  for (const QString& fallback_path : fallback_paths) {
    if (QFile::exists(fallback_path)) {
      LOG(INFO) << absl::StrCat(absl::StrFormat(
        "Successfully got user avatar path from face file: %s.",
        fallback_path.toStdString()));
      return fallback_path;
    }
  }

  LOG(ERROR) << absl::StrCat("Failed to get user avatar path from D-Bus and ",
    "no user face file exists, returning empty string.");
  return {};
}

}  // namespace backend
}  // namespace panel
