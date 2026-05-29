#pragma once

#include "rkit/Core/FourCC.h"

namespace anox::buildsystem
{
	static const uint32_t kAPEDepsNodeID = RKIT_FOURCC('A', 'P', 'E', 'D');
	static const uint32_t kAPEScriptNodeID = RKIT_FOURCC('A', 'P', 'E', 'S');
	static const uint32_t kAPEGroupNodeID = RKIT_FOURCC('A', 'P', 'E', 'G');

	static const uint32_t kFontMaterialNodeID = RKIT_FOURCC('F', 'M', 'T', 'L');
	static const uint32_t kWorldMaterialNodeID = RKIT_FOURCC('W', 'M', 'T', 'L');
	static const uint32_t kModelMaterialNodeID = RKIT_FOURCC('M', 'M', 'T', 'L');
	static const uint32_t kInterfaceMaterialNodeID = RKIT_FOURCC('I', 'M', 'T', 'L');
}
