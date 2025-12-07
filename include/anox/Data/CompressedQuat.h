#include "rkit/Core/Endian.h"

#include <math.h>

namespace anox { namespace data
{
	struct CompressedQuat
	{

		rkit::endian::LittleInt16_t m_x;
		rkit::endian::LittleInt16_t m_y;
		rkit::endian::LittleInt16_t m_z;
	};

	inline CompressedQuat CompressNormalizedQuat(float x, float y, float z)
	{
		float axisValues[3] = { x, y, z };
		int16_t int16Values[3] = {};

		for (int axis = 0; axis < 3; axis++)
		{
			float axisValue = axisValues[axis];

			if (!(axisValue >= -1.f))
				axisValue = -1.f;
			else if (!(axisValue <= 1.f))
				axisValue = 1.f;

			int16Values[axis] = static_cast<int16_t>(floorf(axisValue * 32767.0f + 0.5f));
		}

		CompressedQuat result;
		result.m_x = int16Values[0];
		result.m_y = int16Values[1];
		result.m_z = int16Values[2];

		return result;
	}

	inline void DecompressQuat(const CompressedQuat &compressed, float &outX, float &outY, float &outZ, float &outW)
	{
		const int16_t int16Values[3] = { compressed.m_x.Get(), compressed.m_y.Get(), compressed.m_z.Get() };
		float floatValues[3];
		const float rcp32767 = 0.000030518509475997192297128208258309f;

		for (int axis = 0; axis < 3; axis++)
			floatValues[axis] = static_cast<float>(int16Values[axis] * rcp32767);

		float sqLen = floatValues[0] * floatValues[0] + floatValues[1] * floatValues[1] + floatValues[2] * floatValues[2];
		if (sqLen > 1.f)
			sqLen = 1.f;

		outX = floatValues[0];
		outY = floatValues[1];
		outZ = floatValues[2];
		outW = sqrtf(1.0f - sqLen);
	}
} } // anox::data
