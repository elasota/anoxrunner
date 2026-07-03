#pragma once

#include "rkit/BuildSystem/DependencyGraph.h"

namespace anox::buildsystem
{
	class SceneCompilerBase : public rkit::buildsystem::IDependencyNodeCompiler
	{
	public:
		static rkit::Result Create(rkit::UniquePtr<SceneCompilerBase> &outCompiler);
	};
}
