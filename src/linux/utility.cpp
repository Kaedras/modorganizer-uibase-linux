/*
Mod Organizer shared UI functionality

Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "utility.h"
#include "log.h"
#include "report.h"
#include <QApplication>
#include <QBuffer>
#include <QCollator>
#include <QDir>
#include <QImage>
#include <QScreen>
#include <QStringEncoder>
#include <QUuid>
#include <QtDebug>
#include <memory>
#include <sstream>
#include <format>
#include <iostream>

#include <csignal>
#include "linux/xdg.hpp"
#include <spawn.h>
#include <cerrno>

using namespace std;
namespace fs = std::filesystem;

namespace MOBase
{

enum spawnAction
{
  spawn,
  spawnp
};
string spawnActionToString(spawnAction action)
{
  switch (action) {
  case spawn:
    return "spawn";
  case spawnp:
    return "spawnp";
  }
}

bool removeDir(const QString& dirName)
{
  QDir dir(dirName);

  if (dir.exists()) {
    Q_FOREACH (QFileInfo info,
               dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden |
                                     QDir::AllDirs | QDir::Files,
                                 QDir::DirsFirst)) {
      if (info.isDir()) {
        if (!removeDir(info.absoluteFilePath())) {
          return false;
        }
      } else {
        QFile file(info.absoluteFilePath());
        if (!file.remove()) {
          reportError(QObject::tr("removal of \"%1\" failed: %2")
                          .arg(info.absoluteFilePath())
                          .arg(file.errorString()));
          return false;
        }
      }
    }

    if (!dir.rmdir(dirName)) {
      reportError(QObject::tr("removal of \"%1\" failed").arg(dir.absolutePath()));
      return false;
    }
  } else {
    reportError(QObject::tr("\"%1\" doesn't exist (remove)").arg(dirName));
    return false;
  }

  return true;
}

bool copyDir(const QString& sourceName, const QString& destinationName, bool merge)
{
  QDir sourceDir(sourceName);
  if (!sourceDir.exists()) {
    return false;
  }
  QDir destDir(destinationName);
  if (!destDir.exists()) {
    destDir.mkdir(destinationName);
  } else if (!merge) {
    return false;
  }

  QStringList files = sourceDir.entryList(QDir::Files);
  foreach (QString fileName, files) {
    QString srcName  = sourceName + "/" + fileName;
    QString destName = destinationName + "/" + fileName;
    QFile::copy(srcName, destName);
  }

  files.clear();
  // we leave out symlinks because that could cause an endless recursion
  QStringList subDirs =
      sourceDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
  foreach (QString subDir, subDirs) {
    QString srcName  = sourceName + "/" + subDir;
    QString destName = destinationName + "/" + subDir;
    copyDir(srcName, destName, merge);
  }
  return true;
}

bool shellDelete(const QStringList& fileNames, bool recycle, QWidget* dialog)
{
  bool result = true;
  for (const auto& fileName : fileNames) {
    bool r;
    QFile file = fileName;
    if (recycle) {
      r = file.moveToTrash();
    } else {
      r = file.remove();
    }
    if (!r) {
      result = r;
      int e  = errno;
      log::error("error deleting file '{}': ", formatSystemMessage(e));
    }
  }
  return result;
}

namespace shell
{

  static QString g_urlHandler;

  Result::Result(bool success, int error, QString message, pid_t process)
      : m_success(success), m_error(error), m_message(std::move(message)),
        m_process(process)
  {
    if (m_message.isEmpty()) {
      m_message = QString::fromStdString(formatSystemMessage(m_error));
    }
  }

  Result Result::makeFailure(int error, QString message)
  {
    return {false, error, std::move(message), 0};
  }

  Result Result::makeFailure(std::error_code error)
  {
    return makeFailure(error.value(), QString::fromStdString(error.message()));
  }

  Result Result::makeSuccess(pid_t process)
  {
    return {true, 0, {}, process};
  }

  bool Result::success() const
  {
    return m_success;
  }

  Result::operator bool() const
  {
    return m_success;
  }

  int Result::error() const
  {
    return m_error;
  }

  const QString& Result::message() const
  {
    return m_message;
  }

  pid_t Result::processHandle() const
  {
    return m_process;
  }

  QString Result::toString() const
  {
    if (m_message.isEmpty()) {
      return QObject::tr("Error %1").arg(m_error);
    } else {
      return m_message;
    }
  }

  void LogShellFailure(spawnAction operation, const char* file,
                       const std::vector<const char*>& params, int error)
  {
    QStringList s;

    if (operation) {
      s << ToQString(spawnActionToString(operation));
    }

    if (file) {
      s << ToQString(file);
    }

    if (!params.empty()) {
      for (auto param : params) {
        s << ToQString(param);
      }
    }

    log::error("failed to invoke '{}': {}", s.join(" "), formatSystemMessage(error));
  }

  Result ShellExecuteWrapper(spawnAction operation, const char* file,
                             vector<const char*> params)
  {
    char** environ = nullptr;

    pid_t pid  = 0;
    int status = -1;

    // The only difference between posix_spawn() and posix_spawnp() is the manner in
    // which they specify the file to be executed by the child process. With
    // posix_spawn(), the executable file is specified as a pathname (which can be
    // absolute or relative). With posix_spawnp(), the executable file is specified as a
    // simple filename; the system searches for this file in the list of directories
    // specified by PATH (in the same way as for execvp(3)).
    switch (operation) {
    case spawn:
      status = posix_spawn(&pid, file, nullptr, nullptr,
                           const_cast<char**>(params.data()), environ);
      break;
    case spawnp:
      status = posix_spawnp(&pid, file, nullptr, nullptr,
                            const_cast<char**>(params.data()), environ);
      break;
    }

    if (status != 0) {
      const auto e = errno;
      LogShellFailure(operation, file, params, e);

      return Result::makeFailure(e, QString::fromStdString(formatSystemMessage(e)));
    }

    return Result::makeSuccess(pid);
  }

  Result ShellExecuteWrapper(spawnAction operation, const char* file, const char* param)
  {
    return ShellExecuteWrapper(operation, file, vector<const char*>{param});
  }

  Result ExploreDirectory(const QFileInfo& info)
  {
    const auto path = QDir::toNativeSeparators(info.absoluteFilePath());

    return ShellExecuteWrapper(spawnp, "xdg-open", path.toStdString().c_str());
  }

  Result ExploreFileInDirectory(const QFileInfo& info)
  {
    const auto path = QDir::toNativeSeparators(info.absoluteFilePath());

    return ShellExecuteWrapper(spawnp, "xdg-open", path.toStdString().c_str());
  }

  Result Explore(const QFileInfo& info)
  {
    if (info.isFile()) {
      return ExploreFileInDirectory(info);
    } else if (info.isDir()) {
      return ExploreDirectory(info);
    } else {
      // try the parent directory
      const auto parent = info.dir();

      if (parent.exists()) {
        return ExploreDirectory(QFileInfo(parent.absolutePath()));
      } else {
        return Result::makeFailure(ENOENT);
      }
    }
  }

  Result Explore(const QString& path)
  {
    return Explore(QFileInfo(path));
  }

  Result Explore(const QDir& dir)
  {
    return Explore(QFileInfo(dir.absolutePath()));
  }

  Result Open(const QString& path)
  {
    const auto s_path = path.toStdString();
    return ShellExecuteWrapper(spawnp, "xdg-open", s_path.c_str());
  }

  Result OpenCustomURL(const std::string& format, const std::string& url)
  {
    log::debug("custom url handler: '{}'", format);

    auto cmd = std::format("'{}' '{}'", format, url);
    log::debug("running {}", cmd);

    Result r = ShellExecuteWrapper(spawn, format.c_str(), url.c_str());

    if (r.processHandle() == 0) {
      log::error("failed to run '{}'", cmd);
      log::error("{}", formatSystemMessage(r.error()));
      log::error(
          "{}",
          QObject::tr("You have an invalid custom browser command in the settings."));
      return r;
    }

    return Result::makeSuccess();
  }

  Result Open(const QUrl& url)
  {
    log::debug("opening url '{}'", url.toString());

    const auto s_url = url.toString().toStdString();

    if (g_urlHandler.isEmpty()) {
      return ShellExecuteWrapper(spawnp, "xdg-open", s_url.c_str());
    } else {
      return OpenCustomURL(g_urlHandler.toStdString(), s_url);
    }
  }

  Result Execute(const QString& program, const QString& params)
  {
    const auto program_s = program.toStdString();
    const auto params_s  = params.toStdString();

    return ShellExecuteWrapper(spawn, program_s.c_str(), params_s.c_str());
  }

  void SetUrlHandler(const QString& cmd)
  {
    g_urlHandler = cmd;
  }

  Result Delete(const QFileInfo& path)
  {
    QFile file = path.canonicalPath();
    if (!file.remove()) {
      int e = errno;
      return Result::makeFailure(e);
    }
    return Result::makeSuccess();
  }

  Result Rename(const QFileInfo& src, const QFileInfo& dest)
  {
    QFile source(src.canonicalPath());
    if (!source.rename(dest.canonicalPath())) {
      return Result::makeFailure(source.error(), source.errorString());
    }

    return Result::makeSuccess();
  }

  Result CreateDirectories(const std::filesystem::path& dir)
  {
    error_code e;
    fs::create_directory(dir, e);

    if (e.value() != 0) {
      return Result::makeFailure(e);
    }

    return Result::makeSuccess();
  }

  Result DeleteDirectoryRecursive(const QDir& dir)
  {
    error_code e;

    fs::remove_all(dir.canonicalPath().toStdString());

    if (e.value() != 0) {
      return Result::makeFailure(e);
    }

    return Result::makeSuccess();
  }

}  // namespace shell

bool moveFileRecursive(const QString& source, const QString& baseDir,
                       const QString& destination)
{
  QStringList pathComponents = destination.split("/");
  QString path               = baseDir;
  for (QStringList::Iterator iter = pathComponents.begin();
       iter != pathComponents.end() - 1; ++iter) {
    path.append("/").append(*iter);
    if (!QDir(path).exists() && !QDir().mkdir(path)) {
      reportError(QObject::tr("failed to create directory \"%1\"").arg(path));
      return false;
    }
  }

  QString destinationAbsolute = baseDir.mid(0).append("/").append(destination);
  if (!QFile::rename(source, destinationAbsolute)) {
    // move failed, try copy & delete
    if (!QFile::copy(source, destinationAbsolute)) {
      reportError(QObject::tr("failed to copy \"%1\" to \"%2\"")
                      .arg(source, destinationAbsolute));
      return false;
    } else {
      QFile::remove(source);
    }
  }
  return true;
}

bool copyFileRecursive(const QString& source, const QString& baseDir,
                       const QString& destination)
{
  QStringList pathComponents = destination.split("/");
  QString path               = baseDir;
  for (QStringList::Iterator iter = pathComponents.begin();
       iter != pathComponents.end() - 1; ++iter) {
    path.append("/").append(*iter);
    if (!QDir(path).exists() && !QDir().mkdir(path)) {
      reportError(QObject::tr("failed to create directory \"%1\"").arg(path));
      return false;
    }
  }

  QString destinationAbsolute = baseDir.mid(0).append("/").append(destination);
  if (!QFile::copy(source, destinationAbsolute)) {
    reportError(QObject::tr("failed to copy \"%1\" to \"%2\"")
                    .arg(source, destinationAbsolute));
    return false;
  }
  return true;
}

std::string ToString(const QString& source, bool utf8)
{
  QByteArray array8bit;
  if (utf8) {
    array8bit = source.toUtf8();
  } else {
    array8bit = source.toLocal8Bit();
  }
  return array8bit.constData();
}

QString ToQString(const char* source)
{
  return QString::fromLocal8Bit(source);
}

QString ToQString(const std::string& source)
{
  return QString::fromStdString(source);
}

QString ToQString(const std::wstring& source)
{
  return QString::fromStdWString(source);
}

static int naturalCompareI(const QString& a, const QString& b)
{
  static QCollator c = [] {
    QCollator temp;
    temp.setNumericMode(true);
    temp.setCaseSensitivity(Qt::CaseInsensitive);
    return temp;
  }();

  return c.compare(a, b);
}

int naturalCompare(const QString& a, const QString& b, Qt::CaseSensitivity cs)
{
  if (cs == Qt::CaseInsensitive) {
    return naturalCompareI(a, b);
  }

  static QCollator c = [] {
    QCollator temp;
    temp.setNumericMode(true);
    return temp;
  }();

  return c.compare(a, b);
}

QString getDesktopDirectory()
{
  // NOTE: there could potentially be issues without an XDG-compliant desktop
  // environment if a user does not know what this means, they usually use a compliant
  // one
  string configHome = xdg::ConfigHomeDir();
  QFile conf        = QString::fromStdString(configHome + "/user-dirs.dirs");
  if (conf.open(QIODevice::ReadOnly)) {
    QByteArrayView lookup = "XDG_DESKTOP_DIR=";
    while (!conf.atEnd()) {
      QByteArray line = conf.readLine();
      if (line.startsWith(lookup)) {
        line.remove(0, lookup.length());
        // adjust array size
        line.squeeze();
        return line;
      }
    }
  }

  filesystem::path home = getenv("HOME");
  // use $HOME/Desktop as default
  return QString::fromStdString(home / "Desktop");
}

QString getStartMenuDirectory()
{
  std::filesystem::path path = xdg::DataHomeDir();
  path /= "applications";
  return QString::fromStdString(path);
}

bool shellDeleteQuiet(const QString& fileName, QWidget* dialog)
{
  if (!QFile::remove(fileName)) {
    return shellDelete(QStringList(fileName), false, dialog);
  }
  return true;
}

QString readFileText(const QString& fileName, QString* encoding)
{
  QFile textFile(fileName);
  if (!textFile.open(QIODevice::ReadOnly)) {
    return {};
  }

  QByteArray buffer = textFile.readAll();
  return decodeTextData(buffer, encoding);
}

QString decodeTextData(const QByteArray& fileData, QString* encoding)
{
  QStringConverter::Encoding codec = QStringConverter::Encoding::Utf8;
  QStringEncoder encoder(codec);
  QStringDecoder decoder(codec);
  QString text = decoder.decode(fileData);

  // check reverse conversion. If this was unicode text there can't be data loss
  // this assumes QString doesn't normalize the data in any way so this is a bit unsafe
  if (encoder.encode(text) != fileData) {
    log::debug("conversion failed assuming local encoding");
    auto codecSearch = QStringConverter::encodingForData(fileData);
    if (codecSearch.has_value()) {
      codec   = codecSearch.value();
      decoder = QStringDecoder(codec);
    } else {
      decoder = QStringDecoder(QStringConverter::Encoding::System);
    }
    text = decoder.decode(fileData);
  }

  if (encoding != nullptr) {
    *encoding = QStringConverter::nameForEncoding(codec);
  }

  return text;
}

void removeOldFiles(const QString& path, const QString& pattern, int numToKeep,
                    QDir::SortFlags sorting)
{
  QFileInfoList files =
      QDir(path).entryInfoList(QStringList(pattern), QDir::Files, sorting);

  if (files.count() > numToKeep) {
    QStringList deleteFiles;
    for (int i = 0; i < files.count() - numToKeep; ++i) {
      deleteFiles.append(files.at(i).absoluteFilePath());
    }
    for (const auto& file : deleteFiles) {
      QFile f(file);
      if (!f.remove()) {
        log::error("failed to remove log files: {}", f.errorString());
      }
    }
  }
}

QIcon iconForExecutable(const QString& filepath)
{
  auto result = shell::ShellExecuteWrapper(
      spawnp, "7z",
      {"x", filepath.toStdString().c_str(), ".rsrc/ICON/1", "-o /tmp/mo2/.rsrc/"});
  if (result.success()) {
    return QIcon("/tmp/mo-icon.ico");
  }

  return QIcon(":/MO/gui/executable");
}

enum version_t
{
  fileversion,
  productversion
};

QString getFileVersionInfo(QString const& filepath, version_t type)
{
  auto result = shell::ShellExecuteWrapper(
      spawnp, "7z",
      {"x", filepath.toStdString().c_str(), ".rsrc/version.txt", "-o /tmp/mo2/.rsrc/"});

  QFile versionFile("/tmp/mo2/.rsrc/version.txt");
  QString version = "1.0.0";

  string keyword;
  switch (type) {
  case fileversion:
    keyword = "FILEVERSION";
    break;
  case productversion:
    keyword = "PRODUCTVERSION";
    break;
  }

  // convert
  // FILEVERSION     1,3,22,0 or FILEVERSION     1,3,22,0
  // to
  // 1.3.22.0
  if (versionFile.isOpen()) {
    return version;
  }
  while (!versionFile.atEnd()) {
    auto line = versionFile.readLine();
    if (line.startsWith(keyword)) {
      line.remove(0, keyword.length());
      // remove whitespaces
      version = line.trimmed();
      // replace '' with ''
      version.replace(',', '.');
      break;
    }
  }

  return version;
}

QString getFileVersion(QString const& filepath)
{
  return getFileVersionInfo(filepath, fileversion);
}

QString getProductVersion(QString const& filepath)
{
  return getFileVersionInfo(filepath, productversion);
}

void deleteChildWidgets(QWidget* w)
{
  auto* ly = w->layout();
  if (!ly) {
    return;
  }

  while (auto* item = ly->takeAt(0)) {
    delete item->widget();
    delete item;
  }
}

void trimWString(std::wstring& s)
{
  s.erase(std::remove_if(s.begin(), s.end(),
                         [](wint_t ch) {
                           return std::iswspace(ch);
                         }),
          s.end());
}

inline std::string formatMessage(int error, const string& message)
{
  return to_string(error) + ": " + message;
}

inline std::string formatSystemMessage(int id)
{
  return formatMessage(id, strerror(id));
}

QString localizedSize(unsigned long long bytes, const QString& B, const QString& KB,
                      const QString& MB, const QString& GB, const QString& TB)
{
  constexpr unsigned long long OneKB = 1024ull;
  constexpr unsigned long long OneMB = 1024ull * 1024;
  constexpr unsigned long long OneGB = 1024ull * 1024 * 1024;
  constexpr unsigned long long OneTB = 1024ull * 1024 * 1024 * 1024;

  auto makeNum = [&](int factor) {
    const double n = static_cast<double>(bytes) / std::pow(1024.0, factor);

    // avoids rounding something like "1.999" to "2.00 KB"
    const double truncated =
        static_cast<double>(static_cast<unsigned long long>(n * 100)) / 100.0;

    return QString().setNum(truncated, 'f', 2);
  };

  if (bytes < OneKB) {
    return B.arg(bytes);
  } else if (bytes < OneMB) {
    return KB.arg(makeNum(1));
  } else if (bytes < OneGB) {
    return MB.arg(makeNum(2));
  } else if (bytes < OneTB) {
    return GB.arg(makeNum(3));
  } else {
    return TB.arg(makeNum(4));
  }
}

QDLLEXPORT QString localizedByteSize(unsigned long long bytes)
{
  return localizedSize(bytes, QObject::tr("%1 B"), QObject::tr("%1 KB"),
                       QObject::tr("%1 MB"), QObject::tr("%1 GB"),
                       QObject::tr("%1 TB"));
}

QDLLEXPORT QString localizedByteSpeed(unsigned long long bps)
{
  return localizedSize(bps, QObject::tr("%1 B/s"), QObject::tr("%1 KB/s"),
                       QObject::tr("%1 MB/s"), QObject::tr("%1 GB/s"),
                       QObject::tr("%1 TB/s"));
}

QDLLEXPORT QString localizedTimeRemaining(unsigned int remaining)
{
  QString Result;
  double interval;
  qint64 intval;

  // Hours
  interval = 60.0 * 60.0 * 1000.0;
  intval   = (qint64)trunc((double)remaining / interval);
  if (intval < 0)
    intval = 0;
  remaining -= static_cast<unsigned int>(trunc((double)intval * interval));
  qint64 hours = intval;

  // Minutes
  interval = 60.0 * 1000.0;
  intval   = (qint64)trunc((double)remaining / interval);
  if (intval < 0)
    intval = 0;
  remaining -= static_cast<unsigned int>(trunc((double)intval * interval));
  qint64 minutes = intval;

  // Seconds
  interval = 1000.0;
  intval   = (qint64)trunc((double)remaining / interval);
  if (intval < 0)
    intval = 0;
  remaining -= static_cast<unsigned int>(trunc((double)intval * interval));
  qint64 seconds = intval;

  // Whatever is left over is milliseconds

  char buffer[25];
  memset(buffer, 0, 25);

  if (hours > 0) {
    if (hours < 10)
      sprintf(buffer, "0%lld", hours);
    else
      sprintf(buffer, "%lld", hours);
    Result.append(QString("%1:").arg(buffer));
  }

  if (minutes > 0 || hours > 0) {
    if (minutes < 10 && hours > 0)
      sprintf(buffer, "0%lld", minutes);
    else
      sprintf(buffer, "%lld", minutes);
    Result.append(QString("%1:").arg(buffer));
  }

  if (seconds < 10 && (minutes > 0 || hours > 0))
    sprintf(buffer, "0%lld", seconds);
  else
    sprintf(buffer, "%lld", seconds);
  Result.append(QString("%1").arg(buffer));

  if (hours > 0)
    //: Time remaining hours
    Result.append(QApplication::translate("uibase", "h"));
  else if (minutes > 0)
    //: Time remaining minutes
    Result.append(QApplication::translate("uibase", "m"));
  else
    //: Time remaining seconds
    Result.append(QApplication::translate("uibase", "s"));

  return Result;
}

QDLLEXPORT void localizedByteSizeTests()
{
  auto f = [](unsigned long long n) {
    return localizedByteSize(n).toStdString();
  };

#define CHECK_EQ(a, b)                                                                 \
  if ((a) != (b)) {                                                                    \
    std::cerr << "failed: " << a << " == " << b << "\n";                               \
    emit(SIGTRAP);                                                                     \
  }

  CHECK_EQ(f(0), "0 B");
  CHECK_EQ(f(1), "1 B");
  CHECK_EQ(f(999), "999 B");
  CHECK_EQ(f(1000), "1000 B");
  CHECK_EQ(f(1023), "1023 B");

  CHECK_EQ(f(1024), "1.00 KB");
  CHECK_EQ(f(2047), "1.99 KB");
  CHECK_EQ(f(2048), "2.00 KB");
  CHECK_EQ(f(1048575), "1023.99 KB");

  CHECK_EQ(f(1048576), "1.00 MB");
  CHECK_EQ(f(1073741823), "1023.99 MB");

  CHECK_EQ(f(1073741824), "1.00 GB");
  CHECK_EQ(f(1099511627775), "1023.99 GB");

  CHECK_EQ(f(1099511627776), "1.00 TB");
  CHECK_EQ(f(2759774185818), "2.51 TB");

#undef CHECK_EQ
}

TimeThis::TimeThis(const QString& what) : m_running(false)
{
  start(what);
}

TimeThis::~TimeThis()
{
  stop();
}

void TimeThis::start(const QString& what)
{
  stop();

  m_what    = what;
  m_start   = Clock::now();
  m_running = true;
}

void TimeThis::stop()
{
  using namespace std::chrono;

  if (!m_running) {
    return;
  }

  const auto end = Clock::now();
  const auto d   = duration_cast<milliseconds>(end - m_start).count();

  if (m_what.isEmpty()) {
    log::debug("timing: {} ms", d);
  } else {
    log::debug("timing: {} {} ms", m_what, d);
  }

  m_running = false;
}

}  // namespace MOBase
