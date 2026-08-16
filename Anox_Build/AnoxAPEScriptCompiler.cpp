#include "AnoxAPEScriptCompiler.h"
#include "AnoxAPEWindowCommandData.generated.h"

#include "anox/Build/NodeIDs.h"

#include "anox/Data/APEScript.h"
#include "anox/Data/ResourceTypeCodes.h"

#include "anox/AnoxModule.h"
#include "anox/Label.h"

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/NoCopy.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/QuickSort.h"

#include "rkit/BuildSystem/BuildSystem.h"
#include "rkit/Data/ContentID.h"
#include "rkit/Core/Pair.h"

#include "AnoxMaterialCompiler.h"
#include "AnoxSceneCompiler.h"

#include "APEExternMetadata.generated.inl"

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

	struct APEResourceRefKey
	{
		uint32_t m_resNamespace = 0;
		uint32_t m_resType = 0;
		uint32_t m_nameStringIndex = 0;

		bool operator==(const APEResourceRefKey &other) const = default;
	};
}

template<>
struct rkit::Hasher<anox::buildsystem::APEResourceRefKey> : public rkit::BinaryHasher<anox::buildsystem::APEResourceRefKey>
{
};

namespace anox::buildsystem
{
	enum class APEIntermediateResourceType : uint32_t
	{
		kRawFile,
		kScene,
		kMaterial,
	};

