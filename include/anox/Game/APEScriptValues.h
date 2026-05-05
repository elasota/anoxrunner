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
	using ScriptExprType = data::ape::ExprType;
	using ScriptOperator = data::ape::Operator;
	using ScriptOperandType = data::ape::OperandType;
	using ScriptOperandList = rkit::Span<const uint32_t>;
	using ScriptMaterialReferenceType = data::ape::MaterialReferenceType;

	struct ScriptExprValue
	{
		ScriptExprType m_exprType = ScriptExprType::Empty;
		uint32_t m_index = 0;
	};

	struct ScriptMaterialReference
	{
		ScriptMaterialReferenceType m_refType = ScriptMaterialReferenceType::Invalid;
		uint32_t m_index = 0;
	};
}
