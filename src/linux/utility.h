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

#ifndef MO_UIBASE_UTILITY_INCLUDED
#define MO_UIBASE_UTILITY_INCLUDED

#include <QDir>
#include <QIcon>
#include <QList>
#include <QString>
#include <QTextStream>
#include <QUrl>
#include <QVariant>
#include <algorithm>
#include <set>
#include <vector>

#include "dllimport.h"
#include "exceptions.h"

namespace MOBase
{

/**
 * @brief remove the specified directory including all sub-directories
 *
 * @param dirName name of the directory to delete
 * @return true on success. in case of an error, "removeDir" itself displays an error
 *message
 **/
QDLLEXPORT bool removeDir(const QString& dirName);

/**
 * @brief copy a directory recursively
 * @param sourceName name of the directory to copy
 * @param destinationName name of the target directory
 * @param merge if true, the destination directory is allowed to exist, files will then
 *              be added to that directory. If false, the call will fail in that case
 * @return true if files were copied. This doesn't necessary mean ALL files were copied
 * @note symbolic links are not followed to prevent endless recursion
 */
QDLLEXPORT bool copyDir(const QString& sourceName, const QString& destinationName,
                        bool merge);

/**
 * @brief move a file, creating subdirectories as needed
 * @param source source file name
 * @param destination destination file name
 * @return true if the file was successfully copied
 */
QDLLEXPORT bool moveFileRecursive(const QString& source, const QString& baseDir,
                                  const QString& destination);

/**
 * @brief copy a file, creating subdirectories as needed
 * @param source source file name
 * @param destination destination file name
 * @return true if the file was successfully copied
 */
QDLLEXPORT bool copyFileRecursive(const QString& source, const QString& baseDir,
                                  const QString& destination);

/**
 * @brief copy one or multiple files and ask the user for confirmation on overwrite
 * @param sourceNames names of files to be copied. This can include wildcards
 * @param destinationNames names of the files in the destination location or the
 *destination directory to copy to. There has to be one destination name for each source
 *name or a single directory
 * @param dialog a dialog to be the parent of possible confirmation dialogs
 * @return true on success, false on error. Call errno to retrieve error code
 **/
QDLLEXPORT bool copy(const QStringList& sourceNames,
                          const QStringList& destinationNames,
                          QWidget* dialog = nullptr);

/**
 * @brief copy one or multiple files and ask the user for confirmation on overwrite
 * @param sourceName names of file to be copied. This can include wildcards
 * @param destinationName name of the files in the destination location or the
 *destination directory to copy to. There has to be one destination name for each source
 *name or a single directory
 * @param yesToAll if true, the operation will assume "yes" to overwrite confirmations.
 *This doesn't seem to work when providing multiple files to copy
 * @param dialog a dialog to be the parent of possible confirmation dialogs
 * @return true on success, false on error. Call errno to retrieve error code
 **/
QDLLEXPORT bool copy(const QString& sourceNames, const QString& destinationNames,
                          bool yesToAll = false, QWidget* dialog = nullptr);

/**
 * @brief move one or multiple files and ask the user for confirmation on overwrite
 * @param sourceNames names of files to be moved. This can include wildcards
 * @param destinationNames names of the files in the destination location or the
 *destination directory to move to. There has to be one destination name for each source
 *name or a single directory
 * @param dialog a dialog to be the parent of possible confirmation dialogs
 * @return true on success, false on error. Call errno to retrieve error code
 **/
QDLLEXPORT bool move(const QStringList& sourceNames,
                          const QStringList& destinationNames,
                          QWidget* dialog = nullptr);

/**
 * @brief move one file and ask the user for confirmation on overwrite
 * @param sourceNames names of files to be moved. This can include wildcards
 * @param destinationNames names of the files in the destination location or the
 *destination directory to move to. There has to be one destination name for each source
 *name or a single directory
 * @param dialog a dialog to be the parent of possible confirmation dialogs
 * @return true on success, false on error. Call errno to retrieve error code
 **/
QDLLEXPORT bool move(const QString& sourceNames, const QString& destinationNames,
                          bool yesToAll = false, QWidget* dialog = nullptr);

/**
 * @brief rename a file and ask the user for confirmation on overwrite
 * @param oldName old name of file to be renamed
 * @param newName new name of the file
 * @param dialog a dialog to be the parent of possible confirmation dialogs
 * @param yesToAll if true, the operation will assume "yes" to all overwrite
 *confirmations
 * @return true on success, false on error. Call errno to retrieve error code
 **/
QDLLEXPORT bool shellRename(const QString& oldName, const QString& newName,
                            bool yesToAll = false, QWidget* dialog = nullptr);

/**
 * @brief delete files
 * @param fileNames names of files to be deleted
 * @param recycle if true, the file goes to the recycle bin instead of being permanently
 *deleted
 * @return true on success, false on error. Call errno to retrieve error code
 **/
QDLLEXPORT bool shellDelete(const QStringList& fileNames, bool recycle = false,
                            QWidget* dialog = nullptr);
QDLLEXPORT bool shellDeleteQuiet(const QString& fileName, QWidget* dialog = nullptr);

namespace shell
{
  // returned by the various shell functions; note that the process handle is
  // closed in the destructor, unless stealProcessHandle() was called
  //
  class QDLLEXPORT Result
  {
  public:
    Result(bool success, int error, QString message, pid_t process);

