/*
This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "registry.h"
#include "log.h"
#include "report.h"
#include <QApplication>
#include <QList>
#include <QMessageBox>
#include <QString>

namespace MOBase
{

/**
 *
 * @param appName The name of the section to which the string will be copied. If the section does not exist, it is created. The name of the section is case-independent; the string can be any combination of uppercase and lowercase letters.
 * @param keyName The name of the key to be associated with a string. If the key does not exist in the specified section, it is created. If this parameter is NULL, the entire section, including all entries within the section, is deleted.
 * @param value A null-terminated string to be written to the file. If this parameter is NULL, the key pointed to by the lpKeyName parameter is deleted.
 * @param fileName The name of the initialization file.
 * @return
 */

bool WriteRegistryValue(const char* appName, const char* keyName, const char* value,
                        const char* fileName)
{
  QString fileNameQString = QString("%1").arg(fileName);
  QSettings settings(fileNameQString, QSettings::Format::IniFormat);
  QString key = QString("%1/%2").arg(appName, keyName);
  bool success = true;

  settings.setValue(key, value);
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    success = false;
    switch (settings.status()) {
    case QSettings::AccessError: {
      QFile file(fileNameQString);
      if (!file.isWritable() && file.isReadable()) {
        QMessageBox::StandardButton result =
            MOBase::TaskDialog(qApp->activeModalWidget(),
                               QObject::tr("INI file is read-only"))
                .main(QObject::tr("INI file is read-only"))
                .content(QObject::tr("Mod Organizer is attempting to write to \"%1\" "
                                     "which is currently set to read-only.")
                             .arg(file.fileName()))
                .icon(QMessageBox::Warning)
                .button({QObject::tr("Clear the read-only flag"), QMessageBox::Yes})
                .button({QObject::tr("Allow the write once"),
                         QObject::tr("The file will be set to read-only again."),
                         QMessageBox::Ignore})
                .button({QObject::tr("Skip this file"), QMessageBox::No})
                .remember("clearReadOnly", file.fileName())
                .exec();

        auto oldPermissions = file.permissions();
        // clear the read-only flag if requested
        if (result & (QMessageBox::Yes | QMessageBox::Ignore)) {
          if (file.setPermissions(oldPermissions | QFile::Permission::WriteOwner)) {
            settings.setValue(key, value);
            if (settings.status() == QSettings::NoError) {
              success = true;
            }
          }
        }

        // set the read-only flag if requested
        if (result == QMessageBox::Ignore) {
          file.setPermissions(oldPermissions);
        }
      }
    } break;
    case QSettings::FormatError:
      log::error("format error while writing settings to '{}'", fileName);
      success = false;
      break;
    default:
      break;
    }
  }

  return success;
}

}  // namespace MOBase
