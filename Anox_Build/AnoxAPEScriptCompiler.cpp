#include "AnoxAPEScriptCompiler.h"
#include "AnoxAPEWindowCommandData.generated.h"

#include "anox/Data/APEScript.h"

#include "rkit/Core/HashTable.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/NoCopy.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/LogDriver.h"

namespace anox::buildsystem
{
	class APECompilerContext;

	class APECompilerContext
	{
	public:
		rkit::Result IndexExpression(uint32_t &outIndex, data::ape::Expression &&expr);
		rkit::Result IndexOperandList(uint32_t &outIndex, rkit::Vector<data::ape::ExpressionValue> &&operands);
		rkit::Result IndexString(uint32_t &outIndex, const rkit::ByteString &str);

		rkit::Result ConvertOptionalExprValue(data::ape::ExpressionValue &outExprValue, const rkit::Optional<ape_parse::ExpressionValue> &value);
		rkit::Result ConvertExprValue(uint32_t &outIndex, data::ape::OperandType &outOperandType, bool &outIsString, const ape_parse::ExpressionValue &expr);
		rkit::Result ConvertOptionalByteString(uint32_t &outDWord, const rkit::Optional<rkit::ByteString> &value);
		rkit::Result ConvertByteString(uint32_t &outDWord, const rkit::ByteString &value);
		rkit::Result ConvertFormattingValue(uint32_t &outDWord, const ape_parse::FormattingValue &value);
		rkit::Result ConvertOperand(uint32_t &outIndex, data::ape::OperandType &outOperandType, bool &outIsString, const ape_parse::Operand &operand);

	private:
		class OperandListKey final : public rkit::NoCopy
		{
		public:
			OperandListKey() = delete;
			explicit OperandListKey(rkit::Vector<data::ape::ExpressionValue> &&operands);
			OperandListKey(OperandListKey &&other);

			bool operator==(const OperandListKey &other) const;
			bool operator!=(const OperandListKey &other) const;

			rkit::HashValue_t ComputeHash() const;

		private:
			rkit::Vector<data::ape::ExpressionValue> m_operands;
		};

		class ExpressionKey final : public rkit::NoCopy
		{
		public:
			ExpressionKey() = delete;
			explicit ExpressionKey(data::ape::Expression &&expr);
			ExpressionKey(ExpressionKey &&other);

			bool operator==(const ExpressionKey &other) const;
			bool operator!=(const ExpressionKey &other) const;

			rkit::HashValue_t ComputeHash() const;

		private:
			data::ape::Expression m_expr;
		};

		template<class TKey>
		static rkit::Result IndexValue(uint32_t &outIndex, rkit::HashMap<TKey, uint32_t> &hashMap, rkit::HashValue_t hashValue, TKey &&key);

		rkit::HashMap<ExpressionKey, uint32_t> m_expressions;
		rkit::HashMap<rkit::ByteString, uint32_t> m_strings;
		rkit::HashMap<OperandListKey, uint32_t> m_operandLists;
	};

	class APEWriter final : public ape_parse::IAPEWriter
	{
	public:
		APEWriter() = delete;
		APEWriter(APECompilerContext &context, rkit::IWriteStream &stream);

		rkit::Result Write(float value) override;
		rkit::Result Write(uint8_t value) override;
		rkit::Result Write(uint16_t value) override;
		rkit::Result Write(uint32_t value) override;
		rkit::Result Write(uint64_t value) override;
		rkit::Result Write(const rkit::Optional<ape_parse::ExpressionValue> &value) override;
		rkit::Result Write(const rkit::Optional<rkit::ByteString> &value) override;
		rkit::Result Write(const rkit::ByteString &value) override;
		rkit::Result Write(const ape_parse::FormattingValue &value) override;

	private:
		rkit::Result IndexString(uint32_t &outIndex, uint32_t baseIndex, const rkit::ByteString &value);

		APECompilerContext &m_context;
		rkit::IWriteStream &m_stream;
	};

