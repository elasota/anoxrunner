#pragma once

#include <stdint.h>

#include "rkit/Core/Result.h"
#include "rkit/Core/StringProto.h"

#include "APEScriptValues.h"

namespace rkit
{
	template<class T>
	class Optional;
}

namespace rkit::data
{
	struct ContentID;
}

namespace anox::game
{
	class APEWindowCommandParser
	{
	public:
		static void ParseStatic(const uint8_t *&byteStream, uint32_t &value);
		static void ParseStatic(const uint8_t *&byteStream, uint16_t &value);
		static void ParseStatic(const uint8_t *&byteStream, ScriptExprValue &value);
		static void ParseStatic(const uint8_t *&byteStream, ScriptMaterialReference &value);

		rkit::Result ParseInstanced(const uint8_t *&byteStream, rkit::ByteStringView &value);
		rkit::Result ParseInstanced(const uint8_t *&byteStream, rkit::Optional<rkit::ByteStringView> &value);
		rkit::Result ParseInstanced(const uint8_t *&byteStream, ScriptOperandList &value);

	protected:
		virtual rkit::Result ParseString(uint32_t index, rkit::ByteStringView &value) = 0;
		virtual rkit::Result ParseOperandList(uint32_t index, ScriptOperandList &value) = 0;
	};
}

#include "rkit/Core/Endian.h"
#include "rkit/Core/Optional.h"

#include "anox/Data/APEScript.h"

namespace anox::game
{
	inline void APEWindowCommandParser::ParseStatic(const uint8_t *&byteStream, uint32_t &value)
	{
		value = reinterpret_cast<const rkit::endian::LittleUInt32_t *>(byteStream)->Get();
		byteStream += 4;
	}

	inline void APEWindowCommandParser::ParseStatic(const uint8_t *&byteStream, uint16_t &value)
	{
		value = reinterpret_cast<const rkit::endian::LittleUInt16_t *>(byteStream)->Get();
		byteStream += 2;
	}

	inline void APEWindowCommandParser::ParseStatic(const uint8_t *&byteStream, ScriptExprValue &value)
	{
		const data::ape::ExpressionValue &exprValue = *reinterpret_cast<const data::ape::ExpressionValue *>(byteStream);
		byteStream += sizeof(data::ape::ExpressionValue);

		value.m_exprType = exprValue.m_exprType;
		value.m_index = exprValue.m_index.Get();
	}

	inline void APEWindowCommandParser::ParseStatic(const uint8_t *&byteStream, ScriptMaterialReference &value)
	{
		const data::ape::MaterialReference &exprValue = *reinterpret_cast<const data::ape::MaterialReference *>(byteStream);
		byteStream += sizeof(data::ape::MaterialReference);

		value.m_refType = exprValue.m_refType;
		value.m_index = exprValue.m_index.Get();
	}

	inline rkit::Result APEWindowCommandParser::ParseInstanced(const uint8_t *&byteStream, rkit::ByteStringView &value)
	{
		uint32_t stringIndex = 0;
		ParseStatic(byteStream, stringIndex);

		return ParseString(stringIndex, value);
	}

	inline rkit::Result APEWindowCommandParser::ParseInstanced(const uint8_t *&byteStream, rkit::Optional<rkit::ByteStringView> &value)
	{
		uint32_t stringIndex = 0;
		ParseStatic(byteStream, stringIndex);

		if (stringIndex == 0)
		{
			value.Reset();
			RKIT_RETURN_OK;
		}
		else
		{
			value = rkit::ByteStringView();
			return ParseString(stringIndex - 1, value.Get());
		}
	}

	inline rkit::Result APEWindowCommandParser::ParseInstanced(const uint8_t *&byteStream, ScriptOperandList &value)
	{
		uint32_t opListIndex = 0;
		ParseStatic(byteStream, opListIndex);

		return ParseOperandList(opListIndex, value);
	}
}
