#include "AnoxAPEParser.h"

#include "rkit/Core/Endian.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/String.h"

namespace anox::buildsystem::ape_parse
{
	APEReader::APEReader(rkit::IReadStream &stream)
		: m_stream(stream)
	{
	}

	rkit::Result APEReader::Read(float &value)
	{
		rkit::endian::LittleFloat32_t temp;
		RKIT_CHECK(m_stream.ReadOneBinary(temp));
		value = temp.Get();
		RKIT_RETURN_OK;
	}

	rkit::Result APEReader::Read(uint8_t &value)
	{
		return m_stream.ReadOneBinary(value);
	}

	rkit::Result APEReader::Read(uint16_t &value)
	{
		rkit::endian::LittleUInt16_t temp;
		RKIT_CHECK(m_stream.ReadOneBinary(temp));
		value = temp.Get();
		RKIT_RETURN_OK;
	}

	rkit::Result APEReader::Read(uint32_t &value)
	{
		rkit::endian::LittleUInt32_t temp;
		RKIT_CHECK(m_stream.ReadOneBinary(temp));
		value = temp.Get();
		RKIT_RETURN_OK;
	}

	rkit::Result APEReader::Read(uint64_t &value)
	{
		rkit::endian::LittleUInt64_t temp;
		RKIT_CHECK(m_stream.ReadOneBinary(temp));
		value = temp.Get();
		RKIT_RETURN_OK;
	}

	rkit::Result APEReader::ReadBits(uint32_t &value, uint32_t allowedMask)
	{
		RKIT_CHECK(Read(value));
		if ((value & allowedMask) != value)
			RKIT_THROW(rkit::ResultCode::kDataError);
		RKIT_RETURN_OK;
	}

	rkit::Result APEReader::Read(rkit::Optional<ExpressionValue> &value)
	{
		rkit::endian::LittleUInt64_t exprFlag;
		RKIT_CHECK(m_stream.ReadOneBinary(exprFlag));

		if (exprFlag.Get() == 0)
			value.Reset();
		else if (exprFlag.Get() == 1)
		{
			ExpressionValue expr;
			RKIT_CHECK(expr.Read(*this));

			rkit::endian::LittleUInt64_t zeroCheck;
			RKIT_CHECK(m_stream.ReadOneBinary(zeroCheck));
			if (zeroCheck.Get() != 0)
				RKIT_THROW(rkit::ResultCode::kDataError);

			value = std::move(expr);
		}
		else
			RKIT_THROW(rkit::ResultCode::kDataError);

		RKIT_RETURN_OK;
	}

	rkit::Result APEReader::Read(rkit::Optional<rkit::ByteString> &value)
	{
		uint32_t length = 0;
		RKIT_CHECK(Read(length));

		if (length == 0)
			value.Reset();
		else
		{
			rkit::ByteStringConstructionBuffer cbuf;
			RKIT_CHECK(cbuf.Allocate(length - 1));

			RKIT_CHECK(m_stream.ReadAllSpan(cbuf.GetSpan()));

			uint8_t terminator = 0;
			RKIT_CHECK(m_stream.ReadOneBinary(terminator));
			if (terminator != 0)
				RKIT_THROW(rkit::ResultCode::kDataError);

			value = rkit::ByteString(std::move(cbuf));
		}

		RKIT_RETURN_OK;
	}

	rkit::Result APEReader::Read(rkit::ByteString &value)
	{
		rkit::Optional<rkit::ByteString> bstr;
		RKIT_CHECK(Read(bstr));

		if (!bstr.IsSet())
			RKIT_THROW(rkit::ResultCode::kDataError);

		value = bstr.Get();

		RKIT_RETURN_OK;
	}

	rkit::Result APEReader::Read(FormattingValue &value)
	{
		uint8_t doneByte = 0;
		for (;;)
		{
			RKIT_CHECK(m_stream.ReadOneBinary(doneByte));

			if (doneByte != 0)
				break;

			uint8_t typeByte = 0;
			RKIT_CHECK(m_stream.ReadOneBinary(typeByte));

			rkit::UniquePtr<Operand> operand;
			RKIT_CHECK(Operand::CreateFromType(operand, typeByte));

			RKIT_CHECK(operand->Read(*this));
		}

		uint8_t extraByte = 0;
		RKIT_CHECK(m_stream.ReadOneBinary(extraByte));

		if (doneByte != 0xff || extraByte != 0xff)
			RKIT_THROW(rkit::ResultCode::kDataError);

		RKIT_RETURN_OK;
	}

	rkit::Result APEReader::SkipPadding(size_t count)
	{
		rkit::endian::LittleUInt64_t padding;
		padding = static_cast<uint64_t>(0);

		while (count > 0)
		{
			const size_t sizeToRead = rkit::Min<size_t>(count, sizeof(padding));
			RKIT_CHECK(m_stream.ReadAll(&padding, sizeToRead));

			if (padding.Get() != 0)
				RKIT_THROW(rkit::ResultCode::kDataError);

			count -= sizeToRead;
		}

		RKIT_RETURN_OK;
	}

	rkit::Result ExpressionValue::Read(APEReader &reader)
	{
		uint16_t operatorAndFlags = 0;
		RKIT_CHECK(reader.Read(operatorAndFlags));

		const uint8_t flags = ((operatorAndFlags >> 8) & 0xff);
		const uint8_t op = (operatorAndFlags & 0xff);

		if (op == 0 || op > static_cast<uint8_t>(ExpressionValue::Operator::Count))
			RKIT_THROW(rkit::ResultCode::kDataError);

		m_operator = static_cast<Operator>(op);

		rkit::UniquePtr<Operand> *const operandPtrs[2] =
		{
			&m_left,
			&m_right
		};

		for (int operandIndex = 0; operandIndex < 2; operandIndex++)
		{
			const uint8_t operandType = ((flags >> operandIndex) & 0x15u);

			rkit::UniquePtr<Operand> operand;
			RKIT_CHECK(Operand::CreateFromType(operand, operandType));

			uint32_t prefix1 = 0;
			uint32_t prefix2 = 0;
			RKIT_CHECK(reader.Read(prefix1));
			RKIT_CHECK(reader.Read(prefix2));
			RKIT_CHECK(operand->Read(reader));

			*(operandPtrs[operandIndex]) = std::move(operand);
		}

		RKIT_RETURN_OK;
	}

	rkit::Result Operand::CreateFromType(rkit::UniquePtr<Operand> &outOperand, uint8_t typeByte)
	{
		switch (typeByte)
		{
		case 0:
			return rkit::New<ExpressionOperand>(outOperand);
		case 4:
			return rkit::New<FloatOperand>(outOperand);
		case 5:
			return rkit::New<FloatVariableNameOperand>(outOperand);
		case 16:
			return rkit::New<StringOperand>(outOperand);
		case 17:
			return rkit::New<StringVariableNameOperand>(outOperand);
		default:
			RKIT_THROW(rkit::ResultCode::kDataError);
		}
	}

	rkit::Result FloatOperand::Read(APEReader &reader)
	{
		return reader.Read(m_value);
	}

	rkit::Result FloatVariableNameOperand::Read(APEReader &reader)
	{
		return reader.Read(m_value);
	}

	rkit::Result StringOperand::Read(APEReader &reader)
	{
		return reader.Read(m_value);
	}

	rkit::Result StringVariableNameOperand::Read(APEReader &reader)
	{
		return reader.Read(m_value);
	}

	rkit::Result ExpressionOperand::Read(APEReader &reader)
	{
		return m_value.Read(reader);
	}
}
