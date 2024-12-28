#pragma once


#pragma push_macro("WIN32_LEAN_AND_MEAN")
#pragma push_macro("NOMINMAX")
#pragma push_macro("STRICT")

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>

#undef GetCommandLine
#undef CreateMutex
#undef GetFileAttributes
#undef CreateEvent

#pragma pop_macro("WIN32_LEAN_AND_MEAN")
#pragma pop_macro("NOMINMAX")
#pragma pop_macro("STRICT")
