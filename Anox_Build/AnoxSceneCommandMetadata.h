#pragma once

#include <stddef.h>

namespace anox::buildsystem
{
	enum class SceneCommandParamType
	{
		UInt,
		Float,
		Str,
		EntityID,
		EntityType,
		HexUInt,
		Label,
	};

	struct SceneCommandParamDef
	{
		const char *m_name;
		uint32_t m_nameLength;
		SceneCommandParamType m_paramType;
	};

	struct SceneCommandDef
	{
		const char *m_name;
		uint32_t m_nameLength;
		uint32_t m_paramDefsOffset;
		uint32_t m_paramCount;
		uint32_t m_numOptionalParameters;
		uint8_t m_delimiter;
		bool m_mayFail;
	};
}