    // non-copyable
    Result(const Result&)            = delete;
    Result& operator=(const Result&) = delete;
    Result(Result&&)                 = default;
    Result& operator=(Result&&)      = default;

    static Result makeFailure(int error, QString message = {});
    static Result makeFailure(std::error_code error);
    static Result makeSuccess(pid_t process = 0);

    // whether the operation was successful
    //
    bool success() const;
    explicit operator bool() const;

    // error returned by the underlying function
    //
    int error() const;

    // string representation of the message, may be localized
    //
    const QString& message() const;

    // process handle, if any
    //
    pid_t processHandle() const;
    // the message, or the error number if empty
    //
    QString toString() const;

  private:
    bool m_success;
    int m_error;
    QString m_message;
    pid_t m_process;
  };

  // returns a string representation of the given shell error; these errors are
  // returned as an HINSTANCE from various functions such as ShellExecuteW() or
  // FindExecutableW()
  //
  QDLLEXPORT QString formatError(int i);

  // starts explorer using the given directory and/or file
  //
  // if `info` is a directory, opens it in explorer; if it's a file, opens the
  // directory and selects it
  //
  QDLLEXPORT Result Explore(const QFileInfo& info);

  // starts explorer using the given directory and/or file
  //
  // if `path` is a directory, opens it in explorer; if it's a file, opens the
  // directory and selects it
  //
  QDLLEXPORT Result Explore(const QString& path);

  // starts explorer using the given directory
  //
  QDLLEXPORT Result Explore(const QDir& dir);

  // asks the shell to open the given file with its default handler
  //
  QDLLEXPORT Result Open(const QString& path);

  // asks the shell to open the given link with the default browser
  //
  QDLLEXPORT Result Open(const QUrl& url);

  // @brief asks the shell to execute the given program, with optional
  // parameters
  //
  QDLLEXPORT Result Execute(const QString& program, const QString& params = {});

  // asks the shell to delete the given file (not directory)
  //
  QDLLEXPORT Result Delete(const QFileInfo& path);

  // asks the shell to rename a file or directory from `src` to `dest`; works
  // across volumes
  //
  QDLLEXPORT Result Rename(const QFileInfo& src, const QFileInfo& dest);
  QDLLEXPORT Result Rename(const QFileInfo& src, const QFileInfo& dest,
                           bool copyAllowed);

  QDLLEXPORT Result CreateDirectories(const QDir& dir);
  QDLLEXPORT Result DeleteDirectoryRecursive(const QDir& dir);

