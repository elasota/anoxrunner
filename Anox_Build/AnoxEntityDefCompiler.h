#pragma once

#include "rkit/Core/FourCC.h"
#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/Core/PathProto.h"
#include "rkit/Core/StringProto.h"

#include "anox/Data/MaterialResourceType.h"

namespace rkit
{
	template<class T>
	class Span;
}

namespace anox { namespace buildsystem
{
	static const uint32_t kEntityDefNodeID = RKIT_FOURCC('E', 'D', 'E', 'F');

	class EntityDefCompilerBase : public rkit::buildsystem::IDependencyNodeCompiler
	{
	public:
		static rkit::StringView GetEDefFilePath();

		static rkit::Result Create(rkit::UniquePtr<EntityDefCompilerBase> &outCompiler);
	};
} } // anox::buildsystem
