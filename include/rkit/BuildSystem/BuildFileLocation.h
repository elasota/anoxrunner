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
		};
	}
}
