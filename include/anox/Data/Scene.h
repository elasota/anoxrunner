#pragma once

#include "rkit/Core/FourCC.h"
#include "rkit/Core/Endian.h"

namespace anox::data
{
	enum class ScenePathType : uint8_t
	{
		kCubic,
		kFocus,
		kCommand,
		kScale,
		kRoll,
		kFOV,

		kCount,
	};

	struct SceneHeader
	{
		static constexpr uint32_t kExpectedMagic = RKIT_FOURCC('S', 'C', 'E', 'N');

		rkit::endian::BigUInt32_t m_magic;
		rkit::endian::LittleUInt32_t m_cineID;
		uint8_t m_isInterrupt;
		rkit::endian::LittleUInt32_t m_numStrings;
		rkit::endian::LittleUInt32_t m_numBlocks;
		rkit::endian::LittleUInt32_t m_numPaths;
		rkit::endian::LittleUInt32_t m_nodeCounts[static_cast<size_t>(ScenePathType::kCount)];

		// rkit::endian::LittleUInt32_t m_stringLengths[m_numStrings]
		// uint8_t m_stringChars[sum(m_stringLengths)]
		// data::SceneBlock m_blocks[m_numBlocks]
		// data::ScenePath m_paths[m_numPaths]
		// data::SceneNodeCommon m_common[sum(m_nodeCounts)]
		// data::SceneCubicNode m_cubic[m_nodeCounts[ScenePathType::kCubic]]
		// data::SceneFocusNode m_focus[m_nodeCounts[ScenePathType::kFocus]]
		// data::SceneCommandNode m_cmd[m_nodeCounts[ScenePathType::kCommand]]
		// data::SceneScaleNode m_scale[m_nodeCounts[ScenePathType::kScale]]
		// data::SceneRollNode m_roll[m_nodeCounts[ScenePathType::kRoll]]
		// data::SceneFOVNode m_scale[m_nodeCounts[ScenePathType::kFOV]]
		// data::SceneCommandOpcode m_cmdOpcodes[sum(m_cmd[...].m_numCommands)]
		// rkit::endian::LittleUInt32_t m_cmdParamDWords[sum(m_cmd[...].m_numParamDWords)]
	};

	struct SceneNodeCommon
	{
		rkit::endian::LittleUInt32_t m_flags;
		rkit::endian::LittleUInt32_t m_timeLen;
	};

	struct SceneCubicNode
	{
		rkit::endian::LittleFloat32_t m_position[3];
		rkit::endian::LittleFloat32_t m_velocity[3];
		rkit::endian::LittleUInt32_t m_relativeMode;
	};

	struct SceneFocusNode
	{
		rkit::endian::LittleUInt32_t m_focusTarget;
	};

	struct SceneCommandNode
	{
		rkit::endian::LittleUInt32_t m_numCommands;
		rkit::endian::LittleUInt32_t m_numParamDWords;
	};

	struct SceneScaleNode
	{
		rkit::endian::LittleFloat32_t m_scale[3];
		rkit::endian::LittleFloat32_t m_delta[3];
	};

	struct SceneRollNode
	{
		rkit::endian::LittleFloat32_t m_value;
		rkit::endian::LittleFloat32_t m_rate;
	};

	struct SceneFOVNode
	{
		rkit::endian::LittleFloat32_t m_value;
		rkit::endian::LittleFloat32_t m_rate;
	};

	struct ScenePath
	{
		ScenePathType m_pathType;
		rkit::endian::LittleUInt32_t m_numNodes;
		rkit::endian::LittleUInt32_t m_timeOffs;
		rkit::endian::LittleUInt32_t m_maxLen;
	};

	struct SceneBlock
	{
		rkit::endian::LittleUInt32_t m_numPaths;
		rkit::endian::LittleUInt32_t m_flags;
	};
}
