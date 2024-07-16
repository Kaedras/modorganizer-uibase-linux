#ifndef TASKPROGRESSMANAGER_H
#define TASKPROGRESSMANAGER_H

#include "dllimport.h"
#include <QMutexLocker>
#include <QObject>
#include <QTime>
#include <QTimer>
#include <map>
#ifdef _WIN32
#include <Windows.h>
#include <shobjidl.h>
#else
#include <QtDBus/QtDBus>
#endif

namespace MOBase
{

class QDLLEXPORT TaskProgressManager : QObject
{

  Q_OBJECT

public:
  static TaskProgressManager& instance();

  void forgetMe(quint32 id);
  void updateProgress(quint32 id, qint64 value, qint64 max);

  quint32 getId();

#ifdef _WIN32
public slots:
  bool tryCreateTaskbar();
#endif

private:
  TaskProgressManager();
  void showProgress();

private:
  std::map<quint32, std::pair<QTime, qint64>> m_Percentages;
  QMutex m_Mutex;
  quint32 m_NextId;
  QTimer m_CreateTimer;
  int m_CreateTries;

#ifdef _WIN32
  HWND m_WinId;

  ITaskbarList3* m_Taskbar;
#else
  bool m_successful;
#endif
};

}  // namespace MOBase
#endif  // TASKPROGRESSMANAGER_H