	class APECompilerContext
	{
	public:
		explicit APECompilerContext(rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

		rkit::Result IndexExpression(uint32_t &outIndex, data::ape::Expression &&expr);
		rkit::Result IndexOperandList(uint32_t &outIndex, rkit::Vector<data::ape::ExpressionValue> &&operands);
		rkit::Result IndexString(uint32_t &outIndex, const rkit::ByteString &str);
		rkit::Result IndexResource(uint32_t &outIndex, APEIntermediateResourceType intermediateResourceType,
			const rkit::StringView &prefix, const rkit::ByteStringSliceView &arg);

		rkit::Result ConvertOptionalExprValue(data::ape::ExpressionValue &outExprValue, const rkit::Optional<ape_parse::ExpressionValue> &value);
		rkit::Result ConvertExprValue(uint32_t &outIndex, data::ape::OperandType &outOperandType, bool &outIsString, const ape_parse::ExpressionValue &expr);
		rkit::Result ConvertOptionalByteString(uint32_t &outDWord, const rkit::Optional<rkit::ByteString> &value);
		rkit::Result ConvertByteString(uint32_t &outDWord, const rkit::ByteString &value);
		rkit::Result ConvertFormattingValue(uint32_t &outDWord, const ape_parse::FormattingValue &value);
		rkit::Result ConvertOperand(uint32_t &outIndex, data::ape::OperandType &outOperandType, bool &outIsString, const ape_parse::Operand &operand);
		rkit::Result ConvertMaterial(data::ape::ResourceReference &outMaterialRef, const rkit::ByteString &bstr);

		rkit::Result DumpResults(rkit::Vector<rkit::Vector<data::ape::ExpressionValue>> &outOperandLists,
			rkit::Vector<data::ape::Expression> &outExprs, rkit::Vector<rkit::ByteString> &outStrings,
			rkit::Vector<data::ape::ResourceIdentifier> &outResourceIdentifiers) const;

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
		rkit::HashMap<APEResourceRefKey, uint32_t> m_resourceIDs;

		rkit::buildsystem::IDependencyNodeCompilerFeedback *m_feedback = nullptr;
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


	class APEDepsCompilerImpl : public rkit::OpaqueImplementation<APEDepsCompiler>
	{
	public:
		friend class APEDepsCompilerImpl;

		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
	};

	class APEScriptCompilerImpl : public rkit::OpaqueImplementation<APEScriptCompiler>
	{
	public:
		friend class APEScriptCompiler;

		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

		static rkit::Result PostNodeCompileTask(APEIntermediateResourceType resType, const rkit::StringView &pathStr, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
		static rkit::Result IndexNodeCompileResult(rkit::data::ContentID &outContentID, uint32_t &outResNamespace, uint32_t &outResType, APEIntermediateResourceType resType, const rkit::ByteStringView &pathStr, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

	private:
		enum class DepResourceType
		{
			Script,
			Image,
			Texture,
			Sound,
			Font,
			MusicSeg,
			Particle,
			Music,
			Scene,
			Model,
			File,
			Invalid,
		};

		typedef rkit::BasePath<false, rkit::DefaultPathTraits<rkit::PathTraitFlags::kIsCaseInsensitive | rkit::PathTraitFlags::kAllowWildcards>> WildcardPath_t;

		struct DynamicResourceDef
		{
			rkit::AsciiString m_path;
		};

		struct DynamicResourceCategory
		{
			rkit::Vector<DynamicResourceDef> m_defs;
		};

		struct DynamicResourcesDictionary
		{
			rkit::HashMap<rkit::AsciiString, DynamicResourceCategory> m_resCategories;
		};

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
			rkit::Vector<data::ape::ResourceIdentifier> m_resourceIDs;
		};

		static rkit::Result CompileWindow(APECompilerContext &ctx, CompiledWindowDef &compiledWindow, const WindowDef &wdef);
		static rkit::Result CompileSwitch(APECompilerContext &ctx, CompiledSwitchDef &compiledSwitch, const SwitchDef &switchDef);
		static rkit::Result CompileBasicBlock(APECompilerContext &ctx, rkit::Vector<data::ape::SwitchCommand> &cmdStream, const rkit::HashMap<uint64_t, const ape_parse::SwitchCommand *> &tree, uint64_t firstCC);
		static rkit::Result CompileExtern(APECompilerContext &ctx, uint32_t &outOpcode, uint32_t &outArgList, const rkit::ByteStringView &str);
		static bool IsTerminalCC(uint64_t cc);

		static rkit::Result DumpAPEFile(rkit::IWriteStream &stream, const APEBlob &blob);

		static rkit::Result ReadAPEFile(rkit::IReadStream &stream, APEBlob& blob);

		static rkit::Result ReadDepsCatalog(DynamicResourcesDictionary &resDict, const rkit::StringView &identifier, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

		static rkit::Result FormatExtraDepsPath(rkit::CIPath &outPath, const rkit::StringView &identifier);
		static rkit::Result FormatAnalysisPath(rkit::CIPath &outPath, const rkit::StringView &identifier);
		static rkit::Result FormatOutputPath(rkit::CIPath &outPath, const rkit::StringView &identifier);

		static rkit::Result ResolveDepResourceType(DepResourceType &resType, const rkit::AsciiStringSliceView &categoryStr);
		static rkit::Result NormalizeResourcePath(rkit::AsciiString &outString, DepResourceType resType, const rkit::AsciiStringSliceView &categoryStr);

		template<class TFunc>
		static rkit::Result ForEachDependency(const DynamicResourcesDictionary &dict, const TFunc &func);
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
		m_context.ConvertOptionalExprValue(expr, value);
		return m_stream.WriteOneBinary(expr);
	}

	rkit::Result APEWriter::Write(const rkit::Optional<rkit::ByteString> &value)
	{
		uint32_t index = 0;
		m_context.ConvertOptionalByteString(index, value);
		rkit::endian::LittleUInt32_t indexData = rkit::endian::LittleUInt32_t(index);

		return m_stream.WriteOneBinary(indexData);
	}

	rkit::Result APEWriter::Write(const rkit::ByteString &value)
	{
		uint32_t index = 0;
		IndexString(index, 0, value);
		return Write(index);
	}

	rkit::Result APEWriter::Write(const ape_parse::FormattingValue &value)
	{
		uint32_t index = 0;
		m_context.ConvertFormattingValue(index, value);
		rkit::endian::LittleUInt32_t indexData = rkit::endian::LittleUInt32_t(index);
		return m_stream.WriteOneBinary(indexData);
	}

	rkit::Result APEWriter::Write(const ape_parse::TextureID &value)
	{
		data::ape::ResourceReference matRef = {};

		rkit::ByteString bstr = value.m_str;
		if (bstr.StartsWith(rkit::StringSliceView(u8"../").RemoveEncoding()))
		{
			bstr.Set(bstr.SubString(3, bstr.Length() - 3));
		}
		else
		{
			rkit::ByteString adjusted;
			adjusted.Set(rkit::StringSliceView(u8"gameflow/").RemoveEncoding());
			adjusted.Append(bstr);

			bstr = std::move(adjusted);
		}

		m_context.ConvertMaterial(matRef, bstr);
		return m_stream.WriteOneBinary(matRef);
	}

	rkit::Result APEWriter::Write(const ape_parse::WindowStyleID &value)
	{
		data::ape::ResourceReference matRef = {};

		if (value.m_str.EqualsNoCase(rkit::StringView(u8"null").RemoveEncoding()))
		{
			matRef.m_index = 0;
			matRef.m_refType = data::ape::ResourceReferenceType::Null;
		}
		else
		{
			rkit::ByteString adjusted;
			adjusted.Set(rkit::StringSliceView(u8"graphics/interface/windows/").RemoveEncoding());
			adjusted.Append(value.m_str);

			m_context.ConvertMaterial(matRef, adjusted);
		}

		return m_stream.WriteOneBinary(matRef);
	}

	rkit::Result APEWriter::IndexString(uint32_t &outIndex, uint32_t baseIndex, const rkit::ByteString &value)
	{
		uint32_t ctxIndex = 0;
		m_context.IndexString(ctxIndex, value);

		if (ctxIndex >= std::numeric_limits<uint32_t>::max() - baseIndex)
			RKIT_THROW(rkit::ResultCode::kDataError);

		outIndex = ctxIndex + baseIndex;

		RKIT_RETURN_OK;
	}


	rkit::Result APEDepsCompilerImpl::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result APEDepsCompilerImpl::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result APEScriptCompilerImpl::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		DynamicResourcesDictionary depsDict;
		ReadDepsCatalog(depsDict, depsNode->GetIdentifier(), feedback);

		auto depsProcessFunc = [feedback](const rkit::AsciiStringSliceView &categoryName, const DynamicResourceCategory &category) -> rkit::Result
			{
				DepResourceType resType = DepResourceType::Invalid;

				ResolveDepResourceType(resType, categoryName);

				for (const DynamicResourceDef &resDef : category.m_defs)
				{
					rkit::AsciiString normalizedPath;
					NormalizeResourcePath(normalizedPath, resType, resDef.m_path);

					switch (resType)
					{
					case DepResourceType::Image:
						feedback->AddNodeDependency(kAnoxNamespaceID, kInterfaceMaterialNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, normalizedPath.ToUTF8View());
						break;
					default:
						RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
					}
				}

				RKIT_RETURN_OK;
			};

		ForEachDependency(depsDict, depsProcessFunc);

		rkit::CIPath path;
		path.Set(depsNode->GetIdentifier());

		rkit::UniquePtr<rkit::ISeekableReadStream> inputFile;
		feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, path, inputFile);

		rkit::Vector<WindowDef> windowDefs;
		rkit::Vector<SwitchDef> switchDefs;

		{
			rkit::endian::LittleUInt64_t header;
			inputFile->ReadOneBinary(header);

			if (header.Get() != 0xffffffff0000013d)
			{
				rkit::log::Error(u8"Invalid APE header");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			// Load windows
			for (;;)
			{
				rkit::endian::LittleUInt32_t windowID;
				inputFile->ReadOneBinary(windowID);

				if (windowID.Get() == 0)
					break;

				WindowDef windowDef;
				windowDef.m_windowID = windowID.Get();

				for (;;)
				{
					uint8_t opcode = 0;
					inputFile->ReadOneBinary(opcode);

					rkit::UniquePtr<ape_parse::WindowCommand> cmd;
					ape_parse::CreateWindowCommand(cmd, opcode);

					ape_parse::APEReader reader(*inputFile);
					cmd->Parse(reader);

					if (cmd->GetCommandType() == data::WindowCommandType::End)
						break;

					windowDef.m_commands.Append(std::move(cmd));
				}

				windowDefs.Append(std::move(windowDef));
			}

			// Load switches
			{
				ape_parse::APEReader reader(*inputFile);
				uint32_t switchMarker = 0;
				reader.Read(switchMarker);
				if (switchMarker != 0xfffffffeu)
					RKIT_THROW(rkit::ResultCode::kDataError);

				for (;;)
				{
					uint32_t switchLabel = 0;
					reader.Read(switchLabel);
					if (switchLabel == 0)
						break;

					SwitchDef switchDef;
					switchDef.m_switchID = switchLabel;

					for (;;)
					{
						ape_parse::SwitchCommand cmd;
						reader.Read(cmd.m_cc);
						reader.Read(cmd.m_opcode);

						if (cmd.m_opcode > 21)
						{
							if (cmd.m_opcode == 69)
								break;	// End

							RKIT_THROW(rkit::ResultCode::kDataError);
						}

						reader.Read(cmd.m_str);
						reader.Read(cmd.m_fmt);
						reader.Read(cmd.m_expr);
						switchDef.m_commands.Append(std::move(cmd));
					}

					switchDefs.Append(std::move(switchDef));
				}
			}
		}

		APECompilerContext compilerCtx(feedback);

		APEBlob blob;

		blob.m_windows.Resize(windowDefs.Count());

		for (size_t windowIndex = 0; windowIndex < windowDefs.Count(); windowIndex++)
		{
			CompileWindow(compilerCtx, blob.m_windows[windowIndex], windowDefs[windowIndex]);
		}

		blob.m_switches.Resize(switchDefs.Count());

		for (size_t switchIndex = 0; switchIndex < switchDefs.Count(); switchIndex++)
		{
			CompileSwitch(compilerCtx, blob.m_switches[switchIndex], switchDefs[switchIndex]);
		}

		compilerCtx.DumpResults(blob.m_operandLists, blob.m_exprs, blob.m_strings, blob.m_resourceIDs);

		for (const data::ape::ResourceIdentifier &rid : blob.m_resourceIDs)
		{
			const rkit::ByteString& str = blob.m_strings[rid.m_nameIndex.Get()];

			if (!rkit::CharacterEncodingValidator<rkit::CharacterEncoding::kASCII>::ValidateSpan(str.ToSpan()))
			{
				rkit::log::ErrorFmt(u8"Invalid resource path {}", str);
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			rkit::CIPath path;
			path.SetFromUTF8(rkit::ByteStringView(str).ToUTF8Unsafe());

			PostNodeCompileTask(static_cast<APEIntermediateResourceType>(rid.m_resType.Get()), path.ToString(), feedback);
		}

		{
			rkit::CIPath outPath;
			FormatAnalysisPath(outPath, depsNode->GetIdentifier());

			rkit::UniquePtr<rkit::ISeekableReadWriteStream> outFile;
			feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outPath, outFile);

			DumpAPEFile(*outFile, blob);
		}

		RKIT_RETURN_OK;
	}

	APECompilerContext::APECompilerContext(rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
		: m_feedback(feedback)
	{
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

	rkit::Result APECompilerContext::IndexResource(uint32_t &outIndex, APEIntermediateResourceType resType,
		const rkit::StringView &prefix, const rkit::ByteStringSliceView &argRef)
	{
		rkit::StringSliceView arg = argRef.ToUTF8Unsafe();

		if (!arg.Validate())
			RKIT_THROW(rkit::ResultCode::kInvalidUnicode);

		if (arg.StartsWith(u8"\""))
			arg = arg.SubString(1);
		if (arg.EndsWith(u8"\""))
			arg = arg.SubString(0, arg.Length() - 1);

		rkit::String formattedPathStr;
		formattedPathStr.Format(u8"{}{}", prefix, arg);

		rkit::CIPath path;
		path.Set(formattedPathStr);

		rkit::ByteString pathBStr;
		pathBStr.Set(path.ToString().ToByteView());

		uint32_t pathIndex = 0;
		APECompilerHelper::IndexValue<rkit::ByteString>(pathIndex, m_strings, std::move(pathBStr));

		APEResourceRefKey refKey = {};
		refKey.m_resNamespace = 0;
		refKey.m_resType = static_cast<uint32_t>(resType);
		refKey.m_nameStringIndex = pathIndex;

		APECompilerHelper::IndexValue<APEResourceRefKey>(outIndex, m_resourceIDs, std::move(refKey));

		RKIT_RETURN_OK;
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
			ConvertExprValue(index, operandType, isString, value.Get());

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
			ConvertOperand(leftIndex, leftOpType, leftIsString, *expr.m_left);
			ConvertOperand(rightIndex, rightOpType, rightIsString, *expr.m_right);
			if (leftIsString || rightIsString)
				RKIT_THROW(rkit::ResultCode::kDataError);

			op = static_cast<data::ape::Operator>(expr.m_operator);
			break;
		case ape_parse::ExpressionValue::Operator::Eq:
		case ape_parse::ExpressionValue::Operator::Neq:
			ConvertOperand(leftIndex, leftOpType, leftIsString, *expr.m_left);
			ConvertOperand(rightIndex, rightOpType, rightIsString, *expr.m_right);
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
		IndexExpression(exprIndex, std::move(resultExpr));

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
			IndexString(outIndex, static_cast<const ape_parse::FloatVariableNameOperand &>(operand).m_value);
			outIsString = false;
			outOperandType = data::ape::OperandType::Variable;
			break;
		case ape_parse::OperandType::StringLiteral:
			IndexString(outIndex, static_cast<const ape_parse::StringOperand &>(operand).m_value);
			outIsString = true;
			outOperandType = data::ape::OperandType::Literal;
			break;
		case ape_parse::OperandType::StringVariable:
			IndexString(outIndex, static_cast<const ape_parse::StringVariableNameOperand &>(operand).m_value);
			outIsString = true;
			outOperandType = data::ape::OperandType::Variable;
			break;
		default:
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		RKIT_RETURN_OK;
	}

	rkit::Result APECompilerContext::ConvertMaterial(data::ape::ResourceReference &outMaterialRef, const rkit::ByteString &pathBStr)
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


		if (!isWildcard)
		{
			uint32_t resIndex = 0;

			IndexResource(resIndex, APEIntermediateResourceType::kMaterial, rkit::StringView(), pathBStr);

			outMaterialRef.m_refType = data::ape::ResourceReferenceType::ResourceID;
			outMaterialRef.m_index = resIndex;
		}
		else
		{
			uint32_t pathStrIndex = 0;
			APECompilerHelper::IndexValue<rkit::ByteString>(pathStrIndex, m_strings, rkit::ByteString(pathBStr));

			outMaterialRef.m_refType = data::ape::ResourceReferenceType::WildcardString;
			outMaterialRef.m_index = pathStrIndex;
		}

		RKIT_RETURN_OK;
	}


	rkit::Result APECompilerContext::DumpResults(rkit::Vector<rkit::Vector<data::ape::ExpressionValue>> &outOperandLists,
		rkit::Vector<data::ape::Expression> &outExprs, rkit::Vector<rkit::ByteString> &outStrings,
		rkit::Vector<data::ape::ResourceIdentifier> &outResourceIdentifiers) const
	{
		outOperandLists.Resize(m_operandLists.Count());
		outExprs.Resize(m_expressions.Count());
		outStrings.Resize(m_strings.Count());
		outResourceIdentifiers.Resize(m_resourceIDs.Count());


		for (rkit::HashMapKeyValueView<ExpressionKey, const uint32_t> exprPair : m_expressions)
			outExprs[exprPair.Value()] = exprPair.Key().GetExpression();

		for (rkit::HashMapKeyValueView<rkit::ByteString, const uint32_t> strPair : m_strings)
			outStrings[strPair.Value()] = strPair.Key();

		for (rkit::HashMapKeyValueView<OperandListKey, const uint32_t> opListPair : m_operandLists)
		{
			rkit::Vector<data::ape::ExpressionValue> &outOpList = outOperandLists[opListPair.Value()];
			rkit::ConstSpan<data::ape::ExpressionValue> inOpList = opListPair.Key().GetOperands();

			outOpList.Resize(inOpList.Count());

			rkit::CopySpan(outOpList.ToSpan(), inOpList);
		}

		for (rkit::HashMapKeyValueView<APEResourceRefKey, const uint32_t> rrPair : m_resourceIDs)
		{
			const APEResourceRefKey &rrKey = rrPair.Key();

			data::ape::ResourceIdentifier &rid = outResourceIdentifiers[rrPair.Value()];

			rid.m_contentID = rkit::data::ContentID();
			rid.m_nameIndex = rrKey.m_nameStringIndex;
			rid.m_resNamespace = rrKey.m_resNamespace;
			rid.m_resType = rrKey.m_resType;
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
			IndexString(index, value.Get());
			rkit::SafeAdd<uint32_t>(outDWord, index, 1);
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
				ConvertByteString(index, static_cast<const ape_parse::FloatVariableNameOperand &>(inOperand).m_value);
				break;
			case ape_parse::OperandType::StringLiteral:
				exprType = data::ape::ExprType::StringLiteral;
				ConvertByteString(index, static_cast<const ape_parse::StringOperand &>(inOperand).m_value);
				break;
			case ape_parse::OperandType::StringVariable:
				exprType = data::ape::ExprType::StringVariable;
				ConvertByteString(index, static_cast<const ape_parse::StringVariableNameOperand &>(inOperand).m_value);
				break;
			default:
				RKIT_THROW(rkit::ResultCode::kInternalError);
			}

			outExpr.m_exprType = exprType;
			outExpr.m_index = index;
			operands.Append(outExpr);
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

			hashMap.SetPrehashed(hashValue, std::move(key), static_cast<uint32_t>(newIndex));
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
		m_vec.Append(rkit::Span<const uint8_t>(static_cast<const uint8_t *>(data), count));
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
			FormatAnalysisPath(analysisPath, depsNode->GetIdentifier());

			rkit::UniquePtr<rkit::ISeekableReadStream> inFile;
			feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, analysisPath, inFile);

			ReadAPEFile(*inFile, blob);
		}

		for (size_t i = 0; i < blob.m_resourceIDs.Count(); i++)
		{
			data::ape::ResourceIdentifier &rr = blob.m_resourceIDs[i];

			uint32_t realNamespace = 0;
			uint32_t realType = 0;
			IndexNodeCompileResult(rr.m_contentID, realNamespace, realType, static_cast<APEIntermediateResourceType>(rr.m_resType.Get()), blob.m_strings[rr.m_nameIndex.Get()], feedback);

			rr.m_resNamespace = realNamespace;
			rr.m_resType = realType;
		}

		{
			rkit::CIPath outPath;
			FormatOutputPath(outPath, depsNode->GetIdentifier());

			{
				rkit::UniquePtr<rkit::ISeekableReadWriteStream> outFile;
				feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outPath, outFile);

				DumpAPEFile(*outFile, blob);
			}
		}

		RKIT_RETURN_OK;
	}