	class APEScriptCompilerImpl : public rkit::OpaqueImplementation<APEScriptCompiler>
	{
	public:
		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

	private:
		struct WindowDef
		{
			uint32_t m_windowID = 0;
			rkit::Vector<rkit::UniquePtr<ape_parse::WindowCommand>> m_commands;
		};

		struct SwitchDef
		{
			uint32_t m_switchID = 0;
			rkit::Vector<ape_parse::SwitchCommand> m_commands;
		};

		struct ParseContext
		{
			rkit::Vector<WindowDef> m_windows;
		};

		class VectorMemoryStream final : public rkit::IWriteStream
		{
		public:
			explicit VectorMemoryStream(rkit::Vector<uint8_t> &vec);

			rkit::Result WritePartial(const void *data, size_t count, size_t &outCountWritten) override;
			rkit::Result Flush() override;

		private:
			rkit::Vector<uint8_t> &m_vec;
		};

		static rkit::Result CompileWindow(APECompilerContext &ctx, rkit::Vector<uint8_t> &cmdStream, const WindowDef &wdef);
		static rkit::Result CompileSwitch(APECompilerContext &ctx, rkit::Vector<data::ape::SwitchCommand> &cmdStream, const SwitchDef &switchDef);
		static rkit::Result CompileBasicBlock(APECompilerContext &ctx, rkit::Vector<data::ape::SwitchCommand> &cmdStream, const rkit::HashMap<uint64_t, const ape_parse::SwitchCommand *> &tree, uint64_t firstCC);
		static bool IsTerminalCC(uint64_t cc);
	};

	APEWriter::APEWriter(APECompilerContext &context, rkit::IWriteStream &stream)
		: m_context(context)
		, m_stream(stream)
	{
	}

	rkit::Result APEWriter::Write(float value)
	{
		rkit::endian::LittleFloat32_t encoded(value);
		return m_stream.WriteOneBinary(encoded);
	}

	rkit::Result APEWriter::Write(uint8_t value)
	{
		return m_stream.WriteOneBinary(value);
	}

	rkit::Result APEWriter::Write(uint16_t value)
	{
		rkit::endian::LittleUInt16_t encoded(value);
		return m_stream.WriteOneBinary(value);
	}

	rkit::Result APEWriter::Write(uint32_t value)
	{
		rkit::endian::LittleUInt32_t encoded(value);
		return m_stream.WriteOneBinary(value);
	}

	rkit::Result APEWriter::Write(uint64_t value)
	{
		rkit::endian::LittleUInt64_t encoded(value);
		return m_stream.WriteOneBinary(value);
	}

	rkit::Result APEWriter::Write(const rkit::Optional<ape_parse::ExpressionValue> &value)
	{
		data::ape::ExpressionValue expr = {};
		RKIT_CHECK(m_context.ConvertOptionalExprValue(expr, value));
		return m_stream.WriteOneBinary(expr);
	}

	rkit::Result APEWriter::Write(const rkit::Optional<rkit::ByteString> &value)
	{
		uint32_t index = 0;
		RKIT_CHECK(m_context.ConvertOptionalByteString(index, value));
		rkit::endian::LittleUInt32_t indexData = rkit::endian::LittleUInt32_t(index);

		return m_stream.WriteOneBinary(indexData);
	}

	rkit::Result APEWriter::Write(const rkit::ByteString &value)
	{
		uint32_t index = 0;
		RKIT_CHECK(IndexString(index, 0, value));
		return Write(index);
	}

	rkit::Result APEWriter::Write(const ape_parse::FormattingValue &value)
	{
		uint32_t index = 0;
		RKIT_CHECK(m_context.ConvertFormattingValue(index, value));
		rkit::endian::LittleUInt32_t indexData = rkit::endian::LittleUInt32_t(index);
		return m_stream.WriteOneBinary(indexData);
	}

