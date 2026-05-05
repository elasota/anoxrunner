#include "AnoxAPEScriptCompiler.h"
#include "AnoxAPEWindowCommandData.generated.h"

#include "anox/Data/APEScript.h"
#include "anox/AnoxModule.h"

#include "rkit/Core/HashTable.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/NoCopy.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/QuickSort.h"

#include "rkit/BuildSystem/BuildSystem.h"
#include "rkit/Data/ContentID.h"

#include "AnoxMaterialCompiler.h"


namespace anox::buildsystem
{
	class APECompilerContext;

	class APECompilerHelper
	{
	public:
		template<class TKey>
		static rkit::Result IndexValue(uint32_t &outIndex, rkit::HashMap<TKey, uint32_t> &hashMap, rkit::HashValue_t hashValue, TKey &&key);

		template<class TKey>
		static rkit::Result IndexValue(uint32_t &outIndex, rkit::HashMap<TKey, uint32_t> &hashMap, TKey &&key);
	};

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
		rkit::Result ConvertMaterial(data::ape::MaterialReference &outMaterialRef, const rkit::ByteString &bstr);

		rkit::Result DumpResults(rkit::Vector<rkit::Vector<data::ape::ExpressionValue>> &outOperandLists,
			rkit::Vector<data::ape::Expression> &outExprs, rkit::Vector<rkit::ByteString> &outStrings,
			rkit::Vector<rkit::String> &outMaterialWildcards, rkit::Vector<rkit::CIPath> &outMaterialNames) const;

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

			rkit::ConstSpan<data::ape::ExpressionValue> GetOperands() const;

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

			const data::ape::Expression &GetExpression() const;