	rkit::Result APEScriptCompilerImpl::PostNodeCompileTask(APEIntermediateResourceType resType, const rkit::StringView &pathStr, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		switch (resType)
		{
		case APEIntermediateResourceType::kRawFile:
			// Don't need to compile this
			RKIT_RETURN_OK;
		case APEIntermediateResourceType::kScene:
			feedback->AddNodeDependency(kAnoxNamespaceID, buildsystem::kSceneNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, pathStr);
			RKIT_RETURN_OK;
		case APEIntermediateResourceType::kMaterial:
			feedback->AddNodeDependency(kAnoxNamespaceID, buildsystem::kInterfaceMaterialNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, pathStr);
			RKIT_RETURN_OK;
		default:
			RKIT_THROW(rkit::ResultCode::kInternalError);
		}
	}

	rkit::Result APEScriptCompilerImpl::IndexNodeCompileResult(rkit::data::ContentID &outContentID, uint32_t &outResNamespace, uint32_t &outResType, APEIntermediateResourceType resType, const rkit::ByteStringView &pathBStr, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		if (!rkit::CharacterEncodingValidator<rkit::CharacterEncoding::kASCII>::ValidateSpan(pathBStr.ToSpan()))
			RKIT_THROW(rkit::ResultCode::kInvalidUnicode);

		switch (resType)
		{
		case APEIntermediateResourceType::kRawFile:
			{
				rkit::CIPath path;
				path.Set(pathBStr.ToUTF8Unsafe());

				feedback->IndexCAS(rkit::buildsystem::BuildFileLocation::kSourceDir, path, outContentID);
				outResNamespace = kAnoxNamespaceID;
				outResType = resloaders::kContentIDRawFileResourceTypeCode;
			}
			RKIT_RETURN_OK;
		case APEIntermediateResourceType::kScene:
			{
				rkit::String pathStr;
				SceneCompilerBase::FormatOutputPath(pathStr, pathBStr.ToUTF8Unsafe());

				rkit::CIPath path;
				path.Set(pathStr);

				feedback->IndexCAS(rkit::buildsystem::BuildFileLocation::kIntermediateDir, path, outContentID);
				outResNamespace = kAnoxNamespaceID;
				outResType = resloaders::kContentIDRawFileResourceTypeCode;
			}
			RKIT_RETURN_OK;
		case APEIntermediateResourceType::kMaterial:
			{
				rkit::CIPath path;
				MaterialCompiler::ConstructOutputPath(path, data::MaterialResourceType::kInterface, pathBStr.ToUTF8Unsafe());

				feedback->IndexCAS(rkit::buildsystem::BuildFileLocation::kIntermediateDir, path, outContentID);
				outResNamespace = kAnoxNamespaceID;
				outResType = resloaders::kInterfaceMaterialTypeCode;
			}
			RKIT_RETURN_OK;
		default:
			RKIT_THROW(rkit::ResultCode::kInternalError);
		}


		RKIT_THROW(rkit::ResultCode::kInternalError);
	}

	rkit::Result APEScriptCompilerImpl::CompileWindow(APECompilerContext &ctx, CompiledWindowDef &compiledWindow, const WindowDef &wdef)
	{
		compiledWindow.m_windowID = wdef.m_windowID;

		VectorMemoryStream stream(compiledWindow.m_commandStream);

		APEWriter writer(ctx, stream);

		for (const rkit::UniquePtr<ape_parse::WindowCommand> &cmdPtr : wdef.m_commands)
		{
			const ape_parse::WindowCommand &cmd = *cmdPtr;

			compiledWindow.m_commandStream.Append(static_cast<uint8_t>(cmd.GetCommandType()));
			cmd.Write(writer);
		}

		RKIT_RETURN_OK;
	}

	rkit::Result APEScriptCompilerImpl::CompileSwitch(APECompilerContext &ctx, CompiledSwitchDef &compiledSwitch, const SwitchDef &switchDef)
	{
		compiledSwitch.m_switchID = switchDef.m_switchID;

		rkit::HashMap<uint64_t, const ape_parse::SwitchCommand *> switchCmdTree;

		for (const ape_parse::SwitchCommand &inCmd : switchDef.m_commands)
		{
			switchCmdTree.Set(inCmd.m_cc, &inCmd);
		}

		return CompileBasicBlock(ctx, compiledSwitch.m_commands, switchCmdTree, 1);
	}