	rkit::Result APEWriter::IndexString(uint32_t &outIndex, uint32_t baseIndex, const rkit::ByteString &value)
	{
		uint32_t ctxIndex = 0;
		RKIT_CHECK(m_context.IndexString(ctxIndex, value));

		if (ctxIndex >= std::numeric_limits<uint32_t>::max() - baseIndex)
			RKIT_THROW(rkit::ResultCode::kDataError);

		outIndex = ctxIndex + baseIndex;

		RKIT_RETURN_OK;
	}

	rkit::Result APEScriptCompilerImpl::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::CIPath path;
		RKIT_CHECK(path.Set(depsNode->GetIdentifier()));

		rkit::UniquePtr<rkit::ISeekableReadStream> inputFile;
		RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, path, inputFile));

		rkit::Vector<WindowDef> windowDefs;
		rkit::Vector<SwitchDef> switchDefs;

		{
			rkit::endian::LittleUInt64_t header;
			RKIT_CHECK(inputFile->ReadOneBinary(header));

			if (header.Get() != 0xffffffff0000013d)
			{
				rkit::log::Error(u8"Invalid APE header");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			// Load windows
			for (;;)
			{
				rkit::endian::LittleUInt32_t windowID;
				RKIT_CHECK(inputFile->ReadOneBinary(windowID));

				if (windowID.Get() == 0)
					break;

				WindowDef windowDef;
				windowDef.m_windowID = windowID.Get();

				for (;;)
				{
					uint8_t opcode = 0;
					RKIT_CHECK(inputFile->ReadOneBinary(opcode));

					rkit::UniquePtr<ape_parse::WindowCommand> cmd;
					RKIT_CHECK(ape_parse::CreateWindowCommand(cmd, opcode));

					ape_parse::APEReader reader(*inputFile);
					RKIT_CHECK(cmd->Parse(reader));

					if (cmd->GetCommandType() == data::WindowCommandType::End)
						break;

					RKIT_CHECK(windowDef.m_commands.Append(std::move(cmd)));
				}

				RKIT_CHECK(windowDefs.Append(std::move(windowDef)));
			}

			// Load switches
			{
				ape_parse::APEReader reader(*inputFile);
				uint32_t switchMarker = 0;
				RKIT_CHECK(reader.Read(switchMarker));
				if (switchMarker != 0xfffffffeu)
					RKIT_THROW(rkit::ResultCode::kDataError);

				for (;;)
				{
					uint32_t switchLabel = 0;
					RKIT_CHECK(reader.Read(switchLabel));
					if (switchLabel == 0)
						break;

					SwitchDef switchDef;
					switchDef.m_switchID = switchLabel;

					for (;;)
					{
						ape_parse::SwitchCommand cmd;
						RKIT_CHECK(reader.Read(cmd.m_cc));
						RKIT_CHECK(reader.Read(cmd.m_opcode));

						if (cmd.m_opcode > 21)
						{
							if (cmd.m_opcode == 69)
								break;	// End

							RKIT_THROW(rkit::ResultCode::kDataError);
						}

						RKIT_CHECK(reader.Read(cmd.m_str));
						RKIT_CHECK(reader.Read(cmd.m_fmt));
						RKIT_CHECK(reader.Read(cmd.m_expr));
						RKIT_CHECK(switchDef.m_commands.Append(std::move(cmd)));
					}

					RKIT_CHECK(switchDefs.Append(std::move(switchDef)));
				}
			}
		}

		APECompilerContext compilerCtx;

		rkit::Vector<rkit::Vector<uint8_t>> windowCommandStreams;
		RKIT_CHECK(windowCommandStreams.Resize(windowDefs.Count()));

		for (size_t windowIndex = 0; windowIndex < windowDefs.Count(); windowIndex++)
		{
			RKIT_CHECK(CompileWindow(compilerCtx, windowCommandStreams[windowIndex], windowDefs[windowIndex]));
		}


		rkit::Vector<rkit::Vector<data::ape::SwitchCommand>> switchCommandLists;
		RKIT_CHECK(switchCommandLists.Resize(switchDefs.Count()));

		for (size_t switchIndex = 0; switchIndex < switchDefs.Count(); switchIndex++)
		{
			RKIT_CHECK(CompileSwitch(compilerCtx, switchCommandLists[switchIndex], switchDefs[switchIndex]));
		}

#if 0
		rkit::CIPath outPath;
		{
			rkit::String pathStr;
			RKIT_CHECK(pathStr.Format(u8"ax_ape/%s", depsNode->GetIdentifier()));
			RKIT_CHECK(outPath.Set(outPath));
		}


		rkit::UniquePtr<rkit::ISeekableReadWriteStream> outFile;
		RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kOutputFiles, path, outFile));