  // sets the command used for Open() with a QUrl, %1 is replaced by the URL;
  // pass an empty string to use the system handler
  //
  QDLLEXPORT void SetUrlHandler(const QString& cmd);
}  // namespace shell

/**
 * @brief construct a string containing the elements of a vector concatenated
 *
 * @param value the container to concatenate
 * @param separator sperator to put between elements
 * @param maximum maximum number of elements to print. If there are more elements, "..."
 *is appended to the string Defaults to UINT_MAX.
 * @return a string containing up to "maximum" elements from "value" separated by
 *"separator"
 **/
template <typename T>
QString VectorJoin(const std::vector<T>& value, const QString& separator,
                   size_t maximum = UINT_MAX)
{
  QString result;
  if (!value.empty()) {
    QTextStream stream(&result);
    stream << value[0];
    for (unsigned int i = 1; i < (std::min)(value.size(), maximum); ++i) {
      stream << separator << value[i];
    }
    if (maximum < value.size()) {
      stream << separator << "...";
    }
  }
  return result;
}

/**
 * @brief construct a string containing the elements of a std::set concatenated
 *
 * @param value the container to concatenate
 * @param separator sperator to put between elements
 * @param maximum maximum number of elements to print. If there are more elements, "..."
 *is appended to the string Defaults to UINT_MAX.
 * @return a string containing up to "maximum" elements from "value" separated by
 *"separator"
 **/
template <typename T>
QString SetJoin(const std::set<T>& value, const QString& separator,
                size_t maximum = UINT_MAX)
{
  QString result;
  typename std::set<T>::const_iterator iter = value.begin();
  if (iter != value.end()) {
    QTextStream stream(&result);
    stream << *iter;
    ++iter;
    unsigned int pos = 1;
    for (; iter != value.end() && pos < maximum; ++iter) {
      stream << separator << *iter;
    }
    if (maximum < value.size()) {
      stream << separator << "...";
    }
  }
  return result;
}

template <typename T>
QList<T> ConvertList(const QVariantList& variants)
{
  QList<T> result;
  for (const QVariant& var : variants) {
    if (!var.canConvert<T>()) {
      throw Exception("invalid variant type");
    }
    result.append(var.value<T>());
  }
}

/**
 * @brief convert QString to std::string
 * @param source source string
 * @param utf8 if true, the output string is utf8, otherwise it's the local 8bit
 *encoding (according to qt)
 **/
QDLLEXPORT std::string ToString(const QString& source, bool utf8 = true);

/**
 * @brief convert c string to QString using local 8bit
 * @param source
 * @return
 */
QDLLEXPORT QString ToQString(const char* source);

/**
 * @brief convert std::string to QString (assuming the string to be utf-8 encoded)
 **/
QDLLEXPORT QString ToQString(const std::string& source);

/**
 * @brief convert std::wstring to QString (assuming the wstring to be utf-16 encoded)
 **/
QDLLEXPORT QString ToQString(const std::wstring& source);

#if 0
// TODO: check if this function should be implemented
/**
 * @brief convert a systemtime object to a string containing date and time in local
 *representation
 *
 * @param time the time to convert
 * @return string representation of the time object
 **/
QDLLEXPORT QString ToString(const SYSTEMTIME& time);
#endif
// three-way compare for natural sorting (case-insensitive default, 10 comes
// after 2)
//
QDLLEXPORT int naturalCompare(const QString& a, const QString& b,
                              Qt::CaseSensitivity cs = Qt::CaseInsensitive);

// calls naturalCompare()
//
class QDLLEXPORT NaturalSort
{
public:
  NaturalSort(Qt::CaseSensitivity cs = Qt::CaseInsensitive) : m_cs(cs) {}

