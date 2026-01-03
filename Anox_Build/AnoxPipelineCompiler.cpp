#pragma once

#include "rkit/Core/FourCC.h"
#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/Core/PathProto.h"
#include "rkit/Core/StringProto.h"


namespace rkit
{
	template<class T>
	class Span;
}

namespace anox { namespace buildsystem {
	static const uint32_t kMaterialPipelineKeyCompiler = RKIT_FOURCC('E', 'D', 'E', 'F');

	class MaterialPipelineCompilerBase : public rkit::buildsystem::IDependencyNodeCompiler
	{
	public:
		static rkit::Result Create(rkit::UniquePtr<MaterialPipelineCompilerBase> &outCompiler);
	};
} } // anox::buildsystem
