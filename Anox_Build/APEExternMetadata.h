#pragma once

#include <stddef.h>

namespace anox::buildsystem::ape_parse
{
	enum class ExternFieldType
	{
		Float,
		FloatExpr,
		FloatVar,
		UInt32,
		Int32,
		Str,
		StrVar,
		Obj,
		ObjVar,
		Bool,
		Label,
		Expr,
		ScriptResource,
		ImageResource,
		SoundResource,
		FontResource,
		MusicSegResource,
		ParticleResource,
		MusicResource,
		SceneResource,
		ModelResource,
	};

	struct ExternOpcodeArgMetadata
	{
		const char *m_name;
		size_t m_nameLength;
		ExternFieldType m_fieldType;
	};

	struct ExternOpcodeMetadata
	{
		const char *m_name;
		size_t m_nameLength;
		const ExternOpcodeArgMetadata *m_argMetadata;
		uint32_t m_argCount;
		uint16_t m_opcode;
		bool m_isSpecial;
		uint32_t m_numUnnamedParameters;
		uint32_t m_numRequiredParameters;
	};
}
