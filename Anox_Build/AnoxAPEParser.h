#pragma once

#include "rkit/Core/Optional.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/StringProto.h"
#include "rkit/Core/String.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"

#include "anox/Data/APEWindowCommandOpcodes.generated.h"

namespace rkit
{
	template<class T>
	class Optional;
}

namespace anox::buildsystem::ape_parse
{
	struct IAPEWriter;
	class APEReader;
	struct Operand;

	struct ExpressionValue
	{
		enum class Operator
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

			Count = 14,
		};

		Operator m_operator = Operator::Invalid;
		rkit::UniquePtr<Operand> m_left;
		rkit::UniquePtr<Operand> m_right;

		rkit::Result Read(APEReader &reader);
	};

	enum class OperandType
	{
		FloatLiteral,
		FloatVariable,
		StringLiteral,
		StringVariable,
		Expression,
	};

	struct Operand
	{
		virtual ~Operand() {}

		virtual rkit::Result Read(APEReader &reader) = 0;
		virtual OperandType GetOperandType() const = 0;

		static rkit::Result CreateFromType(rkit::UniquePtr<Operand> &outOperand, uint8_t typeByte);
	};

	struct FloatOperand : public Operand
	{
		float m_value = 0;

		OperandType GetOperandType() const override { return OperandType::FloatLiteral; }
		rkit::Result Read(APEReader &reader) override;
	};

	struct FloatVariableNameOperand : public Operand
	{
		rkit::ByteString m_value;

		OperandType GetOperandType() const override { return OperandType::FloatVariable; }
		rkit::Result Read(APEReader &reader) override;
	};

	struct StringOperand : public Operand
	{
		rkit::ByteString m_value;

		OperandType GetOperandType() const override { return OperandType::StringLiteral; }
		rkit::Result Read(APEReader &reader) override;
	};

	struct ExpressionOperand : public Operand
	{
		ExpressionValue m_value;

		OperandType GetOperandType() const override { return OperandType::Expression; }
		rkit::Result Read(APEReader &reader) override;
	};

	struct StringVariableNameOperand : public Operand
	{
		rkit::ByteString m_value;

		OperandType GetOperandType() const override { return OperandType::StringVariable; }
		rkit::Result Read(APEReader &reader) override;
	};

	struct FormattingValue
	{
		rkit::Vector<rkit::UniquePtr<Operand>> m_operands;
	};

	struct SwitchCommand
	{
		uint64_t m_cc = 0;
		uint8_t m_opcode = 0;
		FormattingValue m_fmt;
		rkit::Optional<rkit::ByteString> m_str;
		rkit::Optional<ExpressionValue> m_expr;
	};

	struct WindowCommand
	{
		virtual ~WindowCommand() {}

		virtual rkit::Result Parse(APEReader &reader) = 0;
		virtual rkit::Result Write(IAPEWriter &writer) const = 0;
		virtual ::anox::data::WindowCommandType GetCommandType() const = 0;
	};

	struct IAPEWriter
	{
		virtual rkit::Result Write(float value) = 0;
		virtual rkit::Result Write(uint8_t value) = 0;
		virtual rkit::Result Write(uint16_t value) = 0;
		virtual rkit::Result Write(uint32_t value) = 0;
		virtual rkit::Result Write(uint64_t value) = 0;
		virtual rkit::Result Write(const rkit::Optional<ExpressionValue> &value) = 0;
		virtual rkit::Result Write(const rkit::Optional<rkit::ByteString> &value) = 0;
		virtual rkit::Result Write(const rkit::ByteString &value) = 0;
		virtual rkit::Result Write(const FormattingValue &value) = 0;
	};

	class APEReader
	{
	public:
		explicit APEReader(rkit::IReadStream &stream);

		rkit::Result Read(float &value);
		rkit::Result Read(uint8_t &value);
		rkit::Result Read(uint16_t &value);
		rkit::Result Read(uint32_t &value);
		rkit::Result Read(uint64_t &value);
		rkit::Result ReadBits(uint32_t &value, uint32_t allowedMask);
		rkit::Result Read(rkit::Optional<ExpressionValue> &value);
		rkit::Result Read(rkit::Optional<rkit::ByteString> &value);
		rkit::Result Read(rkit::ByteString &value);
		rkit::Result Read(FormattingValue &value);
		rkit::Result SkipPadding(size_t count);

	private:
		rkit::IReadStream &m_stream;
	};
}