	rkit::Result APEScriptCompilerImpl::CompileBasicBlock(APECompilerContext &ctx, rkit::Vector<data::ape::SwitchCommand> &cmdStream, const rkit::HashMap<uint64_t, const ape_parse::SwitchCommand *> &tree, uint64_t cc)
	{
		const uint8_t kIfOpcode = 1;
		const uint8_t kExternOpcode = 10;
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
					CompileBasicBlock(ctx, trueBB, tree, ((cc << 2) | 1u));
					CompileBasicBlock(ctx, falseBB, tree, ((cc << 2) | 2u));

					if (falseBB.Count() > std::numeric_limits<uint32_t>::max())
						RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

					if (falseBB.Count() > 0)
					{
						data::ape::SwitchCommand skipFalseBBCmd = {};
						skipFalseBBCmd.m_opcode = kJumpOpcode;
						skipFalseBBCmd.m_strValue = static_cast<uint32_t>(falseBB.Count());

						trueBB.Append(skipFalseBBCmd);
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
					CompileBasicBlock(ctx, trueBB, tree, ((cc << 2) | 1u));

					data::ape::SwitchCommand repeatCmd = {};
					repeatCmd.m_opcode = kRJumpOpcode;
					repeatCmd.m_strValue = static_cast<uint32_t>(trueBB.Count());

					trueBB.Append(repeatCmd);
				}

				outCmd.m_strValue = static_cast<uint32_t>(trueBB.Count());
			}
			else if (cmd.m_opcode == kExternOpcode)
			{
				uint32_t externOpcode = 0;
				uint32_t externArgList = 0;
				if (!cmd.m_str.IsSet())
					RKIT_THROW(rkit::ResultCode::kDataError);

				CompileExtern(ctx, externOpcode, externArgList, cmd.m_str.Get());
				outCmd.m_fmtValue = externOpcode;
				outCmd.m_strValue = externArgList;
			}
			else
			{
				uint32_t index = 0;
				ctx.ConvertOptionalByteString(index, cmd.m_str);
				outCmd.m_strValue = index;
			}

			ctx.ConvertOptionalExprValue(outCmd.m_exprValue, cmd.m_expr);

			if (cmd.m_opcode != kExternOpcode)
			{
				uint32_t fmtValue = 0;
				ctx.ConvertFormattingValue(fmtValue, cmd.m_fmt);
				outCmd.m_fmtValue = fmtValue;
			}

			cmdStream.Append(outCmd);
			cmdStream.Append(trueBB.ToSpan());
			cmdStream.Append(falseBB.ToSpan());

			if (IsTerminalCC(cc))
				break;

			cc = (cc << 2) | 3u;
		}

