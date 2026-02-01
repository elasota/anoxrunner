#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Win32/IncludeWindows.h"
#include "rkit/Core/Timestamp.h"

namespace rkit
{
	template<class T>
	class Vector;

	class ConvUtil_Win32
	{
	public:
		static Result UTF8ToUTF16(const Utf8Char_t *str8, Vector<wchar_t> &outStr16);
		static Result UTF16ToUTF8(const wchar_t *str16, Vector<Utf8Char_t> &outStr16);

		static UTCMSecTimestamp_t FileTimeToUTCMSec(const FILETIME &ftime);
		static FILETIME UTCMSecToFileTime(UTCMSecTimestamp_t timestamp);

		static const uint64_t kUnixEpochStartFT = 116444736000000000ULL;
	};
}
