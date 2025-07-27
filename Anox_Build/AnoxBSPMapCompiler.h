#pragma once

#include "rkit/Core/FourCC.h"
#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/Core/PathProto.h"

namespace anox { namespace buildsystem
{
	static const uint32_t kBSPMapNodeID = RKIT_FOURCC('B', 'S', 'P', 'M');

	struct IDependencyNodeCompilerFeedback;

	class BSPMapCompilerBase : public rkit::buildsystem::IDependencyNodeCompiler
	{
	public:
		static rkit::Result Create(rkit::UniquePtr<BSPMapCompilerBase> &outCompiler);
	};
} } // anox::buildsystem
