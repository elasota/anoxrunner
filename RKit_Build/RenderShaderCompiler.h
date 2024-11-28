#pragma once

#include "rkit/BuildSystem/BuildFileLocation.h"
#include "rkit/Core/String.h"

namespace rkit::buildsystem::rsc_interchange
{
	enum class ShaderStage
	{
		Vertex,
		Pixel,

		Count,
	};

	struct ShaderCompileJobDef
	{
		ShaderStage m_stage = ShaderStage::Pixel;
		BuildFileLocation m_location = BuildFileLocation::kSourceDir;
		String m_path;
		String m_entryPoint;
	};
}
