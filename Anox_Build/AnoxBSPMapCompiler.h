#pragma once

#include "rkit/Core/FourCC.h"
#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/Core/PathProto.h"

namespace anox { namespace buildsystem
{
	static const uint32_t kBSPMapNodeID = RKIT_FOURCC('B', 'S', 'P', 'M');
	static const uint32_t kBSPLightmapNodeID = RKIT_FOURCC('B', 'S', 'P', 'L');
	static const uint32_t kBSPGeometryID = RKIT_FOURCC('B', 'S', 'P', 'G');

	struct IDependencyNodeCompilerFeedback;

	class BSPMapCompilerBase : public rkit::buildsystem::IDependencyNodeCompiler
	{
	public:
		static rkit::Result CreateMapCompiler(rkit::UniquePtr<BSPMapCompilerBase> &outCompiler);
		static rkit::Result CreateLightingCompiler(rkit::UniquePtr<BSPMapCompilerBase> &outCompiler);
		static rkit::Result CreateGeometryCompiler(rkit::UniquePtr<BSPMapCompilerBase> &outCompiler);
	};
} } // anox::buildsystem
