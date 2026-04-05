#pragma once

#include "rkit/Core/Endian.h"

namespace anox::data::ape
{
	enum class Operator : uint8_t
	{
		Invalid = 0,
		Or = 1,
		And = 2,
		Xor = 3,
		Gt = 4,
		Lt = 5,
		Ge = 6,
		Le = 7,
		Eq = 8,
		Add = 9,
		Sub = 10,
		Mul = 11,
		Div = 12,
		Neq = 13,

		StrEq = 14,
		StrNeq = 15,
	};

	enum class OperandType : uint8_t
	{
		Invalid,
		Expression,
		Literal,
		Variable,
	};

	struct Expression
	{
		uint8_t m_packedOperandInfo;
		rkit::endian::LittleUInt32_t m_leftValue;
		rkit::endian::LittleUInt32_t m_rightValue;

		static uint8_t PackOperandInfo(Operator op, OperandType leftOperandType, OperandType rightOperandType);
		static void UnpackOperandInfo(uint8_t packed, Operator &outOp, OperandType &outLeftOperandType, OperandType &outRightOperandType);
	};

	enum class ExprType : uint8_t
	{
		Empty,
		FloatExpression,
		FloatLiteral,
		StringLiteral,
		FloatVariable,
		StringVariable,
	};

	struct ExpressionValue
	{
		ExprType m_exprType;
		rkit::endian::LittleUInt32_t m_index;
	};
}

namespace anox::data::ape
{
	uint8_t Expression::PackOperandInfo(Operator op, OperandType leftOperandType, OperandType rightOperandType)
	{
		return static_cast<uint8_t>(op) | (static_cast<uint8_t>(leftOperandType) << 4) | (static_cast<uint8_t>(rightOperandType) << 6);
	}

	void Expression::UnpackOperandInfo(uint8_t packed, Operator &outOp, OperandType &outLeftOperandType, OperandType &outRightOperandType)
	{
		outOp = static_cast<Operator>(packed & 0xf);
		outLeftOperandType = static_cast<OperandType>((packed >> 4) & 0x3);
		outRightOperandType = static_cast<OperandType>((packed >> 6) & 0x3);
	}
}
