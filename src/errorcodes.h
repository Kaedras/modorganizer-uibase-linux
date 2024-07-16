#ifndef UIBASE_ERRORCODES_H
#define UIBASE_ERRORCODES_H

#include "dllimport.h"
#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32

namespace MOBase
{
#ifdef _WIN32
QDLLEXPORT const wchar_t* errorCodeName(DWORD code);
#endif
QDLLEXPORT const char* errorCodeName(int code);

}  // namespace MOBase
#endif // UIBASE_ERRORCODES_H