#endif

		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}


	rkit::Result APECompilerContext::IndexExpression(uint32_t &outIndex, data::ape::Expression &&expr)
	{
		ExpressionKey exprKey(std::move(expr));
		return IndexValue(outIndex, m_expressions, exprKey.ComputeHash(), std::move(exprKey));
	}

	rkit::Result APECompilerContext::IndexOperandList(uint32_t &outIndex, rkit::Vector<data::ape::ExpressionValue> &&operands)
	{
		OperandListKey opsKey(std::move(operands));
		return IndexValue(outIndex, m_operandLists, opsKey.ComputeHash(), std::move(opsKey));
	}

	rkit::Result APECompilerContext::IndexString(uint32_t &outIndex, const rkit::ByteString &str)
	{
		const rkit::HashValue_t hashValue = rkit::Hasher<rkit::ByteString>::ComputeHash(0, str);
		return IndexValue<rkit::ByteString>(outIndex, m_strings, hashValue, rkit::ByteString(str));
	}

	rkit::Result APECompilerContext::ConvertOptionalExprValue(data::ape::ExpressionValue &outExprValue, const rkit::Optional<ape_parse::ExpressionValue> &value)
	{
		if (!value.IsSet())
		{
			outExprValue.m_exprType = data::ape::ExprType::Empty;
			outExprValue.m_index = 0;
		}
		else
		{
			uint32_t index = 0;
			bool isString = false;
			data::ape::OperandType operandType = data::ape::OperandType::Invalid;
			RKIT_CHECK(ConvertExprValue(index, operandType, isString, value.Get()));

			switch (operandType)
			{
			case data::ape::OperandType::Expression:
				if (isString)
					RKIT_THROW(rkit::ResultCode::kDataError);
				outExprValue.m_exprType = data::ape::ExprType::FloatExpression;
				break;
			case data::ape::OperandType::Literal:
				outExprValue.m_exprType = isString ? data::ape::ExprType::StringLiteral : data::ape::ExprType::FloatLiteral;
				break;
			case data::ape::OperandType::Variable:
				outExprValue.m_exprType = isString ? data::ape::ExprType::StringVariable : data::ape::ExprType::FloatVariable;
				break;
			default:
				RKIT_THROW(rkit::ResultCode::kInternalError);
			}

			outExprValue.m_index = index;
		}

		RKIT_RETURN_OK;
	}

	rkit::Result APECompilerContext::ConvertExprValue(uint32_t &outIndex, data::ape::OperandType &outOperandType, bool &outIsString, const ape_parse::ExpressionValue &expr)
	{
		data::ape::Operator op = data::ape::Operator::Invalid;
		data::ape::OperandType leftOpType = data::ape::OperandType::Invalid;
		data::ape::OperandType rightOpType = data::ape::OperandType::Invalid;

		bool leftIsString = false;
		bool rightIsString = false;

		uint32_t leftIndex = 0;
		uint32_t rightIndex = 0;

		switch (expr.m_operator)
		{
		case ape_parse::ExpressionValue::Operator::Or:
		case ape_parse::ExpressionValue::Operator::And:
		case ape_parse::ExpressionValue::Operator::Xor:
		case ape_parse::ExpressionValue::Operator::Gt:
		case ape_parse::ExpressionValue::Operator::Lt:
		case ape_parse::ExpressionValue::Operator::Ge:
		case ape_parse::ExpressionValue::Operator::Le:
		case ape_parse::ExpressionValue::Operator::Add:
		case ape_parse::ExpressionValue::Operator::Sub:
		case ape_parse::ExpressionValue::Operator::Mul:
		case ape_parse::ExpressionValue::Operator::Div:
			RKIT_CHECK(ConvertOperand(leftIndex, leftOpType, leftIsString, *expr.m_left));
			RKIT_CHECK(ConvertOperand(rightIndex, rightOpType, rightIsString, *expr.m_right));
			if (leftIsString || rightIsString)
				RKIT_THROW(rkit::ResultCode::kDataError);

			op = static_cast<data::ape::Operator>(expr.m_operator);
			break;
		case ape_parse::ExpressionValue::Operator::Eq:
		case ape_parse::ExpressionValue::Operator::Neq:
			RKIT_CHECK(ConvertOperand(leftIndex, leftOpType, leftIsString, *expr.m_left));
			RKIT_CHECK(ConvertOperand(rightIndex, rightOpType, rightIsString, *expr.m_right));
			if (leftIsString && rightIsString)
				op = (expr.m_operator == ape_parse::ExpressionValue::Operator::Eq) ? data::ape::Operator::StrEq : data::ape::Operator::StrNeq;
			else
			{
				if (leftIsString || rightIsString)
					RKIT_THROW(rkit::ResultCode::kDataError);

				op = static_cast<data::ape::Operator>(expr.m_operator);
			}
			break;
		default:
			RKIT_THROW(rkit::ResultCode::kDataError);
		};

		data::ape::Expression resultExpr = {};
		resultExpr.m_leftValue = leftIndex;
		resultExpr.m_rightValue = rightIndex;
		resultExpr.m_packedOperandInfo = data::ape::Expression::PackOperandInfo(op, leftOpType, rightOpType);

		uint32_t exprIndex = 0;
		RKIT_CHECK(IndexExpression(exprIndex, std::move(resultExpr)));

		outIndex = exprIndex;
		outIsString = false;
		outOperandType = data::ape::OperandType::Expression;

		RKIT_RETURN_OK;
	}

	rkit::Result APECompilerContext::ConvertOperand(uint32_t &outIndex, data::ape::OperandType &outOperandType, bool &outIsString, const ape_parse::Operand &operand)
	{
		switch (operand.GetOperandType())
		{
		case ape_parse::OperandType::Expression:
			return ConvertExprValue(outIndex, outOperandType, outIsString, static_cast<const ape_parse::ExpressionOperand &>(operand).m_value);
		case ape_parse::OperandType::FloatLiteral:
			memcpy(&outIndex, &static_cast<const ape_parse::FloatOperand &>(operand).m_value, 4);
			outIsString = false;
			outOperandType = data::ape::OperandType::Literal;
			break;
		case ape_parse::OperandType::FloatVariable:
			RKIT_CHECK(IndexString(outIndex, static_cast<const ape_parse::FloatVariableNameOperand &>(operand).m_value));
			outIsString = false;
			outOperandType = data::ape::OperandType::Variable;
			break;
		case ape_parse::OperandType::StringLiteral:
			RKIT_CHECK(IndexString(outIndex, static_cast<const ape_parse::StringOperand &>(operand).m_value));
			outIsString = true;
			outOperandType = data::ape::OperandType::Literal;
			break;
		case ape_parse::OperandType::StringVariable:
			RKIT_CHECK(IndexString(outIndex, static_cast<const ape_parse::StringVariableNameOperand &>(operand).m_value));
			outIsString = true;
			outOperandType = data::ape::OperandType::Variable;
			break;
		default:
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		RKIT_RETURN_OK;
	}

	rkit::Result APECompilerContext::ConvertOptionalByteString(uint32_t &outDWord, const rkit::Optional<rkit::ByteString> &value)
	{
		if (!value.IsSet())
			outDWord = 0;
		else
		{
			uint32_t index = 0;
			RKIT_CHECK(IndexString(index, value.Get()));
			RKIT_CHECK(rkit::SafeAdd<uint32_t>(outDWord, index, 1));
		}

		RKIT_RETURN_OK;
	}

	rkit::Result APECompilerContext::ConvertByteString(uint32_t &outDWord, const rkit::ByteString &value)
	{
		return IndexString(outDWord, value);
	}

	rkit::Result APECompilerContext::ConvertFormattingValue(uint32_t &outDWord, const ape_parse::FormattingValue &value)
	{
		if (value.m_operands.Count() > std::numeric_limits<uint32_t>::max())
			RKIT_THROW(rkit::ResultCode::kDataError);

		const uint32_t numOperands = static_cast<uint32_t>(value.m_operands.Count());

		rkit::Vector<data::ape::ExpressionValue> operands;

		for (const rkit::UniquePtr<ape_parse::Operand> &inOperandPtr : value.m_operands)
		{
			const ape_parse::Operand &inOperand = *inOperandPtr;

			data::ape::ExprType exprType = data::ape::ExprType::Empty;
			uint32_t index = 0;
			data::ape::ExpressionValue outExpr;
			switch (inOperand.GetOperandType())
			{
			case ape_parse::OperandType::FloatLiteral:
				exprType = data::ape::ExprType::FloatLiteral;
				memcpy(&index, &static_cast<const ape_parse::FloatOperand &>(inOperand).m_value, 4);
				break;
			case ape_parse::OperandType::FloatVariable:
				exprType = data::ape::ExprType::FloatVariable;
				RKIT_CHECK(ConvertByteString(index, static_cast<const ape_parse::FloatVariableNameOperand &>(inOperand).m_value));
				break;
			case ape_parse::OperandType::StringLiteral:
				exprType = data::ape::ExprType::StringLiteral;
				RKIT_CHECK(ConvertByteString(index, static_cast<const ape_parse::StringOperand &>(inOperand).m_value));
				break;
			case ape_parse::OperandType::StringVariable:
				exprType = data::ape::ExprType::StringVariable;
				RKIT_CHECK(ConvertByteString(index, static_cast<const ape_parse::StringVariableNameOperand &>(inOperand).m_value));
				break;
			default:
				RKIT_THROW(rkit::ResultCode::kInternalError);
			}

			outExpr.m_exprType = exprType;
			outExpr.m_index = index;
			RKIT_CHECK(operands.Append(outExpr));
		}

		return IndexOperandList(outDWord, std::move(operands));
	}

	template<class TKey>
	rkit::Result APECompilerContext::IndexValue(uint32_t &outIndex, rkit::HashMap<TKey, uint32_t> &hashMap, rkit::HashValue_t hashValue, TKey &&key)
	{
		const typename rkit::HashMap<TKey, uint32_t>::ConstIterator_t it = hashMap.FindPrehashed(hashValue, key);
		if (it == hashMap.end())
		{
			const size_t newIndex = hashMap.Count();
			if (newIndex > std::numeric_limits<uint32_t>::max())
				RKIT_THROW(rkit::ResultCode::kDataError);

			RKIT_CHECK(hashMap.SetPrehashed(hashValue, std::move(key), static_cast<uint32_t>(newIndex)));
			outIndex = static_cast<uint32_t>(newIndex);
		}
		else
			outIndex = it.Value();

		RKIT_RETURN_OK;
	}

	APECompilerContext::OperandListKey::OperandListKey(rkit::Vector<data::ape::ExpressionValue> &&operands)
		: m_operands(std::move(operands))
	{
	}

	APECompilerContext::OperandListKey::OperandListKey(OperandListKey &&other)
		: m_operands(std::move(other.m_operands))
	{
	}

	bool APECompilerContext::OperandListKey::operator==(const OperandListKey &other) const
	{
		return rkit::CompareSpansEqual(m_operands.ToSpan().ReinterpretCast<const uint8_t>(), other.m_operands.ToSpan().ReinterpretCast<const uint8_t>());
	}

	bool APECompilerContext::OperandListKey::operator!=(const OperandListKey &other) const
	{
		return !((*this) == other);
	}

	rkit::HashValue_t APECompilerContext::OperandListKey::ComputeHash() const
	{
		return rkit::BinaryHasher<data::ape::ExpressionValue>::ComputeHash(0, m_operands.ToSpan());
	}

	APECompilerContext::ExpressionKey::ExpressionKey(data::ape::Expression &&expr)
		: m_expr(std::move(expr))
	{
	}

	APECompilerContext::ExpressionKey::ExpressionKey(ExpressionKey &&other)
		: m_expr(std::move(other.m_expr))
	{
	}

	bool APECompilerContext::ExpressionKey::operator==(const ExpressionKey &other) const
	{
		return !memcmp(&m_expr, &other.m_expr, sizeof(m_expr));
	}

	bool APECompilerContext::ExpressionKey::operator!=(const ExpressionKey &other) const
	{
		return !((*this) == other);
	}

	rkit::HashValue_t APECompilerContext::ExpressionKey::ComputeHash() const
	{
		return rkit::BinaryHasher<data::ape::Expression>::ComputeHash(0, m_expr);
	}


	APEScriptCompilerImpl::VectorMemoryStream::VectorMemoryStream(rkit::Vector<uint8_t> &vec)
		: m_vec(vec)
	{
	}

	rkit::Result APEScriptCompilerImpl::VectorMemoryStream::WritePartial(const void *data, size_t count, size_t &outCountWritten)
	{
		RKIT_CHECK(m_vec.Append(rkit::Span<const uint8_t>(static_cast<const uint8_t *>(data), count)));
		outCountWritten = count;
		RKIT_RETURN_OK;
	}

	rkit::Result APEScriptCompilerImpl::VectorMemoryStream::Flush()
	{
		RKIT_RETURN_OK;
	}

	rkit::Result APEScriptCompilerImpl::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result APEScriptCompilerImpl::CompileWindow(APECompilerContext &ctx, rkit::Vector<uint8_t> &cmdStream, const WindowDef &wdef)
	{
		VectorMemoryStream stream(cmdStream);

		APEWriter writer(ctx, stream);

		for (const rkit::UniquePtr<ape_parse::WindowCommand> &cmdPtr : wdef.m_commands)
		{
			const ape_parse::WindowCommand &cmd = *cmdPtr;

			RKIT_CHECK(cmdStream.Append(static_cast<uint8_t>(cmd.GetCommandType())));
			RKIT_CHECK(cmd.Write(writer));
		}

		RKIT_RETURN_OK;
	}

	rkit::Result APEScriptCompilerImpl::CompileSwitch(APECompilerContext &ctx, rkit::Vector<data::ape::SwitchCommand> &cmdStream, const SwitchDef &switchDef)
	{
		rkit::HashMap<uint64_t, const ape_parse::SwitchCommand *> switchCmdTree;

		for (const ape_parse::SwitchCommand &inCmd : switchDef.m_commands)
		{
			RKIT_CHECK(switchCmdTree.Set(inCmd.m_cc, &inCmd));
		}

		return CompileBasicBlock(ctx, cmdStream, switchCmdTree, 1);
	}

	rkit::Result APEScriptCompilerImpl::CompileBasicBlock(APECompilerContext &ctx, rkit::Vector<data::ape::SwitchCommand> &cmdStream, const rkit::HashMap<uint64_t, const ape_parse::SwitchCommand *> &tree, uint64_t cc)
	{
		const uint8_t kIfOpcode = 1;
		const uint8_t kWhileOpcode = 11;
		const uint8_t kJumpOpcode = 22;
		const uint8_t kRJumpOpcode = 23;

		for (;;)
		{
			rkit::HashMap<uint64_t, const ape_parse::SwitchCommand *>::ConstIterator_t it = tree.Find(cc);
			if (it == tree.end())
				break;

			const ape_parse::SwitchCommand &cmd = *it.Value();

			data::ape::SwitchCommand outCmd = {};
			outCmd.m_opcode = static_cast<uint8_t>(cmd.m_opcode);

			rkit::Vector<data::ape::SwitchCommand> trueBB;
			rkit::Vector<data::ape::SwitchCommand> falseBB;

			if (cmd.m_opcode == kIfOpcode)
			{
				if (!IsTerminalCC(cc))
				{
					RKIT_CHECK(CompileBasicBlock(ctx, trueBB, tree, ((cc << 2) | 1u)));
					RKIT_CHECK(CompileBasicBlock(ctx, falseBB, tree, ((cc << 2) | 2u)));

					if (falseBB.Count() > std::numeric_limits<uint32_t>::max())
						RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

					if (falseBB.Count() > 0)
					{
						data::ape::SwitchCommand skipFalseBBCmd = {};
						skipFalseBBCmd.m_opcode = kJumpOpcode;
						skipFalseBBCmd.m_strValue = static_cast<uint32_t>(falseBB.Count());

						RKIT_CHECK(trueBB.Append(skipFalseBBCmd));
					}

					if (trueBB.Count() > std::numeric_limits<uint32_t>::max())
						RKIT_THROW(rkit::ResultCode::kIntegerOverflow);
				}

				outCmd.m_strValue = static_cast<uint32_t>(trueBB.Count());
			}
			else if (cmd.m_opcode == kWhileOpcode)
			{
				if (!IsTerminalCC(cc))
				{
					RKIT_CHECK(CompileBasicBlock(ctx, trueBB, tree, ((cc << 2) | 1u)));

					data::ape::SwitchCommand repeatCmd = {};
					repeatCmd.m_opcode = kRJumpOpcode;
					repeatCmd.m_strValue = static_cast<uint32_t>(trueBB.Count());

					RKIT_CHECK(trueBB.Append(repeatCmd));
				}

				outCmd.m_strValue = static_cast<uint32_t>(trueBB.Count());
			}
			else
			{
				uint32_t index = 0;
				RKIT_CHECK(ctx.ConvertOptionalByteString(index, cmd.m_str));
				outCmd.m_strValue = index;
			}

			RKIT_CHECK(ctx.ConvertOptionalExprValue(outCmd.m_exprValue, cmd.m_expr));

			uint32_t fmtValue = 0;
			RKIT_CHECK(ctx.ConvertFormattingValue(fmtValue, cmd.m_fmt));
			outCmd.m_fmtValue = fmtValue;

			RKIT_CHECK(cmdStream.Append(outCmd));
			RKIT_CHECK(cmdStream.Append(trueBB.ToSpan()));
			RKIT_CHECK(cmdStream.Append(falseBB.ToSpan()));

			if (IsTerminalCC(cc))
				break;

			cc = (cc << 2) | 3u;
		}

		RKIT_RETURN_OK;
	}

	bool APEScriptCompilerImpl::IsTerminalCC(uint64_t cc)
	{
		return ((cc >> 62) & 3) != 0;
	}


	bool APEScriptCompiler::HasAnalysisStage() const
	{
		return true;
	}

	rkit::Result APEScriptCompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return Impl().RunAnalysis(depsNode, feedback);
	}

	rkit::Result APEScriptCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return Impl().RunCompile(depsNode, feedback);
	}

	uint32_t APEScriptCompiler::GetVersion() const
	{
		return 1;
	}

	rkit::Result APEScriptCompiler::Create(rkit::UniquePtr<APEScriptCompiler> &outCompiler)
	{
		return rkit::New<APEScriptCompiler>(outCompiler);
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::buildsystem::APEScriptCompilerImpl)
