#include "rkit/Core/Endian.h"

namespace anox { namespace data
{
	struct CompressedQuat
	{
		rkit::endian::LittleUInt16_t m_x;
		rkit::endian::LittleUInt16_t m_y;
		rkit::endian::LittleUInt16_t m_z;
	};
} } // anox::data
