#pragma once

#ifdef _WIN32
#ifdef BUILD_AS_DLL
#ifdef CoreProfiler_EXPORTS
#define PROFILER_API __declspec(dllexport)
#else
#define PROFILER_API __declspec(dllimport)
#endif
#else
#define PROFILER_API
#endif
#else
#define PROFILER_API
#endif