		RKIT_RETURN_OK;
	}

	rkit::Result APEScriptCompilerImpl::CompileExtern(APECompilerContext &ctx, uint32_t &outOpcode, uint32_t &outArgList, const rkit::ByteStringView &str)
	{
		size_t offset = 0;
		auto skipWhitespace = [&offset, &str]()
			{
				while (offset < str.Length() && rkit::IsASCIIWhitespace(static_cast<char>(str[offset])))
					offset++;
			};

		auto parseCombinedToken = [&skipWhitespace, &offset, &str](rkit::ByteStringSliceView &outArg, const char *extraDelimiters = "") -> rkit::Result
			{
				skipWhitespace();

				if (offset == str.Length())
				{
					outArg = rkit::ByteStringSliceView();
					RKIT_RETURN_OK;
				}

				size_t startPos = offset;
				uint8_t firstChar = str[offset++];

				if (firstChar == '\"')
				{
					for (;;)
					{
						if (offset == str.Length())
						{
							rkit::log::Warning(u8"Unterminated string arg");
							break;
						}

						if (str[offset++] == '\"')
							break;
					}
				}
				else
				{
					size_t lastNonWhitespace = offset;
					while (offset < str.Length())
					{
						char ch = static_cast<char>(str[offset]);
						if (!rkit::IsASCIIWhitespace(static_cast<char>(ch)))
							lastNonWhitespace = offset;

						offset++;
					}

					offset = lastNonWhitespace + 1;
				}

				outArg = str.SubString(startPos, offset - startPos);
				RKIT_RETURN_OK;
			};

		auto parseToken = [&skipWhitespace, &offset, &str](rkit::ByteStringSliceView& outArg, const char *extraDelimiters = "") -> rkit::Result
		{
			auto isInDelimiterList = [extraDelimiters](uint8_t ch)
				{
					const char *scan = extraDelimiters;
					while (*scan)
					{
						if (ch == static_cast<uint8_t>(*scan))
							return true;

						scan++;
					}

					return false;
				};

			skipWhitespace();

			if (offset == str.Length())
			{
				outArg = rkit::ByteStringSliceView();
				RKIT_RETURN_OK;
			}

			size_t startPos = offset;
			uint8_t firstChar = str[offset++];

			if (isInDelimiterList(firstChar))
			{
				outArg = str.SubString(startPos, offset - startPos);
				RKIT_RETURN_OK;
			}

			if (firstChar == '\"')
			{
				for (;;)
				{
					if (offset == str.Length())
					{
						rkit::log::Warning(u8"Unterminated string arg");
						break;
					}

					if (str[offset++] == '\"')
						break;
				}
			}
			else
			{
				while (offset < str.Length())
				{
					char ch = static_cast<char>(str[offset]);
					if (rkit::IsASCIIWhitespace(static_cast<char>(ch)) || isInDelimiterList(ch))
						break;

					offset++;
				}
			}

			outArg = str.SubString(startPos, offset - startPos);
			RKIT_RETURN_OK;
		};

		skipWhitespace();

		rkit::ByteStringSliceView cmdName;
		parseToken(cmdName);

		const rkit::ConstSpan<ape_parse::ExternOpcodeMetadata> opcodeMetadatas(ape_parse::g_externOpcodeMetadata, rkit::ArraySize(ape_parse::g_externOpcodeMetadata));

		rkit::ByteStringView opName;
		const ape_parse::ExternOpcodeMetadata *selectedOp = nullptr;
		for (const ape_parse::ExternOpcodeMetadata &op : opcodeMetadatas)
		{
			opName = rkit::ByteStringView(reinterpret_cast<const uint8_t *>(op.m_name), op.m_nameLength);
			if (cmdName.EqualsNoCase(opName))
			{
				selectedOp = &op;
				break;
			}
		}

		if (selectedOp == nullptr)
		{
			rkit::log::Error(u8"Unknown extern op");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		rkit::Vector<data::ape::ExpressionValue> argValues;
		argValues.Resize(selectedOp->m_argCount);

		for (data::ape::ExpressionValue &argValue : argValues)
		{
			argValue.m_exprType = data::ape::ExprType::Empty;
			argValue.m_index = 0;
		}

		auto parseArg = [&ctx](data::ape::ExpressionValue &outValue, const ape_parse::ExternOpcodeMetadata& opMetadata, const ape_parse::ExternOpcodeArgMetadata& argMetadata, rkit::ByteStringSliceView arg) -> rkit::Result
			{
				bool couldBeVariable = false;
				{
					uint8_t firstCh = arg[0];
					couldBeVariable = (firstCh == '@') || (firstCh == '_') || (firstCh >= 'a' && firstCh < 'z') || (firstCh >= 'A' && firstCh <= 'Z');
				}

				bool indexIsStr = false;
				bool isVariable = false;

				switch (argMetadata.m_fieldType)
				{
				case ape_parse::ExternFieldType::Label:
					if (couldBeVariable && arg[arg.Length() - 1] == '$')
					{
						isVariable = true;
						indexIsStr = true;
						outValue.m_exprType = data::ape::ExprType::StringVariable;
					}
					else
					{
						rkit::Optional<size_t> splitPos;
						for (size_t i = 0; i < arg.Length(); i++)
						{
							if (arg[i] == ':')
							{
								splitPos = i;
								break;
							}
						}

						if (!splitPos.IsSet() || splitPos.Get() == 0 || splitPos.Get() == arg.Length() - 1u)
						{
							rkit::log::Error(u8"Invalid label");
							RKIT_THROW(rkit::ResultCode::kDataError);
						}

						rkit::IUtilitiesDriver &utils = *rkit::GetDrivers().m_utilitiesDriver;

						// FIXME: Make a function for this
						uint32_t labelHigh = 0;
						uint32_t labelLow = 0;
						if (!utils.ParseUInt32(arg.SubString(0, splitPos.Get()), 10, labelHigh) || !utils.ParseUInt32(arg.SubString(splitPos.Get() + 1), 10, labelLow) || !Label::IsValid(labelHigh, labelLow))
						{
							rkit::log::Error(u8"Invalid label");
							RKIT_THROW(rkit::ResultCode::kDataError);
						}

						outValue.m_exprType = data::ape::ExprType::UIntLiteral;
						outValue.m_index = Label(labelHigh, labelLow).RawValue();
					}
					break;

				case ape_parse::ExternFieldType::FloatExpr:
					if (couldBeVariable)
					{
						indexIsStr = true;
						isVariable = true;
						outValue.m_exprType = data::ape::ExprType::FloatExpression;
						break;
					}

					[[fallthrough]];
				case ape_parse::ExternFieldType::Float:
					{
						float f = 0.f;
						if (!rkit::GetDrivers().m_utilitiesDriver->ParseFloat(arg, f))
						{
							rkit::log::ErrorFmt(u8"Invalid float parameter value for argument {} of op {}", rkit::AsciiStringView(argMetadata.m_name, argMetadata.m_nameLength),
								rkit::AsciiStringView(opMetadata.m_name, opMetadata.m_nameLength));
							RKIT_THROW(rkit::ResultCode::kDataError);
						}

						uint32_t bits = 0;
						memcpy(&bits, &f, 4);

						outValue.m_exprType = data::ape::ExprType::FloatLiteral;
						outValue.m_index = rkit::endian::LittleUInt32_t::FromBits(bits);
					}
					break;
				case ape_parse::ExternFieldType::MusicResource:
					{
						outValue.m_exprType = data::ape::ExprType::ResourceID;

						uint32_t index = 0;
						ctx.IndexResource(index, APEIntermediateResourceType::kRawFile, u8"music/", arg);

						outValue.m_index = index;
					}
					break;
				case ape_parse::ExternFieldType::SceneResource:
					{
						if (arg.EndsWith(rkit::AsciiStringView("$").RemoveEncoding()))
						{
							isVariable = true;
							indexIsStr = true;
							outValue.m_exprType = data::ape::ExprType::StringVariable;
						}
						else
						{
							const rkit::ByteStringView expectedSuffix = rkit::AsciiStringView(".s").RemoveEncoding();

							outValue.m_exprType = data::ape::ExprType::ResourceID;

							rkit::ByteStringSliceView normalizedArg = arg;

							rkit::ByteString normalizedArgStorage;
							if (!normalizedArg.EndsWithNoCase(expectedSuffix))
							{
								normalizedArgStorage.Set(arg);
								normalizedArgStorage.Append(expectedSuffix);
								normalizedArg = normalizedArgStorage;
							}

							uint32_t index = 0;
							ctx.IndexResource(index, APEIntermediateResourceType::kScene, u8"scripts/", normalizedArg);

							outValue.m_index = index;
						}
					}
					break;

				case ape_parse::ExternFieldType::SoundResource:
				case ape_parse::ExternFieldType::ImageResource:
				case ape_parse::ExternFieldType::FontResource:
				case ape_parse::ExternFieldType::FileResource:
				case ape_parse::ExternFieldType::ParticleResource:
				case ape_parse::ExternFieldType::Str:
					{
						if (arg.Length() >= 1 && arg[0] == '\"')
						{
							arg = arg.SubString(1);
							if (arg[arg.Length() - 1] == '\"')
								arg = arg.SubString(0, arg.Length() - 1);
						}
						else if (arg.Length() >= 1 && arg[arg.Length() - 1] == '$')
							isVariable = true;

						indexIsStr = true;
						outValue.m_exprType = isVariable ? data::ape::ExprType::StringVariable : data::ape::ExprType::StringLiteral;
					}
					break;
				case ape_parse::ExternFieldType::UInt32:
					{
						uint32_t uintLiteral = 0;
						if (!rkit::GetDrivers().m_utilitiesDriver->ParseUInt32(arg, 10, uintLiteral))
						{
							rkit::log::ErrorFmt(u8"Invalid uint parameter value for argument {} of op {}", rkit::AsciiStringView(argMetadata.m_name, argMetadata.m_nameLength),
								rkit::AsciiStringView(opMetadata.m_name, opMetadata.m_nameLength));
							RKIT_THROW(rkit::ResultCode::kDataError);
						}

						outValue.m_exprType = data::ape::ExprType::UIntLiteral;
						outValue.m_index = uintLiteral;
					}
					break;
				case ape_parse::ExternFieldType::Int32:
					{
						int32_t intLiteral = 0;
						if (!rkit::GetDrivers().m_utilitiesDriver->ParseInt32(arg, 10, intLiteral))
						{
							rkit::log::ErrorFmt(u8"Invalid uint parameter value for argument {} of op {}", rkit::AsciiStringView(argMetadata.m_name, argMetadata.m_nameLength),
								rkit::AsciiStringView(opMetadata.m_name, opMetadata.m_nameLength));
							RKIT_THROW(rkit::ResultCode::kDataError);
						}

						outValue.m_exprType = data::ape::ExprType::IntLiteral;
						outValue.m_index = static_cast<uint32_t>(intLiteral);
					}
					break;
				case ape_parse::ExternFieldType::Bool:
					if (arg.EqualsNoCase(rkit::StringView(u8"true").RemoveEncoding()))
					{
						outValue.m_exprType = data::ape::ExprType::UIntLiteral;
						outValue.m_index = 1;
						break;
					}
					else if (arg.EqualsNoCase(rkit::StringView(u8"false").RemoveEncoding()))
					{
						outValue.m_exprType = data::ape::ExprType::UIntLiteral;
						outValue.m_index = 0;
						break;
					}
					else
					{
						rkit::log::ErrorFmt(u8"Invalid bool parameter value for argument {} of op {}", rkit::AsciiStringView(argMetadata.m_name, argMetadata.m_nameLength),
							rkit::AsciiStringView(opMetadata.m_name, opMetadata.m_nameLength));
						RKIT_THROW(rkit::ResultCode::kDataError);
					}
					break;
				case ape_parse::ExternFieldType::Obj:
					if (arg.EqualsNoCase(rkit::StringView(u8"null").RemoveEncoding()))
					{
						outValue.m_exprType = data::ape::ExprType::Empty;
						outValue.m_index = 0;
						break;
					}

					[[fallthrough]];
				case ape_parse::ExternFieldType::ObjVar:
					{
						if (arg.Equals(rkit::AsciiStringView("0").RemoveEncoding()))
						{
							outValue.m_exprType = data::ape::ExprType::Empty;
							outValue.m_index = 0;
						}
						else
						{
							if (!couldBeVariable)
							{
								rkit::log::ErrorFmt(u8"Argument {} of op {} was not a variable", rkit::AsciiStringView(argMetadata.m_name, argMetadata.m_nameLength),
									rkit::AsciiStringView(opMetadata.m_name, opMetadata.m_nameLength));
								RKIT_THROW(rkit::ResultCode::kDataError);
							}

							isVariable = true;
							indexIsStr = true;
							outValue.m_exprType = data::ape::ExprType::ObjectVariable;
						}
					}
					break;
				case ape_parse::ExternFieldType::TextureVar:
					if (!couldBeVariable)
					{
						rkit::log::ErrorFmt(u8"Argument {} of op {} was not a variable", rkit::AsciiStringView(argMetadata.m_name, argMetadata.m_nameLength),
							rkit::AsciiStringView(opMetadata.m_name, opMetadata.m_nameLength));
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					isVariable = true;
					indexIsStr = true;
					outValue.m_exprType = data::ape::ExprType::TextureVariable;
					break;
				case ape_parse::ExternFieldType::FloatVar:
					{
						if (!couldBeVariable)
						{
							rkit::log::ErrorFmt(u8"Argument {} of op {} was not a variable", rkit::AsciiStringView(argMetadata.m_name, argMetadata.m_nameLength),
								rkit::AsciiStringView(opMetadata.m_name, opMetadata.m_nameLength));
							RKIT_THROW(rkit::ResultCode::kDataError);
						}

						isVariable = true;
						indexIsStr = true;
						outValue.m_exprType = data::ape::ExprType::FloatVariable;
					}
					break;
				case ape_parse::ExternFieldType::StrVar:
					{
						if (!couldBeVariable || arg[arg.Length() - 1] != '$')
						{
							rkit::log::ErrorFmt(u8"Argument {} of op {} was not a variable", rkit::AsciiStringView(argMetadata.m_name, argMetadata.m_nameLength),
								rkit::AsciiStringView(opMetadata.m_name, opMetadata.m_nameLength));
							RKIT_THROW(rkit::ResultCode::kDataError);
						}

						isVariable = true;
						indexIsStr = true;
						outValue.m_exprType = data::ape::ExprType::StringVariable;
					}
					break;
				default:
					RKIT_THROW(rkit::ResultCode::kInternalError);
				}

				if (indexIsStr)
				{
					uint32_t strIndex = 0;
					rkit::ByteString bstr;
					bstr.Set(arg);

					if (isVariable)
					{
						bstr.MakeLower();
					}

					ctx.IndexString(strIndex, bstr);
					outValue.m_index = strIndex;
				}

				RKIT_RETURN_OK;
			};

		if (selectedOp->m_isSpecial)
		{
			const rkit::ConstSpan<ape_parse::ExternOpcodeArgMetadata> argMetadatas(selectedOp->m_argMetadata, selectedOp->m_argCount);

			auto findArg = [argMetadatas](const rkit::AsciiStringView &name) -> size_t
				{
					size_t index = 0;
					for (const ape_parse::ExternOpcodeArgMetadata &argMetadata : argMetadatas)
					{
						if (name.EqualsNoCase(rkit::AsciiStringView(argMetadata.m_name, argMetadata.m_nameLength)))
							break;

						index++;
					}

					RKIT_ASSERT(index != argMetadatas.Count());

					return index;
				};

			rkit::Vector<rkit::Pair<rkit::AsciiStringView, data::ape::ExpressionValue>> namedParams;

			if (opName.EqualsNoCase(rkit::AsciiStringView("cam_set").RemoveEncoding()))
			{
				rkit::ByteStringSliceView argToken;
				parseToken(argToken, "(");

				if (argToken.Length() == 0)
				{
					rkit::log::Error(u8"Missing property argument for cam_set");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				size_t argIndex = findArg("property");
				parseArg(argValues[argIndex], *selectedOp, selectedOp->m_argMetadata[argIndex], argToken);

				if (argToken.EqualsNoCase(rkit::AsciiStringView("from").RemoveEncoding()) || argToken.EqualsNoCase(rkit::AsciiStringView("to").RemoveEncoding()))
				{
					RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
				}
				else
				{
					const bool isStringArg = argToken.EqualsNoCase(rkit::AsciiStringView("cam_owner").RemoveEncoding());

					argIndex = findArg(isStringArg ? rkit::AsciiStringView("str") : rkit::AsciiStringView("v0"));

					parseToken(argToken);

					if (argToken.Length() == 0)
					{
						rkit::log::Error(u8"Missing value argument for cam_set");
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					parseArg(argValues[argIndex], *selectedOp, selectedOp->m_argMetadata[argIndex], argToken);
				}
			}
			else if (opName.EqualsNoCase(rkit::AsciiStringView("cam_to").RemoveEncoding())
				|| opName.EqualsNoCase(rkit::AsciiStringView("cam_from").RemoveEncoding()))
			{
				const rkit::AsciiStringView xyzArgList[] = {"x", "y", "z" };
				const rkit::AsciiStringView targetArgList[] = {"target"};

				rkit::ByteStringSliceView argTokens[3];

				for (size_t i = 0; i < 3; i++)
				{
					parseToken(argTokens[i]);
				}

				rkit::ConstSpan<rkit::AsciiStringView> propertyNames;
				if (argTokens[0].Length() > 0 && argTokens[1].Length() == 0 && argTokens[2].Length() == 0)
					propertyNames = rkit::ConstSpan<rkit::AsciiStringView>(targetArgList);
				else if (argTokens[0].Length() > 0 && argTokens[1].Length() > 0 && argTokens[2].Length() > 0)
					propertyNames = rkit::ConstSpan<rkit::AsciiStringView>(xyzArgList);
				else
				{
					rkit::log::ErrorFmt(u8"Invalid argument count for {}", rkit::AsciiStringView(selectedOp->m_name, selectedOp->m_nameLength));
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				for (size_t inArgIndex = 0; inArgIndex < propertyNames.Count(); inArgIndex++)
				{
					const size_t outArgIndex = findArg(propertyNames[inArgIndex]);
					parseArg(argValues[outArgIndex], *selectedOp, selectedOp->m_argMetadata[outArgIndex], argTokens[inArgIndex]);
				}
			}
			else if (opName.EqualsNoCase(rkit::AsciiStringView("set2dcursor").RemoveEncoding()))
			{
				const size_t kMaxImageArgs = 5;

				rkit::ByteStringSliceView imageArgTokens[kMaxImageArgs];
				const rkit::AsciiStringView imageArgNames[kMaxImageArgs] =
				{
					"frame0",
					"frame1",
					"frame2",
					"frame3",
					"frame4",
				};

				const size_t kMaxNumericArgs = 3;
				rkit::ByteStringSliceView numericArgTokens[kMaxNumericArgs];
				const rkit::AsciiStringView numericArgNames[kMaxNumericArgs] =
				{
					"param0",
					"param1",
					"param2",
				};

				size_t numImageArgs = 0;
				size_t numNumericArgs = 0;

				rkit::ByteStringSliceView argToken;
				parseToken(argToken, "(");

				auto isNumericTokenOrEmpty = [](const rkit::ByteStringSliceView &str)
					{
						for (uint8_t ch : str)
						{
							if (ch < '0' || ch > '9')
								return false;
						}

						return true;
					};

				while (argToken.Length() > 0)
				{
					if (isNumericTokenOrEmpty(argToken))
						break;

					if (numImageArgs == kMaxImageArgs)
					{
						rkit::log::Error(u8"Too many image args for Set2DCursor");
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					imageArgTokens[numImageArgs++] = argToken;
					parseToken(argToken);
				}

				while (argToken.Length() > 0)
				{
					if (numNumericArgs == kMaxNumericArgs)
					{
						rkit::log::Error(u8"Too many image args for Set2DCursor");
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					numericArgTokens[numNumericArgs++] = argToken;
					parseToken(argToken);

					if (!isNumericTokenOrEmpty(argToken))
					{
						rkit::log::Error(u8"Expected numeric arg for Set2DCursor");
						RKIT_THROW(rkit::ResultCode::kDataError);
					}
				}

				auto mapArgList = [&findArg, &parseArg, &argValues, selectedOp](const rkit::ByteStringSliceView *tokens, const rkit::AsciiStringView *argNames, size_t numArgs) -> rkit::Result
					{
						for (size_t inputIndex = 0; inputIndex < numArgs; inputIndex++)
						{
							const size_t argIndex = findArg(argNames[inputIndex]);
							parseArg(argValues[argIndex], *selectedOp, selectedOp->m_argMetadata[argIndex], tokens[inputIndex]);
						}

						RKIT_RETURN_OK;
					};

				if (numImageArgs == 0)
				{
					rkit::log::Error(u8"No frames specified for Set2DCursor");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				if (imageArgTokens[0].EqualsNoCase(rkit::AsciiStringView("default").RemoveEncoding()))
				{
					const rkit::ByteStringSliceView defaultArgToken = rkit::AsciiStringView("true").RemoveEncoding();
					const rkit::AsciiStringView defaultArgName = rkit::AsciiStringView("isDefault");

					mapArgList(&defaultArgToken, &defaultArgName, 1);
				}
				else
				{
					mapArgList(numericArgTokens, numericArgNames, numNumericArgs);
					mapArgList(imageArgTokens, imageArgNames, numImageArgs);
				}
			}
			else if (opName.EqualsNoCase(rkit::AsciiStringView("adjust_stat").RemoveEncoding()))
			{
				rkit::ByteStringSliceView argToken;
				parseToken(argToken);

				if (argToken.Length() == 0)
				{
					rkit::log::Error(u8"Missing target for adjust_stat");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				const rkit::ConstSpan<ape_parse::ExternOpcodeArgMetadata> args(selectedOp->m_argMetadata, selectedOp->m_argCount);

				const size_t targetArgIndex = (argToken.Length() > 0 && argToken[0] == '@') ? findArg("targetobj") : findArg("targetName");
				parseArg(argValues[targetArgIndex], *selectedOp, args[targetArgIndex], argToken);

				const size_t statArgIndex = findArg("stat");
				const size_t amountArgIndex = findArg("amount");

				rkit::ByteStringSliceView statToken;
				rkit::ByteStringSliceView amountToken;

				parseToken(statToken);

				if (statToken.Length() == 0)
				{
					rkit::log::Error(u8"Missing target for adjust_stat");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				if (statToken[0] == '\"')
				{
					statToken = statToken.SubString(1);
					if (statToken.Length() > 0 && statToken[statToken.Length() - 1] == '\"')
						statToken = statToken.SubString(0, statToken.Length() - 1);

					size_t splitIndex = 0;
					while (splitIndex < statToken.Length())
					{
						if (statToken[splitIndex] == '=')
							break;
						else
							splitIndex++;
					}

					if (splitIndex == statToken.Length())
					{
						rkit::log::Error(u8"Missing amount for adjust_stat");
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					amountToken = statToken.SubString(splitIndex + 1);
					statToken = statToken.SubString(0, splitIndex);
				}
				else
				{
					parseToken(amountToken);
				}

				if (statToken.Length() == 0)
				{
					rkit::log::Error(u8"Missing amount for adjust_stat");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				parseArg(argValues[statArgIndex], *selectedOp, args[statArgIndex], statToken);
				parseArg(argValues[amountArgIndex], *selectedOp, args[amountArgIndex], amountToken);
			}
			else
			{
				// Unknown special op
				RKIT_THROW(rkit::ResultCode::kInternalError);
			}
		}
		else
		{
			// Normal not-special arg parsing
			for (size_t argIndex = 0; argIndex < selectedOp->m_numRequiredParameters; argIndex++)
			{
				rkit::ByteStringSliceView argToken;

				if (selectedOp->m_isCombined)
				{
					parseCombinedToken(argToken);
				}
				else
				{
					parseToken(argToken);
				}

				if (argToken.Length() == 0)
				{
					rkit::log::ErrorFmt(u8"Missing required argument for {}", rkit::AsciiStringView(selectedOp->m_name, selectedOp->m_nameLength));
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				parseArg(argValues[argIndex], *selectedOp, selectedOp->m_argMetadata[argIndex], argToken);
			}

			for (size_t argIndex = selectedOp->m_numRequiredParameters; argIndex < selectedOp->m_numUnnamedParameters; argIndex++)
			{
				rkit::ByteStringSliceView argToken;
				parseToken(argToken);

				if (argToken.Length() == 0)
					break;

				parseArg(argValues[argIndex], *selectedOp, selectedOp->m_argMetadata[argIndex], argToken);
			}

			if (selectedOp->m_numUnnamedParameters < selectedOp->m_argCount)
			{
				for (;;)
				{
					rkit::ByteStringSliceView nameToken;
					parseToken(nameToken, "=");

					if (nameToken.Length() == 0)
						break;

					rkit::ByteStringSliceView eqToken;
					parseToken(eqToken, "=");


					rkit::ByteStringSliceView valueToken;
					parseToken(valueToken);

					if (eqToken != rkit::AsciiStringView("=").RemoveEncoding() || valueToken.Length() == 0)
					{
						rkit::log::ErrorFmt(u8"Missing required argument for {}", rkit::AsciiStringView(selectedOp->m_name, selectedOp->m_nameLength));
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					rkit::Optional<size_t> argIndex;
					for (size_t i = selectedOp->m_numUnnamedParameters; i < selectedOp->m_argCount; i++)
					{
						const ape_parse::ExternOpcodeArgMetadata &argMetadata = selectedOp->m_argMetadata[i];
						if (nameToken.EqualsNoCase(rkit::AsciiStringView(argMetadata.m_name, argMetadata.m_nameLength).RemoveEncoding()))
						{
							argIndex = i;
							break;
						}
					}

					if (!argIndex.IsSet())
					{
						rkit::log::ErrorFmt(u8"Unrecognized argument for {}", rkit::AsciiStringView(selectedOp->m_name, selectedOp->m_nameLength));
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					parseArg(argValues[argIndex.Get()], *selectedOp, selectedOp->m_argMetadata[argIndex.Get()], valueToken);
				}
			}
		}

		// Check for extra args
		{
			rkit::ByteStringSliceView argToken;
			parseToken(argToken);

			if (argToken.Length() > 0)
			{
				rkit::log::ErrorFmt(u8"Too many arguments for extern {}", rkit::AsciiStringView(selectedOp->m_name, selectedOp->m_nameLength));
				RKIT_THROW(rkit::ResultCode::kDataError);
			}
		}

		ctx.IndexOperandList(outArgList, std::move(argValues));
		outOpcode = selectedOp->m_opcode;

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
			catalog.m_numResourceIDs = static_cast<uint32_t>(blob.m_resourceIDs.Count());

			stream.WriteOneBinary(catalog);
		}

		for (const rkit::ByteString &str : blob.m_strings)
		{
			rkit::endian::LittleUInt32_t strLength = rkit::endian::LittleUInt32_t(str.Length());
			stream.WriteOneBinary(strLength);
		}

		for (const rkit::ByteString &str : blob.m_strings)
		{
			stream.WriteAllSpan(str.ToSpan());
		}

		stream.WriteAllSpan(blob.m_exprs.ToSpan());

		for (const rkit::Vector<data::ape::ExpressionValue> &opList : blob.m_operandLists)
		{
			rkit::endian::LittleUInt32_t opListCount = rkit::endian::LittleUInt32_t(opList.Count());
			stream.WriteOneBinary(opListCount);
		}

		for (const rkit::Vector<data::ape::ExpressionValue> &opList : blob.m_operandLists)
		{
			stream.WriteAllSpan(opList.ToSpan());
		}

		for (size_t windowIndex = 0; windowIndex < blob.m_windows.Count(); windowIndex++)
		{
			data::ape::Window window = {};
			window.m_commandStreamLength = static_cast<uint32_t>(blob.m_windows[windowIndex].m_commandStream.Count());
			window.m_windowID = blob.m_windows[windowIndex].m_windowID;
			stream.WriteOneBinary(window);
		}

		for (const CompiledWindowDef &window : blob.m_windows)
		{
			stream.WriteAllSpan(window.m_commandStream.ToSpan());
		}

		for (size_t switchIndex = 0; switchIndex < blob.m_switches.Count(); switchIndex++)
		{
			data::ape::Switch sw = {};
			sw.m_numCommands = static_cast<uint32_t>(blob.m_switches[switchIndex].m_commands.Count());
			sw.m_switchID = blob.m_switches[switchIndex].m_switchID;
			stream.WriteOneBinary(sw);
		}

		for (const CompiledSwitchDef &sw : blob.m_switches)
		{
			stream.WriteAllSpan(sw.m_commands.ToSpan());
		}

		stream.WriteAllSpan(blob.m_resourceIDs.ToSpan());

		RKIT_RETURN_OK;
	}

	rkit::Result APEScriptCompilerImpl::ReadAPEFile(rkit::IReadStream &stream, APEBlob &blob)
	{
		data::ape::APEScriptCatalog catalog;

		stream.ReadOneBinary(catalog);

		const size_t numStrings = catalog.m_numStrings.Get();
		blob.m_strings.Resize(numStrings);

		rkit::Vector<rkit::ByteStringConstructionBuffer> stringCBufs;
		stringCBufs.Resize(numStrings);

		blob.m_operandLists.Resize(catalog.m_numOperandLists.Get());
		blob.m_windows.Resize(catalog.m_numWindows.Get());
		blob.m_switches.Resize(catalog.m_numSwitches.Get());
		blob.m_exprs.Resize(catalog.m_numExprs.Get());
		blob.m_resourceIDs.Resize(catalog.m_numResourceIDs.Get());

		for (rkit::ByteStringConstructionBuffer &strCBuf : stringCBufs)
		{
			rkit::endian::LittleUInt32_t strLength;
			stream.ReadOneBinary(strLength);

			strCBuf.Allocate(strLength.Get());
		}

		for (rkit::ByteStringConstructionBuffer &strCBuf : stringCBufs)
		{
			stream.ReadAllSpan(strCBuf.GetSpan());
		}

		rkit::ProcessParallelSpans(blob.m_strings.ToSpan(), stringCBufs.ToSpan(), [](rkit::ByteString &str, rkit::ByteStringConstructionBuffer &cbuf)
			{
				str = rkit::ByteString(std::move(cbuf));
			});

		stream.ReadAllSpan(blob.m_exprs.ToSpan());

		for (rkit::Vector<data::ape::ExpressionValue> &opList : blob.m_operandLists)
		{
			rkit::endian::LittleUInt32_t opListCount;
			stream.ReadOneBinary(opListCount);
			opList.Resize(opListCount.Get());
		}

		for (rkit::Vector<data::ape::ExpressionValue> &opList : blob.m_operandLists)
		{
			stream.ReadAllSpan(opList.ToSpan());
		}

		for (size_t windowIndex = 0; windowIndex < blob.m_windows.Count(); windowIndex++)
		{
			data::ape::Window window = {};
			stream.ReadOneBinary(window);

			blob.m_windows[windowIndex].m_windowID = window.m_windowID.Get();
			blob.m_windows[windowIndex].m_commandStream.Resize(window.m_commandStreamLength.Get());
		}

		for (CompiledWindowDef &window : blob.m_windows)
		{
			stream.ReadAllSpan(window.m_commandStream.ToSpan());
		}

		for (size_t switchIndex = 0; switchIndex < blob.m_switches.Count(); switchIndex++)
		{
			data::ape::Switch sw = {};
			stream.ReadOneBinary(sw);

			blob.m_switches[switchIndex].m_switchID = sw.m_switchID.Get();
			blob.m_switches[switchIndex].m_commands.Resize(sw.m_numCommands.Get());
		}

		for (CompiledSwitchDef &sw : blob.m_switches)
		{
			stream.ReadAllSpan(sw.m_commands.ToSpan());
		}

		stream.ReadAllSpan(blob.m_resourceIDs.ToSpan());

		RKIT_RETURN_OK;
	}

	rkit::Result APEScriptCompilerImpl::ReadDepsCatalog(DynamicResourcesDictionary &resDict, const rkit::StringView &identifier, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::Vector<char> depsFileContentsVector;

		size_t fileSize = 0;
		{
			rkit::String pathStr;
			pathStr.Format(u8"anox/apedeps/{}deps", identifier);

			rkit::CIPath path;
			path.Set(pathStr);

			rkit::UniquePtr<rkit::ISeekableReadStream> inputFile;
			feedback->TryOpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, path, inputFile);

			if (!inputFile.IsValid())
			{
				resDict.m_resCategories.Clear();
				RKIT_RETURN_OK;
			}

			if (inputFile->GetSize() > std::numeric_limits<size_t>::max())
				RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

			fileSize = static_cast<size_t>(inputFile->GetSize());
			depsFileContentsVector.Resize(fileSize);
			inputFile->ReadAllSpan(depsFileContentsVector.ToSpan());
		}

		DynamicResourceCategory *category = nullptr;

		const rkit::ConstSpan<char> depsFileContents = depsFileContentsVector.ToSpan();

		size_t lineStart = 0;

		while (lineStart < fileSize)
		{
			size_t nextLineStart = 0;
			size_t lineEnd = lineStart;

			for (;;)
			{
				if (lineEnd == fileSize)
				{
					nextLineStart = lineEnd;
					break;
				}

				const char ch = depsFileContents[lineEnd];
				if (ch == '\r')
				{
					nextLineStart = lineEnd + 1;
					if (nextLineStart < fileSize && depsFileContents[nextLineStart] == '\n')
						nextLineStart++;
					break;
				}
				if (ch == '\n')
				{
					nextLineStart = lineEnd + 1;
					break;
				}

				lineEnd++;
			}

			const rkit::ConstSpan<char> lineContentsSpan = depsFileContents.SubSpan(lineStart, lineEnd - lineStart);
			lineStart = nextLineStart;

			if (lineContentsSpan.Count() == 0)
				continue;

			if (!rkit::CharacterEncodingValidator<rkit::CharacterEncoding::kASCII>::ValidateSpan(lineContentsSpan))
			{
				rkit::log::Error(u8"Invalid ASCII path");
				RKIT_THROW(rkit::ResultCode::kInvalidUnicode);
			}

			const rkit::AsciiStringSliceView lineContents(lineContentsSpan);

			if (lineContents.StartsWith("["))
			{
				if (!lineContents.EndsWith("]"))
				{
					rkit::log::Error(u8"Invalid category");
					RKIT_THROW(rkit::ResultCode::kInvalidUnicode);
				}

				rkit::AsciiString categoryStr;
				categoryStr.Set(lineContents.SubString(1, lineContents.Length() - 2));

				const rkit::HashValue_t hash = rkit::Hasher<rkit::AsciiString>::ComputeHash(0, categoryStr);
				const rkit::HashMap<rkit::AsciiString, DynamicResourceCategory>::Iterator_t existingIt = resDict.m_resCategories.FindPrehashed(hash, categoryStr);
				if (existingIt == resDict.m_resCategories.end())
				{
					rkit::HashMap<rkit::AsciiString, DynamicResourceCategory>::Iterator_t newIt;
					resDict.m_resCategories.SetAndGetIterator(newIt, std::move(categoryStr), DynamicResourceCategory());

					category = &newIt.Value();
				}
				else
					category = &existingIt.Value();
			}
			else
			{
				if (!category)
				{
					rkit::log::Error(u8"File was specified outside of a category");
					RKIT_THROW(rkit::ResultCode::kInvalidUnicode);
				}

				DynamicResourceDef resDef;
				resDef.m_path.Set(lineContents);
				category->m_defs.Append(std::move(resDef));
			}
		}

		RKIT_RETURN_OK;
	}

	rkit::Result APEScriptCompilerImpl::FormatAnalysisPath(rkit::CIPath &outPath, const rkit::StringView &identifier)
	{
		rkit::String formattedPath;
		formattedPath.Format(u8"ax_ape/a/{}", identifier);
		return outPath.Set(formattedPath);
	}

	rkit::Result APEScriptCompilerImpl::FormatExtraDepsPath(rkit::CIPath &outPath, const rkit::StringView &identifier)
	{
		rkit::String formattedPath;
		formattedPath.Format(u8"ax_ape/d/{}", identifier);
		return outPath.Set(formattedPath);
	}

	rkit::Result APEScriptCompilerImpl::FormatOutputPath(rkit::CIPath &outPath, const rkit::StringView &identifier)
	{
		rkit::String formattedPath;
		formattedPath.Format(u8"ax_ape/c/{}", identifier);
		return outPath.Set(formattedPath);
	}

	rkit::Result APEScriptCompilerImpl::ResolveDepResourceType(DepResourceType &resType, const rkit::AsciiStringSliceView &categoryStr)
	{
		size_t openBracePos = 0;
		while (openBracePos < categoryStr.Length())
		{
			if (categoryStr[openBracePos] == '(')
				break;
			else
				openBracePos++;
		}

		const rkit::AsciiStringSliceView assetTypeName = categoryStr.SubString(0, openBracePos);

		if (assetTypeName.EqualsNoCase("image"))
			resType = DepResourceType::Image;
		else if (assetTypeName.EqualsNoCase("texture"))
			resType = DepResourceType::Texture;
		else if (assetTypeName.EqualsNoCase("sound"))
			resType = DepResourceType::Sound;
		else if (assetTypeName.EqualsNoCase("font"))
			resType = DepResourceType::Font;
		else if (assetTypeName.EqualsNoCase("musicseg"))
			resType = DepResourceType::MusicSeg;
		else if (assetTypeName.EqualsNoCase("music"))
			resType = DepResourceType::Music;
		else if (assetTypeName.EqualsNoCase("particle"))
			resType = DepResourceType::Particle;
		else if (assetTypeName.EqualsNoCase("scene"))
			resType = DepResourceType::Scene;
		else if (assetTypeName.EqualsNoCase("model"))
			resType = DepResourceType::Model;
		else if (assetTypeName.EqualsNoCase("file"))
			resType = DepResourceType::File;
		else
		{
			rkit::log::Error(u8"Unknown asset type name");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		RKIT_RETURN_OK;
	}

	rkit::Result APEScriptCompilerImpl::NormalizeResourcePath(rkit::AsciiString &outString, DepResourceType resType, const rkit::AsciiStringSliceView &categoryStr)
	{
		rkit::AsciiStringView prefixStr;

		switch (resType)
		{
		case DepResourceType::Image:
			prefixStr = rkit::AsciiStringView("gameflow/");
			break;
		default:
			break;
		}

		rkit::Vector<char> resultChars;
		resultChars.Append(prefixStr.ToSpan());

		for (char ch : categoryStr)
		{
			if (ch == '\\' || ch == '/')
			{
				if (resultChars.Count() > 0 && resultChars[resultChars.Count() - 1] != '/')
				{
					resultChars.Append('/');
				}
			}
			else
			{
				if (ch >= 'A' && ch <= 'Z')
					ch = ch - 'A' + 'a';

				resultChars.Append(ch);
			}
		}

		bool shouldRemoveExtension = false;

		switch (resType)
		{
		case DepResourceType::Image:
			shouldRemoveExtension = true;
			break;
		default:
			break;
		}

		if (shouldRemoveExtension)
		{
			size_t extPos = resultChars.Count();
			while (extPos > 0)
			{
				const char ch = resultChars[--extPos];
				if (ch == '.')
				{
					resultChars.ShrinkToSize(extPos);
					break;
				}

				if (ch == '/')
					break;
			}
		}

		rkit::AsciiStringConstructionBuffer cbuf;
		cbuf.Allocate(resultChars.Count());
		rkit::CopySpanNonOverlapping(cbuf.GetSpan(), resultChars.ToSpan());

		outString = rkit::AsciiString(std::move(cbuf));

		RKIT_RETURN_OK;
	}

	template<class TFunc>
	rkit::Result APEScriptCompilerImpl::ForEachDependency(const DynamicResourcesDictionary &dict, const TFunc &func)
	{
		typedef rkit::Pair<const rkit::AsciiString *, const DynamicResourceCategory *> KeyValuePair_t;
		rkit::Vector<KeyValuePair_t> sortedCategories;

		for (const rkit::HashMapKeyValueView<rkit::AsciiString, const DynamicResourceCategory> &kvp : dict.m_resCategories)
		{
			sortedCategories.Append(KeyValuePair_t(&kvp.Key(), &kvp.Value()));
		}

		rkit::QuickSort(sortedCategories.begin(), sortedCategories.end(), [](const KeyValuePair_t &a, const KeyValuePair_t &b)
			{
				return (*a.First()) < (*b.First());
			});

		for (const KeyValuePair_t &kvp : sortedCategories)
		{
			func(*kvp.First(), *kvp.Second());
		}

		RKIT_RETURN_OK;
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
		return 2;
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
		ResolvePath(depsFilePath, depsNode->GetIdentifier(), feedback);

		feedback->AddNodeDependency(rkit::buildsystem::kDefaultNamespace, rkit::buildsystem::kDepsNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, depsFilePath.ToString());

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
					fileInfo.Set(fsView);
					fileList.Append(std::move(fileInfo));
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
			feedback->IndexCAS(fileStatus.m_location, fileStatus.m_filePath, cid);
			contentIDs.Append(cid);
		}

		rkit::String outPathStr;
		outPathStr.Format(u8"ax_ape/g/{}", depsNode->GetIdentifier());

		rkit::CIPath outPath;
		outPath.Set(outPathStr);

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> outFile;
		feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outPath, outFile);

		outFile->WriteAllSpan(contentIDs.ToSpan());

		RKIT_RETURN_OK;
	}

	rkit::Result APEGroupCompilerImpl::ResolvePath(rkit::CIPath &depsFilePath, const rkit::StringView &groupNodeIdentifier, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::CIPath groupPath;
		groupPath.Set(groupNodeIdentifier);

		rkit::UniquePtr<rkit::ISeekableReadStream> inFile;
		feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, groupPath, inFile);

		const rkit::FilePos_t fileSize = inFile->GetSize();

		rkit::Vector<rkit::Utf8Char_t> pathBytes;
		if (inFile->GetSize() > std::numeric_limits<size_t>::max())
			RKIT_THROW(rkit::ResultCode::kDataError);

		pathBytes.Resize(static_cast<size_t>(fileSize));

		inFile->ReadAllSpan(pathBytes.ToSpan());

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
	
	bool APEDepsCompiler::HasAnalysisStage() const
	{
		return true;
	}

	rkit::Result APEDepsCompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return Impl().RunAnalysis(depsNode, feedback);
	}

	rkit::Result APEDepsCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return Impl().RunCompile(depsNode, feedback);
	}

	uint32_t APEDepsCompiler::GetVersion() const
	{
		return 1;
	}

	rkit::Result APEDepsCompiler::Create(rkit::UniquePtr<APEDepsCompiler> &outCompiler)
	{
		return rkit::New<APEDepsCompiler>(outCompiler);
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::buildsystem::APEDepsCompilerImpl)
RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::buildsystem::APEScriptCompilerImpl)
RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::buildsystem::APEGroupCompilerImpl)
