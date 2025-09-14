#pragma once

#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/Core/FourCC.h"
	
namespace anox { namespace buildsystem
{
	static const uint32_t kMD2ModelNodeID = RKIT_FOURCC('A', 'M', 'D', '2');
	static const uint32_t kMDAModelNodeID = RKIT_FOURCC('A', 'M', 'D', 'A');

	class AnoxMDACompilerBase : public rkit::buildsystem::IDependencyNodeCompiler
	{
	public:
		static rkit::Result Create(rkit::UniquePtr<AnoxMDACompilerBase> &outCompiler);
	};

	class AnoxMD2CompilerBase : public rkit::buildsystem::IDependencyNodeCompiler
	{
	public:
		static rkit::Result Create(rkit::UniquePtr<AnoxMD2CompilerBase> &outCompiler);
	};
} }
