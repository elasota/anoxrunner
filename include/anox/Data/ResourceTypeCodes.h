#pragma once

#include "rkit/Core/FourCC.h"

namespace anox::resloaders
{
	static const uint32_t kRawFileResourceTypeCode = RKIT_FOURCC('F', 'I', 'L', 'E');
	static const uint32_t kBSPModelResourceTypeCode = RKIT_FOURCC('B', 'S', 'P', 'M');
	static const uint32_t kSpawnDefsResourceTypeCode = RKIT_FOURCC('S', 'P', 'W', 'N');
	static const uint32_t kWorldMaterialTypeCode = RKIT_FOURCC('M', 'T', 'L', 'W');
	static const uint32_t kModelMaterialTypeCode = RKIT_FOURCC('M', 'T', 'L', 'M');
	static const uint32_t kTextureResourceTypeCode = RKIT_FOURCC('T', 'X', 'T', 'R');
	static const uint32_t kEntityDefTypeCode = RKIT_FOURCC('E', 'D', 'E', 'F');
	static const uint32_t kMDAModelResourceTypeCode = RKIT_FOURCC('M', 'D', 'A', 'M');
}