		private:
			data::ape::Expression m_expr;
		};

		rkit::HashMap<ExpressionKey, uint32_t> m_expressions;
		rkit::HashMap<rkit::ByteString, uint32_t> m_strings;
		rkit::HashMap<OperandListKey, uint32_t> m_operandLists;
		rkit::HashMap<rkit::CIPath, uint32_t> m_materialNames;
		rkit::HashMap<rkit::String, uint32_t> m_materialWildcards;
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
		rkit::Result Write(const ape_parse::TextureID &value) override;
		rkit::Result Write(const ape_parse::WindowStyleID &value) override;

	private:
		rkit::Result IndexString(uint32_t &outIndex, uint32_t baseIndex, const rkit::ByteString &value);

		APECompilerContext &m_context;
		rkit::IWriteStream &m_stream;
	};

	class APEScriptCompilerImpl : public rkit::OpaqueImplementation<APEScriptCompiler>
	{
	public:
		friend class APEScriptCompiler;

		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

	private:
		typedef rkit::BasePath<false, rkit::DefaultPathTraits<rkit::PathTraitFlags::kIsCaseInsensitive | rkit::PathTraitFlags::kAllowWildcards>> WildcardPath_t;

		struct WindowDef
		{
			uint32_t m_windowID = 0;
			rkit::Vector<rkit::UniquePtr<ape_parse::WindowCommand>> m_commands;
		};

		struct CompiledWindowDef
		{
			rkit::endian::LittleUInt32_t m_windowID;
			rkit::Vector<uint8_t> m_commandStream;
		};

		struct SwitchDef
		{
			uint32_t m_switchID = 0;
			rkit::Vector<ape_parse::SwitchCommand> m_commands;
		};

		struct CompiledSwitchDef
		{
			rkit::endian::LittleUInt32_t m_switchID;
			rkit::Vector<data::ape::SwitchCommand> m_commands;
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

		struct APEBlob
		{
			rkit::Vector<rkit::ByteString> m_strings;
			rkit::Vector<data::ape::Expression> m_exprs;
			rkit::Vector<rkit::Vector<data::ape::ExpressionValue>> m_operandLists;
			rkit::Vector<CompiledWindowDef> m_windows;
			rkit::Vector<CompiledSwitchDef> m_switches;
			rkit::Vector<rkit::String> m_materialWildcards;
			rkit::Vector<rkit::CIPath> m_materialNames;
			rkit::Vector<rkit::data::ContentID> m_materialContentIDs;
			rkit::Vector<rkit::endian::LittleUInt32_t> m_materialNameLookups;
			rkit::Vector<data::ape::MaterialWildcardLookup> m_materialWildcardLookups;
		};

		static rkit::Result CompileWindow(APECompilerContext &ctx, CompiledWindowDef &compiledWindow, const WindowDef &wdef);
		static rkit::Result CompileSwitch(APECompilerContext &ctx, CompiledSwitchDef &compiledSwitch, const SwitchDef &switchDef);
		static rkit::Result CompileBasicBlock(APECompilerContext &ctx, rkit::Vector<data::ape::SwitchCommand> &cmdStream, const rkit::HashMap<uint64_t, const ape_parse::SwitchCommand *> &tree, uint64_t firstCC);
		static bool IsTerminalCC(uint64_t cc);

		static rkit::Result DumpAPEFile(rkit::IWriteStream &stream, const APEBlob &blob);

		static rkit::Result ReadAPEFile(rkit::IReadStream &stream, APEBlob& blob);

		static rkit::Result FormatExtraDepsPath(rkit::CIPath &outPath, const rkit::StringView &identifier);
		static rkit::Result FormatAnalysisPath(rkit::CIPath &outPath, const rkit::StringView &identifier);
		static rkit::Result FormatOutputPath(rkit::CIPath &outPath, const rkit::StringView &identifier);
	};


	class APEGroupCompilerImpl final : public rkit::OpaqueImplementation<APEGroupCompiler>
	{
	public:
		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

	private:
		static rkit::Result ResolvePath(rkit::CIPath &depsFilePath, const rkit::StringView &groupNodeIdentifier, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
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

	rkit::Result APEWriter::Write(const ape_parse::TextureID &value)
	{
		data::ape::MaterialReference matRef = {};

		rkit::ByteString bstr = value.m_str;
		if (bstr.StartsWith(rkit::StringSliceView(u8"../").RemoveEncoding()))
		{
			RKIT_CHECK(bstr.Set(bstr.SubString(3, bstr.Length() - 3)));
		}
		else
		{
			rkit::ByteString adjusted;
			RKIT_CHECK(adjusted.Set(rkit::StringSliceView(u8"gameflow/").RemoveEncoding()));
			RKIT_CHECK(adjusted.Append(bstr));

			bstr = std::move(adjusted);
		}

		RKIT_CHECK(m_context.ConvertMaterial(matRef, bstr));
		return m_stream.WriteOneBinary(matRef);
	}

	rkit::Result APEWriter::Write(const ape_parse::WindowStyleID &value)
	{
		data::ape::MaterialReference matRef = {};

		if (value.m_str.EqualsNoCase(rkit::StringView(u8"null").RemoveEncoding()))
		{
			matRef.m_index = 0;
			matRef.m_refType = data::ape::MaterialReferenceType::Null;
		}
		else
		{
			rkit::ByteString adjusted;
			RKIT_CHECK(adjusted.Set(rkit::StringSliceView(u8"graphics/interface/windows/").RemoveEncoding()));
			RKIT_CHECK(adjusted.Append(value.m_str));

			RKIT_CHECK(m_context.ConvertMaterial(matRef, adjusted));
		}

		return m_stream.WriteOneBinary(matRef);
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


		APEBlob blob;

		RKIT_CHECK(blob.m_windows.Resize(windowDefs.Count()));

		for (size_t windowIndex = 0; windowIndex < windowDefs.Count(); windowIndex++)
		{
			RKIT_CHECK(CompileWindow(compilerCtx, blob.m_windows[windowIndex], windowDefs[windowIndex]));
		}

		RKIT_CHECK(blob.m_switches.Resize(switchDefs.Count()));

		for (size_t switchIndex = 0; switchIndex < switchDefs.Count(); switchIndex++)
		{
			RKIT_CHECK(CompileSwitch(compilerCtx, blob.m_switches[switchIndex], switchDefs[switchIndex]));
		}

		RKIT_CHECK(compilerCtx.DumpResults(blob.m_operandLists, blob.m_exprs, blob.m_strings, blob.m_materialWildcards, blob.m_materialNames));

		{
			rkit::CIPath outPath;
			RKIT_CHECK(FormatAnalysisPath(outPath, depsNode->GetIdentifier()));

			rkit::UniquePtr<rkit::ISeekableReadWriteStream> outFile;
			RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outPath, outFile));

			RKIT_CHECK(DumpAPEFile(*outFile, blob));
		}

		if (blob.m_materialWildcards.Count() > 0)
		{
			rkit::CIPath outPath;
			RKIT_CHECK(FormatExtraDepsPath(outPath, depsNode->GetIdentifier()));

			rkit::UniquePtr<rkit::ISeekableReadWriteStream> outFile;
			RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outPath, outFile));

			for (const rkit::String &wildcard : blob.m_materialWildcards)
			{
				rkit::StaticArray<char, 11> prefix;
				prefix[0] = ':';
				rkit::utils::ExtractFourCC(kAnoxNamespaceID, prefix[1], prefix[2], prefix[3], prefix[4]);
				prefix[5] = ':';
				rkit::utils::ExtractFourCC(anox::buildsystem::kInterfaceMaterialNodeID, prefix[6], prefix[7], prefix[8], prefix[9]);
				prefix[10] = ' ';

				RKIT_CHECK(outFile->WriteAllSpan(prefix.ToSpan()));
				RKIT_CHECK(outFile->WriteAllSpan(wildcard.ToSpan()));
				RKIT_CHECK(outFile->WriteOneBinary(static_cast<uint8_t>('\n')));
			}

			RKIT_CHECK(feedback->AddNodeDependency(
				rkit::buildsystem::kDefaultNamespace,
				rkit::buildsystem::kDepsNodeID,
				rkit::buildsystem::BuildFileLocation::kIntermediateDir,
				outPath.ToString()));
		}

		for (const rkit::CIPath &path : blob.m_materialNames)
		{
			RKIT_CHECK(feedback->AddNodeDependency(
				kAnoxNamespaceID,
				anox::buildsystem::kInterfaceMaterialNodeID,
				rkit::buildsystem::BuildFileLocation::kSourceDir,
				path.ToString()));
		}

		RKIT_RETURN_OK;
	}


	rkit::Result APECompilerContext::IndexExpression(uint32_t &outIndex, data::ape::Expression &&expr)
	{
		ExpressionKey exprKey(std::move(expr));
		return APECompilerHelper::IndexValue(outIndex, m_expressions, exprKey.ComputeHash(), std::move(exprKey));
	}

	rkit::Result APECompilerContext::IndexOperandList(uint32_t &outIndex, rkit::Vector<data::ape::ExpressionValue> &&operands)
	{
		OperandListKey opsKey(std::move(operands));
		return APECompilerHelper::IndexValue(outIndex, m_operandLists, opsKey.ComputeHash(), std::move(opsKey));
	}

	rkit::Result APECompilerContext::IndexString(uint32_t &outIndex, const rkit::ByteString &str)
	{
		return APECompilerHelper::IndexValue<rkit::ByteString>(outIndex, m_strings, rkit::ByteString(str));
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

	rkit::Result APECompilerContext::ConvertMaterial(data::ape::MaterialReference &outMaterialRef, const rkit::ByteString &pathBStr)
	{
		if (!rkit::CharacterEncodingValidator<rkit::CharacterEncoding::kASCII>::ValidateSpan(pathBStr.ToSpan()))
		{
			rkit::log::Error(u8"Material path had invalid characters");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		bool isWildcard = false;
		for (const uint8_t ch : pathBStr.ToSpan())
		{
			if (ch == '$')
			{
				isWildcard = true;
				break;
			}
		}

		uint32_t index = 0;
		if (isWildcard)
		{
			outMaterialRef.m_refType = data::ape::MaterialReferenceType::WildcardString;

			rkit::Vector<rkit::Utf8Char_t> wildcardStrChars;
			rkit::Vector<uint8_t> formatStrChars;

			{
				bool isInFillIn = false;
				for (uint8_t ch : pathBStr.ToSpan())
				{
					if (ch == u8'$')
					{
						isInFillIn = !isInFillIn;
						if (isInFillIn)
						{
							RKIT_CHECK(wildcardStrChars.Append(u8'*'));
						}
						RKIT_CHECK(formatStrChars.Append(ch));
					}
					else
					{
						const uint8_t lowerChar = rkit::InvariantCharCaseAdjuster<uint8_t>::ToLower(ch);

						if (isInFillIn)
						{
							RKIT_CHECK(formatStrChars.Append(ch));
						}
						else
						{
							RKIT_CHECK(wildcardStrChars.Append(static_cast<rkit::Utf8Char_t>(lowerChar)));
							RKIT_CHECK(formatStrChars.Append(lowerChar));
						}
					}
				}
			}


			{
				rkit::String wildcardStr;
				RKIT_CHECK(wildcardStr.Set(wildcardStrChars.ToSpan()));

				uint32_t wildcardIndex = 0;
				RKIT_CHECK(APECompilerHelper::IndexValue<rkit::String>(wildcardIndex, m_materialWildcards, std::move(wildcardStr)));
			}

			{
				rkit::ByteString formatStr;
				RKIT_CHECK(formatStr.Set(formatStrChars.ToSpan()));

				RKIT_CHECK(APECompilerHelper::IndexValue<rkit::ByteString>(index, m_strings, std::move(formatStr)));
			}
		}
		else
		{
			rkit::CIPath path;
			RKIT_CHECK(path.Set(rkit::ByteStringView(pathBStr).ToUTF8Unsafe()));

			RKIT_CHECK(APECompilerHelper::IndexValue<rkit::CIPath>(index, m_materialNames, rkit::CIPath(path)));
			outMaterialRef.m_refType = data::ape::MaterialReferenceType::ContentID;
		}

		outMaterialRef.m_index = index;

		RKIT_RETURN_OK;
	}


	rkit::Result APECompilerContext::DumpResults(rkit::Vector<rkit::Vector<data::ape::ExpressionValue>> &outOperandLists,
		rkit::Vector<data::ape::Expression> &outExprs, rkit::Vector<rkit::ByteString> &outStrings,
		rkit::Vector<rkit::String> &outMaterialWildcards, rkit::Vector<rkit::CIPath> &outMaterialNames) const
	{
		RKIT_CHECK(outOperandLists.Resize(m_operandLists.Count()));
		RKIT_CHECK(outExprs.Resize(m_expressions.Count()));
		RKIT_CHECK(outStrings.Resize(m_strings.Count()));
		RKIT_CHECK(outMaterialWildcards.Resize(m_materialWildcards.Count()));
		RKIT_CHECK(outMaterialNames.Resize(m_materialNames.Count()));


		for (rkit::HashMapKeyValueView<ExpressionKey, const uint32_t> exprPair : m_expressions)
			outExprs[exprPair.Value()] = exprPair.Key().GetExpression();

		for (rkit::HashMapKeyValueView<rkit::ByteString, const uint32_t> strPair : m_strings)
			outStrings[strPair.Value()] = strPair.Key();

		for (rkit::HashMapKeyValueView<rkit::CIPath, const uint32_t> strPair : m_materialNames)
			outMaterialNames[strPair.Value()] = strPair.Key();

		for (rkit::HashMapKeyValueView<rkit::String, const uint32_t> strPair : m_materialWildcards)
			outMaterialWildcards[strPair.Value()] = strPair.Key();

		for (rkit::HashMapKeyValueView<OperandListKey, const uint32_t> opListPair : m_operandLists)
		{
			rkit::Vector<data::ape::ExpressionValue> &outOpList = outOperandLists[opListPair.Value()];
			rkit::ConstSpan<data::ape::ExpressionValue> inOpList = opListPair.Key().GetOperands();

			RKIT_CHECK(outOpList.Resize(inOpList.Count()));

			rkit::CopySpan(outOpList.ToSpan(), inOpList);
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
	rkit::Result APECompilerHelper::IndexValue(uint32_t &outIndex, rkit::HashMap<TKey, uint32_t> &hashMap, TKey &&key)
	{
		const rkit::HashValue_t hashValue = rkit::Hasher<TKey>::ComputeHash(0, key);
		return IndexValue(outIndex, hashMap, hashValue, std::move(key));
	}

	template<class TKey>
	rkit::Result APECompilerHelper::IndexValue(uint32_t &outIndex, rkit::HashMap<TKey, uint32_t> &hashMap, rkit::HashValue_t hashValue, TKey &&key)
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

	rkit::ConstSpan<data::ape::ExpressionValue> APECompilerContext::OperandListKey::GetOperands() const
	{
		return m_operands.ToSpan();
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

	const data::ape::Expression &APECompilerContext::ExpressionKey::GetExpression() const
	{
		return m_expr;
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
		APEBlob blob;

		{
			rkit::CIPath analysisPath;
			RKIT_CHECK(FormatAnalysisPath(analysisPath, depsNode->GetIdentifier()));

			rkit::UniquePtr<rkit::ISeekableReadStream> inFile;
			RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, analysisPath, inFile));

			RKIT_CHECK(ReadAPEFile(*inFile, blob));
		}

		rkit::HashMap<rkit::data::ContentID, uint32_t> contentIDToIndex;
		rkit::HashMap<rkit::ByteString, uint32_t> stringToIndex;

		{
			uint32_t strIndex = 0;
			for (const rkit::ByteString &bstr : blob.m_strings)
			{
				RKIT_CHECK(stringToIndex.Set(bstr, strIndex++));
			}
		}

		if (blob.m_materialNames.Count())
		{
			for (const rkit::CIPath &materialPath : blob.m_materialNames)
			{
				rkit::CIPath materialOutputPath;
				RKIT_CHECK(MaterialCompiler::ConstructOutputPath(materialOutputPath, data::MaterialResourceType::kInterface, materialPath.ToString()));

				rkit::data::ContentID cid;
				RKIT_CHECK(feedback->IndexCAS(rkit::buildsystem::BuildFileLocation::kIntermediateDir, materialOutputPath, cid));

				uint32_t cidIndex = 0;
				RKIT_CHECK(APECompilerHelper::IndexValue<rkit::data::ContentID>(cidIndex, contentIDToIndex, std::move(cid)));
				RKIT_CHECK(blob.m_materialNameLookups.Append(rkit::endian::LittleUInt32_t(cidIndex)));
			}

			blob.m_materialNames.Reset();
		}

		if (blob.m_materialWildcards.Count())
		{
			rkit::HashMap<rkit::data::ContentID, size_t> contentToIndex;


			rkit::CIPath extraDepsPath;
			RKIT_CHECK(FormatExtraDepsPath(extraDepsPath, depsNode->GetIdentifier()));

			rkit::buildsystem::IDependencyNode *depsFileNode = nullptr;
			for (const rkit::buildsystem::NodeDependencyInfo &depsInfo : depsNode->GetNodeDependencies())
			{
				rkit::buildsystem::IDependencyNode *candidate = depsInfo.m_node;
				if (candidate->GetDependencyNodeNamespace() == rkit::buildsystem::kDefaultNamespace
					&& candidate->GetDependencyNodeType() == rkit::buildsystem::kDepsNodeID
					&& extraDepsPath.ToString() == candidate->GetIdentifier())
				{
					depsFileNode = candidate;
					break;
				}
			}

			if (depsFileNode == nullptr)
				RKIT_THROW(rkit::ResultCode::kInternalError);

			bool anyExists = false;
			for (const rkit::buildsystem::FileDependencyInfoView &depsView : depsFileNode->GetAnalysisFileDependencies())
			{
				if (depsView.m_fileExists && depsView.m_status.m_location == rkit::buildsystem::BuildFileLocation::kSourceDir)
				{
					anyExists = true;
					rkit::StringView matPath = depsView.m_status.m_filePath.ToStringView();

					rkit::CIPath materialOutputPath;
					RKIT_CHECK(MaterialCompiler::ConstructOutputPath(materialOutputPath, data::MaterialResourceType::kInterface, depsView.m_status.m_filePath.ToStringView()));

					rkit::data::ContentID cid;
					RKIT_CHECK(feedback->IndexCAS(rkit::buildsystem::BuildFileLocation::kIntermediateDir, materialOutputPath, cid));

					uint32_t cidIndex = 0;
					RKIT_CHECK(APECompilerHelper::IndexValue<rkit::data::ContentID>(cidIndex, contentIDToIndex, std::move(cid)));

					uint32_t nameIndex = 0;
					{
						rkit::ByteString matString;
						RKIT_CHECK(matString.Set(matPath.RemoveEncoding()));

						RKIT_CHECK(APECompilerHelper::IndexValue<rkit::ByteString>(nameIndex, stringToIndex, std::move(matString)));
					}

					data::ape::MaterialWildcardLookup wildcardLookup;
					wildcardLookup.m_stringIndex = nameIndex;
					wildcardLookup.m_materialContentIndex = cidIndex;
					RKIT_CHECK(blob.m_materialWildcardLookups.Append(wildcardLookup));
				}
			}

			if (!anyExists)
				RKIT_THROW(rkit::ResultCode::kDataError);

			blob.m_materialWildcards.Reset();
		}

		RKIT_CHECK(blob.m_materialContentIDs.Resize(contentIDToIndex.Count()));
		RKIT_CHECK(blob.m_strings.Resize(stringToIndex.Count()));

		for (rkit::HashMapKeyValueView<rkit::ByteString, uint32_t> kvp : stringToIndex)
			blob.m_strings[kvp.Value()] = kvp.Key();

		for (rkit::HashMapKeyValueView<rkit::data::ContentID, uint32_t> kvp : contentIDToIndex)
			blob.m_materialContentIDs[kvp.Value()] = kvp.Key();

		{
			rkit::CIPath outPath;
			RKIT_CHECK(FormatOutputPath(outPath, depsNode->GetIdentifier()));

			{
				rkit::UniquePtr<rkit::ISeekableReadWriteStream> outFile;
				RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outPath, outFile));

				RKIT_CHECK(DumpAPEFile(*outFile, blob));
			}
		}

		RKIT_RETURN_OK;
	}

	rkit::Result APEScriptCompilerImpl::CompileWindow(APECompilerContext &ctx, CompiledWindowDef &compiledWindow, const WindowDef &wdef)
	{
		compiledWindow.m_windowID = wdef.m_windowID;

		VectorMemoryStream stream(compiledWindow.m_commandStream);

		APEWriter writer(ctx, stream);

		for (const rkit::UniquePtr<ape_parse::WindowCommand> &cmdPtr : wdef.m_commands)
		{
			const ape_parse::WindowCommand &cmd = *cmdPtr;

			RKIT_CHECK(compiledWindow.m_commandStream.Append(static_cast<uint8_t>(cmd.GetCommandType())));
			RKIT_CHECK(cmd.Write(writer));
		}

		RKIT_RETURN_OK;
	}

	rkit::Result APEScriptCompilerImpl::CompileSwitch(APECompilerContext &ctx, CompiledSwitchDef &compiledSwitch, const SwitchDef &switchDef)
	{
		compiledSwitch.m_switchID = switchDef.m_switchID;

		rkit::HashMap<uint64_t, const ape_parse::SwitchCommand *> switchCmdTree;

		for (const ape_parse::SwitchCommand &inCmd : switchDef.m_commands)
		{
			RKIT_CHECK(switchCmdTree.Set(inCmd.m_cc, &inCmd));
		}

		return CompileBasicBlock(ctx, compiledSwitch.m_commands, switchCmdTree, 1);
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

	rkit::Result APEScriptCompilerImpl::DumpAPEFile(rkit::IWriteStream &stream, const APEBlob &blob)
	{
		{
			data::ape::APEScriptCatalog catalog;

			catalog.m_numStrings = static_cast<uint32_t>(blob.m_strings.Count());
			catalog.m_numExprs = static_cast<uint32_t>(blob.m_exprs.Count());
			catalog.m_numOperandLists = static_cast<uint32_t>(blob.m_operandLists.Count());
			catalog.m_numWindows = static_cast<uint32_t>(blob.m_windows.Count());
			catalog.m_numSwitches = static_cast<uint32_t>(blob.m_switches.Count());
			catalog.m_numMaterialWildcards = static_cast<uint32_t>(blob.m_materialWildcards.Count());
			catalog.m_numMaterialNames = static_cast<uint32_t>(blob.m_materialNames.Count());
			catalog.m_numMaterialContentIDs = static_cast<uint32_t>(blob.m_materialContentIDs.Count());
			catalog.m_numMaterialNameLookups = static_cast<uint32_t>(blob.m_materialNameLookups.Count());
			catalog.m_numMaterialWildcardLookups = static_cast<uint32_t>(blob.m_materialWildcardLookups.Count());

			RKIT_CHECK(stream.WriteOneBinary(catalog));
		}

		for (const rkit::ByteString &str : blob.m_strings)
		{
			rkit::endian::LittleUInt32_t strLength = rkit::endian::LittleUInt32_t(str.Length());
			RKIT_CHECK(stream.WriteOneBinary(strLength));
		}

		for (const rkit::ByteString &str : blob.m_strings)
		{
			RKIT_CHECK(stream.WriteAllSpan(str.ToSpan()));
		}

		RKIT_CHECK(stream.WriteAllSpan(blob.m_exprs.ToSpan()));

		for (const rkit::Vector<data::ape::ExpressionValue> &opList : blob.m_operandLists)
		{
			rkit::endian::LittleUInt32_t opListCount = rkit::endian::LittleUInt32_t(opList.Count());
			RKIT_CHECK(stream.WriteOneBinary(opListCount));
		}

		for (const rkit::Vector<data::ape::ExpressionValue> &opList : blob.m_operandLists)
		{
			RKIT_CHECK(stream.WriteAllSpan(opList.ToSpan()));
		}

		for (size_t windowIndex = 0; windowIndex < blob.m_windows.Count(); windowIndex++)
		{
			data::ape::Window window = {};
			window.m_commandStreamLength = static_cast<uint32_t>(blob.m_windows[windowIndex].m_commandStream.Count());
			window.m_windowID = blob.m_windows[windowIndex].m_windowID;
			RKIT_CHECK(stream.WriteOneBinary(window));
		}

		for (const CompiledWindowDef &window : blob.m_windows)
		{
			RKIT_CHECK(stream.WriteAllSpan(window.m_commandStream.ToSpan()));
		}

		for (size_t switchIndex = 0; switchIndex < blob.m_switches.Count(); switchIndex++)
		{
			data::ape::Switch sw = {};
			sw.m_numCommands = static_cast<uint32_t>(blob.m_switches[switchIndex].m_commands.Count());
			sw.m_switchID = blob.m_switches[switchIndex].m_switchID;
			RKIT_CHECK(stream.WriteOneBinary(sw));
		}

		for (const CompiledSwitchDef &sw : blob.m_switches)
		{
			RKIT_CHECK(stream.WriteAllSpan(sw.m_commands.ToSpan()));
		}

		for (const rkit::String &str : blob.m_materialWildcards)
		{
			rkit::endian::LittleUInt32_t strLengthData(static_cast<uint32_t>(str.Length()));
			RKIT_CHECK(stream.WriteOneBinary(strLengthData));
			RKIT_CHECK(stream.WriteAllSpan(str.ToSpan()));
		}

		for (const rkit::CIPath &str : blob.m_materialNames)
		{
			rkit::endian::LittleUInt32_t strLengthData(static_cast<uint32_t>(str.Length()));
			RKIT_CHECK(stream.WriteOneBinary(strLengthData));
			RKIT_CHECK(stream.WriteAllSpan(str.ToString().ToSpan()));
		}

		RKIT_CHECK(stream.WriteAllSpan(blob.m_materialContentIDs.ToSpan()));
		RKIT_CHECK(stream.WriteAllSpan(blob.m_materialNameLookups.ToSpan()));
		RKIT_CHECK(stream.WriteAllSpan(blob.m_materialWildcardLookups.ToSpan()));

		RKIT_RETURN_OK;
	}

	rkit::Result APEScriptCompilerImpl::ReadAPEFile(rkit::IReadStream &stream, APEBlob &blob)
	{
		data::ape::APEScriptCatalog catalog;

		RKIT_CHECK(stream.ReadOneBinary(catalog));

		const size_t numStrings = catalog.m_numStrings.Get();
		RKIT_CHECK(blob.m_strings.Resize(numStrings));

		rkit::Vector<rkit::ByteStringConstructionBuffer> stringCBufs;
		RKIT_CHECK(stringCBufs.Resize(numStrings));

		RKIT_CHECK(blob.m_operandLists.Resize(catalog.m_numOperandLists.Get()));
		RKIT_CHECK(blob.m_windows.Resize(catalog.m_numWindows.Get()));
		RKIT_CHECK(blob.m_switches.Resize(catalog.m_numSwitches.Get()));
		RKIT_CHECK(blob.m_exprs.Resize(catalog.m_numExprs.Get()));
		RKIT_CHECK(blob.m_materialContentIDs.Resize(catalog.m_numMaterialContentIDs.Get()));
		RKIT_CHECK(blob.m_materialNames.Resize(catalog.m_numMaterialNames.Get()));
		RKIT_CHECK(blob.m_materialNameLookups.Resize(catalog.m_numMaterialNameLookups.Get()));
		RKIT_CHECK(blob.m_materialWildcardLookups.Resize(catalog.m_numMaterialWildcardLookups.Get()));
		RKIT_CHECK(blob.m_materialWildcards.Resize(catalog.m_numMaterialWildcards.Get()));

		for (rkit::ByteStringConstructionBuffer &strCBuf : stringCBufs)
		{
			rkit::endian::LittleUInt32_t strLength;
			RKIT_CHECK(stream.ReadOneBinary(strLength));

			RKIT_CHECK(strCBuf.Allocate(strLength.Get()));
		}

		for (rkit::ByteStringConstructionBuffer &strCBuf : stringCBufs)
		{
			RKIT_CHECK(stream.ReadAllSpan(strCBuf.GetSpan()));
		}

		rkit::ProcessParallelSpans(blob.m_strings.ToSpan(), stringCBufs.ToSpan(), [](rkit::ByteString &str, rkit::ByteStringConstructionBuffer &cbuf)
			{
				str = rkit::ByteString(std::move(cbuf));
			});

		RKIT_CHECK(stream.ReadAllSpan(blob.m_exprs.ToSpan()));

		for (rkit::Vector<data::ape::ExpressionValue> &opList : blob.m_operandLists)
		{
			rkit::endian::LittleUInt32_t opListCount;
			RKIT_CHECK(stream.ReadOneBinary(opListCount));
			RKIT_CHECK(opList.Resize(opListCount.Get()));
		}

		for (rkit::Vector<data::ape::ExpressionValue> &opList : blob.m_operandLists)
		{
			RKIT_CHECK(stream.ReadAllSpan(opList.ToSpan()));
		}

		for (size_t windowIndex = 0; windowIndex < blob.m_windows.Count(); windowIndex++)
		{
			data::ape::Window window = {};
			RKIT_CHECK(stream.ReadOneBinary(window));

			blob.m_windows[windowIndex].m_windowID = window.m_windowID.Get();
			RKIT_CHECK(blob.m_windows[windowIndex].m_commandStream.Resize(window.m_commandStreamLength.Get()));
		}

		for (CompiledWindowDef &window : blob.m_windows)
		{
			RKIT_CHECK(stream.ReadAllSpan(window.m_commandStream.ToSpan()));
		}

		for (size_t switchIndex = 0; switchIndex < blob.m_switches.Count(); switchIndex++)
		{
			data::ape::Switch sw = {};
			RKIT_CHECK(stream.ReadOneBinary(sw));

			blob.m_switches[switchIndex].m_switchID = sw.m_switchID.Get();
			RKIT_CHECK(blob.m_switches[switchIndex].m_commands.Resize(sw.m_numCommands.Get()));
		}

		for (CompiledSwitchDef &sw : blob.m_switches)
		{
			RKIT_CHECK(stream.ReadAllSpan(sw.m_commands.ToSpan()));
		}

		auto readIntermediatePath = [&stream](rkit::String &outPath) -> rkit::Result
			{
				rkit::endian::LittleUInt32_t strLengthData;
				RKIT_CHECK(stream.ReadOneBinary(strLengthData));

				const size_t len = strLengthData.Get();

				rkit::Vector<rkit::Utf8Char_t> pathChars;
				RKIT_CHECK(pathChars.Resize(len));

				RKIT_CHECK(stream.ReadAllSpan(pathChars.ToSpan()));

				return outPath.Set(rkit::StringSliceView(pathChars.ToSpan()));
			};

		for (rkit::String &path : blob.m_materialWildcards)
		{
			RKIT_CHECK(readIntermediatePath(path));
		}

		for (rkit::CIPath &path : blob.m_materialNames)
		{
			rkit::String str;
			RKIT_CHECK(readIntermediatePath(str));
			RKIT_CHECK(path.Set(str));
		}

		RKIT_CHECK(stream.ReadAllSpan(blob.m_materialContentIDs.ToSpan()));
		RKIT_CHECK(stream.ReadAllSpan(blob.m_materialNameLookups.ToSpan()));
		RKIT_CHECK(stream.ReadAllSpan(blob.m_materialWildcardLookups.ToSpan()));

		RKIT_RETURN_OK;
	}

	rkit::Result APEScriptCompilerImpl::FormatAnalysisPath(rkit::CIPath &outPath, const rkit::StringView &identifier)
	{
		rkit::String formattedPath;
		RKIT_CHECK(formattedPath.Format(u8"ax_ape/a/{}", identifier));
		return outPath.Set(formattedPath);
	}

	rkit::Result APEScriptCompilerImpl::FormatExtraDepsPath(rkit::CIPath &outPath, const rkit::StringView &identifier)
	{
		rkit::String formattedPath;
		RKIT_CHECK(formattedPath.Format(u8"ax_ape/d/{}", identifier));
		return outPath.Set(formattedPath);
	}

	rkit::Result APEScriptCompilerImpl::FormatOutputPath(rkit::CIPath &outPath, const rkit::StringView &identifier)
	{
		rkit::String formattedPath;
		RKIT_CHECK(formattedPath.Format(u8"ax_ape/c/{}", identifier));
		return outPath.Set(formattedPath);
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
		return 8;
	}

	rkit::Result APEScriptCompiler::FormatOutputPath(rkit::CIPath &outPath, const rkit::StringView &identifier)
	{
		return APEScriptCompilerImpl::FormatOutputPath(outPath, identifier);
	}

	rkit::Result APEScriptCompiler::Create(rkit::UniquePtr<APEScriptCompiler> &outCompiler)
	{
		return rkit::New<APEScriptCompiler>(outCompiler);
	}

	rkit::Result APEGroupCompilerImpl::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::CIPath depsFilePath;
		RKIT_CHECK(ResolvePath(depsFilePath, depsNode->GetIdentifier(), feedback));

		RKIT_CHECK(feedback->AddNodeDependency(rkit::buildsystem::kDefaultNamespace, rkit::buildsystem::kDepsNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, depsFilePath.ToString()));

		RKIT_RETURN_OK;
	}

	rkit::Result APEGroupCompilerImpl::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::Vector<rkit::buildsystem::FileStatus> fileList;

		for (rkit::buildsystem::NodeDependencyInfo nodeDep : depsNode->GetNodeDependencies())
		{
			rkit::buildsystem::IDependencyNode *depsFileNode = nodeDep.m_node;

			for (rkit::buildsystem::NodeDependencyInfo depsNodeDep : nodeDep.m_node->GetNodeDependencies())
			{
				rkit::buildsystem::IDependencyNode *apeScriptNode = depsNodeDep.m_node;

				for (rkit::buildsystem::FileStatusView fsView : apeScriptNode->GetCompileProducts())
				{
					rkit::buildsystem::FileStatus fileInfo;
					RKIT_CHECK(fileInfo.Set(fsView));
					RKIT_CHECK(fileList.Append(std::move(fileInfo)));
				}
			}
		}

		rkit::QuickSort(fileList.begin(), fileList.end(), [](const rkit::buildsystem::FileStatus &a, const rkit::buildsystem::FileStatus &b)
			{
				return a.m_filePath.ToString() < b.m_filePath.ToString();
			});

		for (size_t i = 1; i < fileList.Count(); )
		{
			if (fileList[i - 1].m_filePath == fileList[i].m_filePath)
				fileList.RemoveAtIndex(i);
			else
				i++;
		}

		rkit::Vector<rkit::data::ContentID> contentIDs;

		for (const rkit::buildsystem::FileStatus &fileStatus : fileList)
		{
			rkit::data::ContentID cid;
			RKIT_CHECK(feedback->IndexCAS(fileStatus.m_location, fileStatus.m_filePath, cid));
			RKIT_CHECK(contentIDs.Append(cid));
		}

		rkit::String outPathStr;
		RKIT_CHECK(outPathStr.Format(u8"ax_ape/g/{}", depsNode->GetIdentifier()));

		rkit::CIPath outPath;
		RKIT_CHECK(outPath.Set(outPathStr));

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> outFile;
		RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outPath, outFile));

		RKIT_CHECK(outFile->WriteAllSpan(contentIDs.ToSpan()));

		RKIT_RETURN_OK;
	}

	rkit::Result APEGroupCompilerImpl::ResolvePath(rkit::CIPath &depsFilePath, const rkit::StringView &groupNodeIdentifier, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::CIPath groupPath;
		RKIT_CHECK(groupPath.Set(groupNodeIdentifier));

		rkit::UniquePtr<rkit::ISeekableReadStream> inFile;
		RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, groupPath, inFile));

		const rkit::FilePos_t fileSize = inFile->GetSize();

		rkit::Vector<rkit::Utf8Char_t> pathBytes;
		if (inFile->GetSize() > std::numeric_limits<size_t>::max())
			RKIT_THROW(rkit::ResultCode::kDataError);

		RKIT_CHECK(pathBytes.Resize(static_cast<size_t>(fileSize)));

		RKIT_CHECK(inFile->ReadAllSpan(pathBytes.ToSpan()));

		rkit::StringSliceView slView(pathBytes.ToSpan());
		if (!slView.Validate())
			RKIT_THROW(rkit::ResultCode::kDataError);

		return depsFilePath.Set(rkit::StringSliceView(pathBytes.ToSpan()));
	}

	bool APEGroupCompiler::HasAnalysisStage() const
	{
		return true;
	}

	rkit::Result APEGroupCompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return Impl().RunAnalysis(depsNode, feedback);
	}

	rkit::Result APEGroupCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return Impl().RunCompile(depsNode, feedback);
	}

	uint32_t APEGroupCompiler::GetVersion() const
	{
		return 3;
	}

	rkit::Result APEGroupCompiler::Create(rkit::UniquePtr<APEGroupCompiler> &outCompiler)
	{
		return rkit::New<APEGroupCompiler>(outCompiler);
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::buildsystem::APEScriptCompilerImpl)
RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::buildsystem::APEGroupCompilerImpl)