  bool operator()(const QString& a, const QString& b)
  {
    return (naturalCompare(a, b, m_cs) < 0);
  }

private:
  Qt::CaseSensitivity m_cs;
};

/**
 * throws on failure
 * @return absolute path of the desktop directory for the current user
 **/
QDLLEXPORT QString getDesktopDirectory();

/**
 * throws on failure
 * @return absolute path of the start menu directory for the current user
 **/
QDLLEXPORT QString getStartMenuDirectory();

/**
 * @brief read a file and return it's content as a unicode string. This tries to guess
 *        the encoding used in the file
 * @param fileName name of the file to read
 * @param encoding (optional) if this is set, the target variable received the name of
 *the encoding used
 * @return the textual content of the file or an empty string if the file doesn't exist
 **/
QDLLEXPORT QString readFileText(const QString& fileName, QString* encoding = nullptr);

/**
 * @brief decode raw text data. This tries to guess the encoding used in the file
 * @param fileData raw data to decode
 * @param encoding (optional) if this is set, the target variable received the name of
 *the encoding used
 * @return the textual content of the file or an empty string if the file doesn't exist
 **/
QDLLEXPORT QString decodeTextData(const QByteArray& fileData,
                                  QString* encoding = nullptr);

/**
 * @brief delete files matching a pattern
 * @param directory in which to delete files
 * @param pattern the name pattern files have to match
 * @param numToKeep the number of files to keep
 * @param sorting if numToKeep is not 0, the last numToKeep files according to this
 *sorting a kept
 **/
QDLLEXPORT void removeOldFiles(const QString& path, const QString& pattern,
                               int numToKeep, QDir::SortFlags sorting = QDir::Time);

/**
 * @brief retrieve the icon of an executable. Currently this always extracts the biggest
 *icon
 * @param absolute path to the executable
 * @return the icon
 **/
QDLLEXPORT QIcon iconForExecutable(const QString& filePath);

/**
 * @brief Retrieve the file version of the given executable.
 *
 * @param filepath Absolute path to the executable.
 *
 * @return the file version, or an empty string if the file
 *   version could not be retrieved.
 */
QDLLEXPORT QString getFileVersion(QString const& filepath);

/**
 * @brief Retrieve the product version of the given executable.
 *
 * @param filepath Absolute path to the executable.
 *
 * @return the file version, or an empty string if the product
 *   version could not be retrieved.
 */
QDLLEXPORT QString getProductVersion(QString const& program);

// removes and deletes all the children of the given widget
//
QDLLEXPORT void deleteChildWidgets(QWidget* w);

template <typename T>
bool isOneOf(const T& val, const std::initializer_list<T>& list)
{
  return std::find(list.begin(), list.end(), val) != list.end();
}

QDLLEXPORT std::string formatSystemMessage(int id);

QDLLEXPORT QString localizedByteSize(unsigned long long bytes);
QDLLEXPORT QString localizedByteSpeed(unsigned long long bytesPerSecond);

QDLLEXPORT QString localizedTimeRemaining(unsigned int msecs);

template <class F>
class Guard
{
public:
  Guard() : m_call(false) {}

  explicit Guard(F f) : m_f(f), m_call(true) {}

  Guard(Guard&& g) : m_f(std::move(g.m_f)) { g.m_call = false; }

  ~Guard()
  {
    if (m_call)
      m_f();
  }

  Guard& operator=(Guard&& g) noexcept
  {
    m_f      = std::move(g.m_f);
    g.m_call = false;
    return *this;
  }

  void kill() { m_call = false; }

  Guard(const Guard&)            = delete;
  Guard& operator=(const Guard&) = delete;

private:
  F m_f;
  bool m_call;
};

// remembers the time in the constructor, logs the time elapsed in the
// destructor
//
class QDLLEXPORT TimeThis
{
public:
  // calls start()
  //
  explicit TimeThis(const QString& what = {});

  // calls stop()
  //
  ~TimeThis();

  TimeThis(const TimeThis&)            = delete;
  TimeThis& operator=(const TimeThis&) = delete;

  // remembers the current time and the given string; if there is currently
  // a timing active, calls stop() to log it first
  //
  void start(const QString& what = {});

  // logs the time elapsed since start() in the form of "timing: what X ms";
  // no-op if start() wasn't called
  //
  void stop();

private:
  using Clock = std::chrono::high_resolution_clock;

  QString m_what;
  Clock::time_point m_start;
  bool m_running;
};

template <class F>
bool forEachLineInFile(const QString& filePath, F&& f)
{
  QFile file(filePath);
  file.open(QIODevice::ReadOnly | QIODevice::Text);
  if (!file.isOpen() || file.size() == 0 || !file.isReadable()) {
    return false;
  }

  MOBase::Guard g([&] {
    file.close();
  });

  while (!file.atEnd()) {
    QByteArray line = file.readLine();
    // skip empty lines
    if (line.isEmpty()) {
      continue;
    }
    // remove whitespaces from beginning and end of line
    line = line.trimmed();
    // skip comments
    if (line.startsWith('#')) {
      continue;
    }
    // skip line if it only had whitespaces
    if (line.isEmpty()) {
      continue;
    }
    f(QString::fromUtf8(line));
  }
  return true;
}
}  // namespace MOBase

#endif  // MO_UIBASE_UTILITY_INCLUDED
