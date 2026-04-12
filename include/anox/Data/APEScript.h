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
		ExprType m_exprType = ExprType::Empty;
		rkit::endian::LittleUInt32_t m_index;
	};

	struct Window
	{
		rkit::endian::LittleUInt32_t m_windowID;
		rkit::endian::LittleUInt32_t m_commandStreamLength;
	};

	struct Switch
	{
		rkit::endian::LittleUInt32_t m_switchID;
		rkit::endian::LittleUInt32_t m_numCommands;
	};

	struct SwitchCommand
	{
		uint8_t m_opcode = 0;
		rkit::endian::LittleUInt32_t m_fmtValue;
		rkit::endian::LittleUInt32_t m_strValue;	// Also instr jump count for if/jump instrs
		data::ape::ExpressionValue m_exprValue;
	};

	struct APEScriptCatalog
	{
		rkit::endian::LittleUInt32_t m_numStrings;
		rkit::endian::LittleUInt32_t m_numOperandLists;
		rkit::endian::LittleUInt32_t m_numWindows;
		rkit::endian::LittleUInt32_t m_numSwitches;

		// uint32_t m_stringLengths[m_numStrings]
		// uint32_t m_stringChars[m_numStrings][m_stringLengths[i]]
		// uint32_t m_operandListCounts[m_numOperandLists]
		// uint32_t m_operands[m_numOperandLists][m_operandListCounts[i]]
		// Window m_windows[m_numWindows]
		// uint8_t m_windowCommandStreams[m_numWindows][window.m_commandStreamLength]
		// Switch m_switches[m_numSwitches]
		// SwitchCommand m_switchCommands[m_numSwitches][switch.m_numCommands]
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
