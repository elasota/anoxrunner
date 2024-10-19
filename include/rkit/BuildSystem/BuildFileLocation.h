#pragma once

namespace rkit::buildsystem
{
	enum class BuildFileLocation
	{
		kInvalid,
		kSourceDir,
		kIntermediateDir,
		kOutputDir,
	};
}
