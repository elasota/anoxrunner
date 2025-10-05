#pragma once

namespace rkit
{
	namespace buildsystem
	{
		enum class BuildFileLocation
		{
			kInvalid,
			kSourceDir,
			kIntermediateDir,
			kOutputFiles,
			kOutputContent,

			kAuxDir0,
			kAuxDir10000 = kAuxDir0 + 10000,
		};
	}
}
