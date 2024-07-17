#pragma once

namespace MOBase
{

#if defined(UIBASE_EXPORT)
#define QDLLEXPORT __attribute__((visibility("default")))
#elif defined(_NODLL)
#define QDLLEXPORT
#else
#undef DLLEXPORT
#define QDLLEXPORT __attribute__((visibility("hidden")))
#endif

}  // namespace MOBase