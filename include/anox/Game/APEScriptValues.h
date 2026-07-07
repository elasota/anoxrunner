#pragma once

#include "anox/Data/APEScript.h"

#include <stdint.h>

namespace rkit
{
	template<class T>
	class Span;
}

namespace anox::game
{
	struct ScriptExprValue;

	using ScriptExprType = data::ape::ExprType;
	using ScriptOperator = data::ape::Operator;
	using ScriptOperandType = data::ape::OperandType;
	using ScriptOperandList = rkit::Span<ScriptExprValue>;
	using ScriptResourceReferenceType = data::ape::ResourceReferenceType;

	struct ScriptExprValue
	{
		ScriptExprType m_exprType = ScriptExprType::Empty;
		uint32_t m_index = 0;
	};

	struct ScriptResourceReference
	{
		ScriptResourceReferenceType m_refType = ScriptResourceReferenceType::Invalid;
		uint32_t m_index = 0;
	};
}
