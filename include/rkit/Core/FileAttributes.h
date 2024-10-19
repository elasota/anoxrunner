#pragma once

#include "StreamProtos.h"
#include "Timestamp.h"

namespace rkit
{
	struct FileAttributes
	{
		UTCMSecTimestamp_t m_fileTime = 0;
		FilePos_t m_fileSize = 0;
		bool m_isDirectory = false;
	};
}
