#include "rkit/Core/Endian.h"

#include <stdint.h>

namespace anox
{
	namespace afs
	{
		struct HeaderData
		{
			static const uint32_t kAFSMagic = 0x54414441;
			static const uint32_t kAFSVersion = 9;

			rkit::endian::LittleUInt32_t m_magic;
			rkit::endian::LittleUInt32_t m_catalogLocation;
			rkit::endian::LittleUInt32_t m_catalogSize;
			rkit::endian::LittleUInt32_t m_version;
		};

		struct FileData
		{
			static const size_t kFileNameBufferSize = 128;

			char m_filePath[kFileNameBufferSize];
			rkit::endian::LittleUInt32_t m_location;
			rkit::endian::LittleUInt32_t m_uncompressedSize;
			rkit::endian::LittleUInt32_t m_compressedSize;
			rkit::endian::LittleUInt32_t m_checksum;
		};
	}
}
