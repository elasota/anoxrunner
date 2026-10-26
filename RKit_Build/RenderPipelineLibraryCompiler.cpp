#include "RenderPipelineLibraryCompiler.h"

#include "rkit/Utilities/TextParser.h"

#include "rkit/Data/DataDriver.h"
#include "rkit/Data/RenderDataHandler.h"

#include "rkit/BuildSystem/BuildSystem.h"
#include "rkit/BuildSystem/PackageBuilder.h"

#include "rkit/Core/LogDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/StringPool.h"
#include "rkit/Core/Vector.h"
#include "rkit/Core/Optional.h"

#include <cstring>

namespace rkit::buildsystem::rpc_interchange
{
	enum class EntityType
	{
		StaticSampler,
		StructDef,
		InputLayout,
		GraphicsPipeline,
		PipelineLayout,
		RenderPass,
	};

	class Entity : public NoCopy
	{
	public:
		virtual ~Entity() {}

		virtual EntityType GetEntityType() const = 0;

		Result SetName(const StringView &str)
		{
			return m_name.Set(str);
		}

		const String &GetName() const
		{
			return m_name;
		}

	private:
		String m_name;
	};

	class StructDefEntity final : public Entity
	{
	public:
		explicit StructDefEntity() {}

		render::StructureType &GetDesc() { return m_desc; }
		const render::StructureType &GetDesc() const { return m_desc; }

		Vector<const render::StructureMemberDesc *> &GetStructMembersVector() { return m_structMembers; }

		EntityType GetEntityType() const override { return EntityType::StructDef; }

	private:
		render::StructureType m_desc;
		Vector<const render::StructureMemberDesc *> m_structMembers;
	};

	class InputLayoutEntity final : public Entity
	{
	public:
		explicit InputLayoutEntity() {}

		render::InputLayoutDesc &GetDesc() { return m_desc; }

		Vector<const render::InputLayoutVertexInputDesc *> &GetVertexInputs() { return m_vertexInputs; }

		EntityType GetEntityType() const override { return EntityType::InputLayout; }

	private:
		render::InputLayoutDesc m_desc;
		Vector<const render::InputLayoutVertexInputDesc *> m_vertexInputs;
	};

	class PipelineLayoutEntity final : public Entity
	{
	public:
		explicit PipelineLayoutEntity() {}

		render::PipelineLayoutDesc &GetDesc() { return m_desc; }
		Vector<const render::DescriptorHeapBindingDesc *> &GetDescriptorHeaps() { return m_descriptorHeaps; }
		Vector<const render::LayoutInlineConstantDesc *> &GetLayoutInlineConstants() { return m_layoutInlineConstants; }
		Vector<const render::LayoutConstantBufferDesc *> &GetLayoutConstantBuffers() { return m_layoutConstantBuffers; }

		EntityType GetEntityType() const override { return EntityType::PipelineLayout; }

	private:
		render::PipelineLayoutDesc m_desc;
		rkit::Vector<const render::DescriptorHeapBindingDesc *> m_descriptorHeaps;
		rkit::Vector<const render::LayoutInlineConstantDesc *> m_layoutInlineConstants;
		rkit::Vector<const render::LayoutConstantBufferDesc *> m_layoutConstantBuffers;
	};

	class GraphicsPipelineEntity final : public Entity
	{
	public:
		explicit GraphicsPipelineEntity() {}

		render::GraphicsPipelineDesc &GetDesc() { return m_desc; }

		Vector<const render::RenderTargetDesc *> &GetRenderTargets() { return m_renderTargets; }
		Vector<const render::RenderOperationDesc *> &GetRenderOperations() { return m_renderOperations; }
		Vector<const render::DescriptorHeapBindingDesc *> &GetDescriptorHeapBindings() { return m_descriptorHeapBindings; }
		Vector<const render::ConstantBufferBindingDesc *> &GetConstantBufferBindings() { return m_constantBufferBindings; }

		EntityType GetEntityType() const override { return EntityType::GraphicsPipeline; }

	private:
		render::GraphicsPipelineDesc m_desc;
		Vector<const render::RenderTargetDesc *> m_renderTargets;
		Vector<const render::RenderOperationDesc *> m_renderOperations;
		Vector<const render::DescriptorHeapBindingDesc *> m_descriptorHeapBindings;
		Vector<const render::ConstantBufferBindingDesc *> m_constantBufferBindings;
	};

	class RenderPassEntity final : public Entity
	{
	public:
		explicit RenderPassEntity() {}

		render::RenderPassDesc &GetDesc() { return m_desc; }
		Vector<const render::RenderTargetDesc *> &GetRenderTargetDescs() { return m_rtDescs; }

		EntityType GetEntityType() const override { return EntityType::RenderPass; }

	private:
		render::RenderPassDesc m_desc;
		Vector<const render::RenderTargetDesc *> m_rtDescs;
	};
} // rkit::buildsystem::rpc_interchange

namespace rkit::buildsystem::rpc_common
{
	const uint32_t kLibraryIndexID = RKIT_FOURCC('R', 'P', 'L', 'I');
	const uint32_t kLibraryIndexVersion = 1;

	class LibraryCompilerBase
	{
	public:
		static Result FormatGraphicPipelinePath(CIPath &path, const StringView &identifier, size_t pipelineIndex);
		static Result FormatGlobalsPath(CIPath &path, const StringView &identifier);
		static Result FormatIndexPath(CIPath &path, const StringView &identifier);
		static Result FormatCombinedOutputPath(CIPath &path, const StringView &identifier);
	};
} // rkit::buildsystem::rpc_common

namespace rkit::buildsystem::rpc_analyzer
{
	class IncludedFileKey
	{
	public:
		IncludedFileKey(BuildFileLocation location, const CIPath &str);

		bool operator==(const IncludedFileKey &other) const;
		bool operator!=(const IncludedFileKey &other) const;

		HashValue_t ComputeHash(HashValue_t baseHash) const;

	private:
		BuildFileLocation m_location = BuildFileLocation::kInvalid;
		CIPath m_string;
	};

	class ShortTempToken
	{
	public:
		Result Set(const Span<const Utf8Char_t> &span);
		Span<Utf8Char_t> GetChars();
		void Truncate(size_t truncatedSize);

	private:
		static const size_t kStaticBufferSize = 32;

		Vector<Utf8Char_t> m_chars;
		Utf8Char_t m_staticBuffer[kStaticBufferSize] = {};
		size_t m_length = 0;
	};

	struct AnalyzerIncludeStack
	{
		AnalyzerIncludeStack();
		AnalyzerIncludeStack(BuildFileLocation location, const CIPath &path, bool canTryAlternate);

		CIPath m_path;
		BuildFileLocation m_location = BuildFileLocation::kInvalid;
		bool m_canTryAlternate = false;
		bool m_isScanning = true;

		UniquePtr<utils::ITextParser> m_textParser;
		Vector<uint8_t> m_fileContents;
	};

	class LibraryAnalyzer final : public IStringResolver, public rpc_common::LibraryCompilerBase
	{
	public:
		typedef utils::EncodingTextParserProxy<Utf8Char_t, CharacterEncoding::kUTF8> TextParser_t;

		explicit LibraryAnalyzer(data::IDataDriver *dataDriver, IDependencyNodeCompilerFeedback *feedback);

		Result Run(IDependencyNode *depsNode);

		// StringResolver implementation
		StringSliceView ResolveGlobalString(size_t index) const override;
		StringSliceView ResolveConfigKey(size_t index) const override;
		StringSliceView ResolveTempString(size_t index) const override;
		Span<const uint8_t> ResolveBinaryContent(size_t index) const override;

	private:
		struct ConfigKey
		{
			render::GlobalStringIndex_t m_name;
			data::RenderRTTIMainType m_mainType = data::RenderRTTIMainType::Invalid;
		};

		struct SimpleNumericTypeResolution
		{
			SimpleNumericTypeResolution(const StringView &str, render::NumericType numericType)
				: m_name(str)
				, m_numericType(numericType)
			{
			}

			StringView m_name;
			render::NumericType m_numericType;
		};

		struct VertexInputFeedMapping
		{
			String m_name;
			render::InputLayoutVertexFeedDesc *m_feedDesc = nullptr;

			bool m_strideIsSetAutomatically = false;
			bool m_strideIsSet = false;
		};

		Result ScanTopStackItem(AnalyzerIncludeStack &item);
		Result ParseTopStackItem(AnalyzerIncludeStack &item);
		Result ParseDirective(const Utf8Char_t *filePath, TextParser_t &parser, bool &outHaveDirective);

		Result ParseIncludeDirective(const Utf8Char_t *filePath, TextParser_t &parser);

		template<class T>
		Result ParseEntity(const Utf8Char_t *blamePath, TextParser_t &parser, Result(LibraryAnalyzer:: *parseFunc)(const Utf8Char_t *, StringView, TextParser_t &, T &));

		Result ParseStructDef(const Utf8Char_t *filePath, StringView name, TextParser_t &parser, rpc_interchange::StructDefEntity &pc);
		Result ParseInputLayout(const Utf8Char_t *filePath, StringView name, TextParser_t &parser, rpc_interchange::InputLayoutEntity &il);
		Result ParsePipelineLayout(const Utf8Char_t *filePath, StringView name, TextParser_t &parser, rpc_interchange::PipelineLayoutEntity &pl);
		Result ParseGraphicsPipeline(const Utf8Char_t *filePath, StringView name, TextParser_t &parser, rpc_interchange::GraphicsPipelineEntity &gp);
		Result ParseRenderPass(const Utf8Char_t *filePath, StringView name, TextParser_t &parser, rpc_interchange::RenderPassEntity &gp);

		Result ResolveQuotedString(ShortTempToken &outToken, const Span<const Utf8Char_t> &inToken);
		Result ResolveInputLayoutVertexInputs(const Utf8Char_t *filePath, size_t line, size_t col, render::TempStringIndex_t feedName, const Span<const Utf8Char_t> &nameBase, Vector<const render::InputLayoutVertexInputDesc *> &descsVector, uint32_t &inOutOffset, const render::ValueType &inputSourcesType, render::InputLayoutVertexFeedDesc *inputFeed);
		Result ResolveInputLayoutVertexInputs(const Utf8Char_t *filePath, size_t line, size_t col, render::TempStringIndex_t feedName, const Span<const Utf8Char_t> &nameBase, Vector<const render::InputLayoutVertexInputDesc *> &descsVector, uint32_t &inOutOffset, const render::VectorOrScalarNumericType &inputSourcesType, render::InputLayoutVertexFeedDesc *inputFeed);
		static uint32_t ResolvePackedValueTypeSize(const render::ValueType &valueType);
		static uint32_t ResolvePackedStructureTypeSize(const render::StructureType &structureType);
		static uint32_t ResolveCompoundNumericTypeSize(render::NumericType numericType, render::VectorDimension rows, render::VectorDimension cols);
		static uint32_t ResolveVectorOrScalarNumericTypeSize(render::NumericType numericType, render::VectorOrScalarDimension dimension);
		static uint32_t ResolveVectorNumericTypeSize(render::NumericType numericType, render::VectorDimension dimension);
		static uint32_t ResolveNumericTypeSize(render::NumericType numericType);
		static render::VectorOrScalarDimension ConvertToMaybeScalar(render::VectorDimension dimension);

		Result ExpectIdentifier(const Utf8Char_t *blamePath, Span<const Utf8Char_t> &outToken, TextParser_t &parser);
		static Result CheckValidIdentifier(const Utf8Char_t *blamePath, const Span<const Utf8Char_t> &token, const TextParser_t &parser);

		Result ParseEnum(const Utf8Char_t *blamePath, const data::RenderRTTIEnumType *rtti, void *obj, bool isConfigurable, TextParser_t &parser);
		Result ParseNumber(const Utf8Char_t *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, TextParser_t &parser);
		Result ParseUInt(const Utf8Char_t *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const Utf8Char_t> &token, size_t line, size_t col);
		Result ParseSInt(const Utf8Char_t *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const Utf8Char_t> &token, size_t line, size_t col);
		Result ParseFloat(const Utf8Char_t *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const Utf8Char_t> &token, size_t line, size_t col);
		Result ParseBool(const Utf8Char_t *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const Utf8Char_t> &token, size_t line, size_t col);
		Result ParseStruct(const Utf8Char_t *blamePath, const data::RenderRTTIStructType *rtti, void *obj, TextParser_t &parser);
		Result ParseValue(const Utf8Char_t *blamePath, const data::RenderRTTITypeBase *rtti, void *obj, bool isConfigurable, TextParser_t &parser);
		Result ParseValueType(const Utf8Char_t *blamePath, render::ValueType &valueType, const Span<const Utf8Char_t> &token, TextParser_t &parser);
		Result ParseConfigurable(const Utf8Char_t *blamePath, void *obj, data::RenderRTTIMainType mainType, void (*writeNameFunc)(void *, const render::ConfigStringIndex_t &), TextParser_t &parser);
		Result ParseUIntConstant(const Utf8Char_t *blamePath, size_t blameLine, size_t blameCol, const Span<const Utf8Char_t> &token, uint64_t max, uint64_t &outValue);
		Result ParseStringIndex(const Utf8Char_t *blamePath, const data::RenderRTTIStringIndexType *rtti, void *obj, TextParser_t &parser);

		Result ParseStructMember(const Utf8Char_t *blamePath, const Span<const Utf8Char_t> &memberName, const data::RenderRTTIStructType *rtti, void *obj, TextParser_t &parser);

		template<class T>
		Result ParseDynamicStructMember(const Utf8Char_t *blamePath, const Span<const Utf8Char_t> &memberName, const data::RenderRTTIStructType *rtti, T &obj, TextParser_t &parser);

		static bool IsTokenChars(const Span<const Utf8Char_t> &span, const Utf8Char_t *tokenStr, size_t tokenLength);

		template<size_t TSize>
		static bool IsToken(const Span<const Utf8Char_t> &span, const Utf8Char_t(&tokenChars)[TSize]);

		template<class T>
		static Result Deduplicate(Vector<UniquePtr<T>> &instVector, const T &instance, const T *&outDeduplicated);

		Result IndexString(const Span<const Utf8Char_t> &span, render::GlobalStringIndex_t &outStringIndex);
		Result IndexString(const Span<const Utf8Char_t> &span, render::TempStringIndex_t &outStringIndex);

		Result IndexString(const render::GlobalStringIndex_t &globalIndex, render::TempStringIndex_t &outStringIndex);

		static bool IsBindingTyped(render::DescriptorBindingType descType);

		void RemoveTopStackItem();

		Result ExportPackages(IDependencyNode *depsNode) const;

		data::IDataDriver *m_dataDriver = nullptr;

		HashSet<IncludedFileKey> m_includedFiles;

		Vector<AnalyzerIncludeStack> m_includeStack;
		IDependencyNodeCompilerFeedback *m_feedback;

		HashMap<String, UniquePtr<rpc_interchange::Entity>> m_entities;
		StringPoolBuilder m_globalStringPool;

		HashMap<render::GlobalStringIndex_t, render::ConfigStringIndex_t> m_configNameToKey;
		HashMap<render::GlobalStringIndex_t, render::TempStringIndex_t> m_globalStringToTempString;
		Vector<ConfigKey> m_configKeys;
		Vector<render::GlobalStringIndex_t> m_tempStrings;

		Vector<UniquePtr<render::StructureMemberDesc>> m_smDescs;
		Vector<UniquePtr<render::InputLayoutVertexInputDesc>> m_vertexInputDescs;
		Vector<UniquePtr<render::RenderTargetDesc>> m_rtDescs;
		Vector<UniquePtr<render::RenderOperationDesc>> m_roDescs;
		Vector<UniquePtr<render::ShaderDesc>> m_shaderDescs;
		Vector<UniquePtr<render::DepthStencilOperationDesc>> m_depthStencilOperationDescs;
		Vector<UniquePtr<render::DepthStencilTargetDesc>> m_depthStencilTargetDescs;
		Vector<UniquePtr<render::DescriptorHeapBindingDesc>> m_dhbDescs;
		Vector<UniquePtr<render::LayoutInlineConstantDesc>> m_licDescs;
		Vector<UniquePtr<render::LayoutConstantBufferDesc>> m_lcbDescs;
		Vector<UniquePtr<render::ConstantBufferBindingDesc>> m_cbbDescs;


		Vector<render::GraphicsPipelineNameLookup> m_graphicsPipelines;
		Vector<render::RenderPassNameLookup> m_renderPasses;

		Vector<SimpleNumericTypeResolution> m_numericTypeResolutions;

		Vector<UniquePtr<render::CompoundNumericType>> m_compoundTypes;
		Vector<UniquePtr<render::VectorNumericType>> m_vectorTypes;
		Vector<UniquePtr<render::VectorOrScalarNumericType>> m_vectorOrScalarTypes;

		Vector<UniquePtr<render::InputLayoutVertexFeedDesc>> m_vertexFeedDescs;
	};
} // rkit::buildsystem::rpc_analyzer

namespace rkit::buildsystem::rpc_combiner
{
	class LibraryCombiner final : public IPipelineLibraryCombiner, public NoCopy
	{
	public:
		explicit LibraryCombiner(data::IDataDriver *dataDriver);

		Result AddInput(IReadStream &stream) override;
		Result WritePackage(ISeekableWriteStream &stream) override;

	private:
		class PackageInputResolver final : public IStringResolver, public NoCopy
		{
		public:
			PackageInputResolver(data::IRenderDataPackage &pkg, const Span<const Vector<uint8_t>> &binaryContent);

			virtual StringSliceView ResolveGlobalString(size_t index) const override;
			virtual StringSliceView ResolveConfigKey(size_t index) const override;
			virtual StringSliceView ResolveTempString(size_t index) const override;

			virtual Span<const uint8_t> ResolveBinaryContent(size_t index) const override;

		private:
			data::IRenderDataPackage &m_pkg;
			Span<const Vector<uint8_t>> m_binaryContent;
		};

		Result CheckInitObjects();

		UniquePtr<IPackageBuilder> m_pkgBuilder;
		UniquePtr<IPackageObjectWriter> m_pkgObjectWriter;

		HashSet<String> m_graphicPipelineNames;

		data::IDataDriver *m_dataDriver;
	};
} // rkit::buildsystem::rpc_combiner

namespace rkit::buildsystem::rpc_compiler
{
	class LibraryCompiler final : public rpc_common::LibraryCompilerBase
	{
	public:
		explicit LibraryCompiler(data::IDataDriver *dataDriver, IDependencyNodeCompilerFeedback *feedback);

		Result Run(IDependencyNode *depsNode);

	private:
		rpc_combiner::LibraryCombiner m_combiner;

		IDependencyNodeCompilerFeedback *m_feedback;
	};
} // rkit::buildsystem::rpc_compiler

namespace rkit
{
	template<>
	struct Hasher<rkit::buildsystem::rpc_analyzer::IncludedFileKey> : public DefaultSpanHasher<rkit::buildsystem::rpc_analyzer::IncludedFileKey>
	{
		static HashValue_t ComputeHash(HashValue_t baseHash, const rkit::buildsystem::rpc_analyzer::IncludedFileKey &key)
		{
			return key.ComputeHash(baseHash);
		}
	};
}

namespace rkit::buildsystem::rpc_common
{
	Result LibraryCompilerBase::FormatGraphicPipelinePath(CIPath &path, const StringView &identifier, size_t pipelineIndex)
	{
		String str;
		str.Format(u8"rpll/g_{}/{}", pipelineIndex, identifier);

		return path.Set(str);
	}

	Result LibraryCompilerBase::FormatGlobalsPath(CIPath &path, const StringView &identifier)
	{
		String str;
		str.Format(u8"rpll/globs/{}", identifier);

		return path.Set(str);
	}

	Result LibraryCompilerBase::FormatIndexPath(CIPath &path, const StringView &identifier)
	{
		String str;
		str.Format(u8"rpll/idx/{}", identifier);

		return path.Set(str);
	}

	Result LibraryCompilerBase::FormatCombinedOutputPath(CIPath &path, const StringView &identifier)
	{
		String str;
		str.Format(u8"rpll/out/{}", identifier);

		return path.Set(str);
	}
} // rkit::buildsystem::rpc_common

namespace rkit::buildsystem::rpc_analyzer
{
	IncludedFileKey::IncludedFileKey(BuildFileLocation location, const CIPath &str)
		: m_location(location)
		, m_string(str)
	{
	}

	HashValue_t IncludedFileKey::ComputeHash(HashValue_t baseHash) const
	{
		HashValue_t hash = baseHash;

		hash = Hasher<int32_t>::ComputeHash(hash, static_cast<int32_t>(m_location));
		hash = Hasher<CIPath>::ComputeHash(hash, m_string);

		return hash;
	}

	bool IncludedFileKey::operator==(const IncludedFileKey &other) const
	{
		return m_location == other.m_location && m_string == other.m_string;
	}

	bool IncludedFileKey::operator!=(const IncludedFileKey &other) const
	{
		return !((*this) == other);
	}


	Result ShortTempToken::Set(const Span<const Utf8Char_t> &span)
	{
		m_chars.Reset();
		m_length = 0;

		if (span.Count() <= kStaticBufferSize)
			memcpy(m_staticBuffer, span.Ptr(), span.Count());
		else
		{
			m_chars.Append(span);
		}

		m_length = span.Count();

		RKIT_RETURN_OK;
	}

	Span<Utf8Char_t> ShortTempToken::GetChars()
	{
		if (m_chars.Count() > 0)
			return Span<Utf8Char_t>(m_chars.GetBuffer(), m_length);
		else
			return Span<Utf8Char_t>(m_staticBuffer, m_length);
	}

	void ShortTempToken::Truncate(size_t newLength)
	{
		RKIT_ASSERT(m_length >= newLength);
		m_length = newLength;
	}

	AnalyzerIncludeStack::AnalyzerIncludeStack()
	{
	}

	AnalyzerIncludeStack::AnalyzerIncludeStack(BuildFileLocation location, const CIPath &path, bool canTryAlternate)
		: m_location(location)
		, m_path(path)
		, m_canTryAlternate(canTryAlternate)
	{
	}

	LibraryAnalyzer::LibraryAnalyzer(data::IDataDriver *dataDriver, IDependencyNodeCompilerFeedback *feedback)
		: m_feedback(feedback)
		, m_dataDriver(dataDriver)
	{
	}

	Result LibraryAnalyzer::Run(IDependencyNode *depsNode)
	{
		if (m_numericTypeResolutions.Count() == 0)
		{
			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"float", render::NumericType::Float32));
			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"half", render::NumericType::Float16));
			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"int", render::NumericType::SInt32));
			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"uint", render::NumericType::UInt32));

			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"double", render::NumericType::Float64));
			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"ulong", render::NumericType::UInt64));
			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"long", render::NumericType::SInt64));

			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"bool", render::NumericType::Bool));

			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"byte", render::NumericType::UInt8));
			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"sbyte", render::NumericType::SInt8));

			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"short", render::NumericType::SInt16));
			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"ushort", render::NumericType::UInt16));

			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"nbyte", render::NumericType::UNorm8));
			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"nushort", render::NumericType::UNorm16));

			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"nsbyte", render::NumericType::SNorm8));
			m_numericTypeResolutions.Append(SimpleNumericTypeResolution(u8"nshort", render::NumericType::SNorm16));
		}

		CIPath path;
		path.Set(depsNode->GetIdentifier());

		m_includeStack.Append(AnalyzerIncludeStack(depsNode->GetInputFileLocation(), path, false));

		while (m_includeStack.Count() > 0)
		{
			AnalyzerIncludeStack &topItem = m_includeStack[m_includeStack.Count() - 1];

			if (topItem.m_isScanning)
			{
				ScanTopStackItem(topItem);
			}
			else
			{
				ParseTopStackItem(topItem);
			}
		}

		ExportPackages(depsNode);

		// Generate graphics pipeline shader jobs

		RKIT_RETURN_OK;
	}

	StringSliceView LibraryAnalyzer::ResolveGlobalString(size_t index) const
	{
		return m_globalStringPool.GetStringByIndex(index);
	}

	StringSliceView LibraryAnalyzer::ResolveConfigKey(size_t index) const
	{
		return m_globalStringPool.GetStringByIndex(m_configKeys[index].m_name.GetIndex());
	}

	StringSliceView LibraryAnalyzer::ResolveTempString(size_t index) const
	{
		return m_globalStringPool.GetStringByIndex(m_tempStrings[index].GetIndex());
	}

	Span<const uint8_t> LibraryAnalyzer::ResolveBinaryContent(size_t index) const
	{
		RKIT_ASSERT(false);
		return Span<const uint8_t>();
	}

	Result LibraryAnalyzer::ScanTopStackItem(AnalyzerIncludeStack &item)
	{
		bool exists = false;

		IncludedFileKey fKey(item.m_location, item.m_path);

		if (m_includedFiles.Contains(fKey))
		{
			RemoveTopStackItem();
			RKIT_RETURN_OK;
		}

		m_includedFiles.Add(fKey);

		UniquePtr<ISeekableReadStream> stream;
		m_feedback->TryOpenInput(item.m_location, item.m_path, stream);

		if (!stream.IsValid())
		{
			if (item.m_canTryAlternate)
			{
				if (item.m_location == BuildFileLocation::kIntermediateDir)
				{
					item.m_location = BuildFileLocation::kSourceDir;
					RKIT_RETURN_OK;
				}

				rkit::log::ErrorFmt(u8"Could not open input file '{}'", item.m_path.ToString());
				RKIT_THROW(ResultCode::kFileOpenError);
			}
		}

		IUtilitiesDriver &utils = *GetDrivers().m_utilitiesDriver;

		Vector<uint8_t> streamBytes;
		utils.ReadEntireFile(*stream, streamBytes);

		item.m_fileContents = std::move(streamBytes);

		if (!CharacterEncodingValidator<CharacterEncoding::kUTF8>::ValidateSpan<uint8_t>(item.m_fileContents.ToSpan()))
		{
			rkit::log::ErrorFmt(u8"Input file '{}' contained invalid Unicode characters", item.m_path.ToString());
			RKIT_THROW(ResultCode::kInvalidUnicode);
		}

		utils.CreateTextParser(item.m_fileContents.ToSpan(), utils::TextParserCommentType::kC, utils::TextParserLexerType::kC, item.m_textParser);
		item.m_isScanning = false;

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::ParseTopStackItem(AnalyzerIncludeStack &item)
	{
		utils::ITextParser *parserBase = item.m_textParser.Get();

		TextParser_t parser(*parserBase);

		parser.SkipWhitespace();

		bool haveMore = true;
		while (haveMore)
		{
			haveMore = false;

			size_t oldStackCount = m_includeStack.Count();
			ParseDirective(item.m_path.CStr(), parser, haveMore);

			if (oldStackCount != m_includeStack.Count())
				RKIT_RETURN_OK;
		}

		// Parsing completed
		RemoveTopStackItem();

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::ParseDirective(const Utf8Char_t *path, TextParser_t &parser, bool &outHaveDirective)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		bool haveToken = false;
		rkit::Span<const Utf8Char_t> directiveToken;
		parser.ReadToken(haveToken, directiveToken);

		if (!haveToken)
		{
			outHaveDirective = false;
			RKIT_RETURN_OK;
		}

		outHaveDirective = true;

		auto parseEntityBlock = [this, &directiveToken, path, &parser, line, col]() -> Result
			{
				if (IsToken(directiveToken, u8"include"))
					return ParseIncludeDirective(path, parser);
				else if (IsToken(directiveToken, u8"struct"))
					return ParseEntity(path, parser, &LibraryAnalyzer::ParseStructDef);
				else if (IsToken(directiveToken, u8"InputLayout"))
					return ParseEntity(path, parser, &LibraryAnalyzer::ParseInputLayout);
				else if (IsToken(directiveToken, u8"PipelineLayout"))
					return ParseEntity(path, parser, &LibraryAnalyzer::ParsePipelineLayout);
				else if (IsToken(directiveToken, u8"GraphicsPipeline"))
					return ParseEntity(path, parser, &LibraryAnalyzer::ParseGraphicsPipeline);
				else if (IsToken(directiveToken, u8"RenderPass"))
					return ParseEntity(path, parser, &LibraryAnalyzer::ParseRenderPass);
				else
				{
					rkit::log::ErrorFmt(u8"{} [{}:{}] Invalid directive", path, line, col);
					RKIT_THROW(ResultCode::kMalformedFile);
				}
			};

		RKIT_TRY_CATCH_RETHROW(parseEntityBlock(),
			CatchContext
			(
				[path, line, col]
				{
					rkit::log::ErrorFmt(u8"{} [{}:{}] Directive parsing failed", path, line, col);
				}
			)
		);

		RKIT_RETURN_OK;
	}

	template<class T>
	Result LibraryAnalyzer::ParseEntity(const Utf8Char_t *blamePath, TextParser_t &parser, Result(LibraryAnalyzer:: *parseFunc)(const Utf8Char_t *, StringView, TextParser_t &, T &))
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		Span<const Utf8Char_t> entityNameSpan;
		ExpectIdentifier(blamePath, entityNameSpan, parser);

		String entityName;
		entityName.Set(entityNameSpan);

		if (m_entities.Find(entityName) != m_entities.end())
		{
			rkit::log::ErrorFmt(u8"{} [{}:{}] Object with this name already exists", blamePath, line, col);
			RKIT_THROW(ResultCode::kMalformedFile);
		}

		UniquePtr<rpc_interchange::Entity> entity = New<T>();

		entity->SetName(entityName);

		T *obj = static_cast<T *>(entity.Get());

		(this->*parseFunc)(blamePath, entityName, parser, *obj);

		m_entities.Set(entityName, std::move(entity));
	}

	Result LibraryAnalyzer::ParseStructDef(const Utf8Char_t *blamePath, StringView name, TextParser_t &parser, rpc_interchange::StructDefEntity &sd)
	{
		render::TempStringIndex_t structNameTSI;
		IndexString(name.ToSpan(), structNameTSI);

		parser.ExpectToken(u8"{");

		for (;;)
		{
			Span<const Utf8Char_t> typeToken;
			Span<const Utf8Char_t> nameToken;

			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);

			parser.RequireToken(typeToken);

			if (IsToken(typeToken, u8"}"))
				break;

			CheckValidIdentifier(blamePath, typeToken, parser);

			render::ValueType valueType;
			ParseValueType(blamePath, valueType, typeToken, parser);

			ExpectIdentifier(blamePath, nameToken, parser);

			render::TempStringIndex_t tsi;
			IndexString(nameToken, tsi);

			for (const render::StructureMemberDesc *smDesc : sd.GetStructMembersVector())
			{
				if (smDesc->m_name == tsi)
				{
					rkit::log::ErrorFmt(u8"{} [{}:{}] Struct member with this name already exists", blamePath, line, col);
					RKIT_THROW(ResultCode::kMalformedFile);
				}
			}

			if (IsToken(nameToken, u8"}"))
				break;

			UniquePtr<render::StructureMemberDesc> smDesc = New<render::StructureMemberDesc>();

			IndexString(nameToken, smDesc->m_name);
			smDesc->m_type = valueType;

			sd.GetStructMembersVector().Append(smDesc.Get());
			this->m_smDescs.Append(std::move(smDesc));
		}

		sd.GetDesc().m_name = structNameTSI;
		sd.GetDesc().m_members = sd.GetStructMembersVector().ToSpan();
	}

	Result LibraryAnalyzer::ParseInputLayout(const Utf8Char_t *blamePath, StringView name, TextParser_t &parser, rpc_interchange::InputLayoutEntity &il)
	{
		parser.ExpectToken(u8"{");

		Vector<VertexInputFeedMapping> feedMappings;

		size_t line = 0;
		size_t col = 0;

		bool hasNumberedMappings = false;
		bool hasSequentialMappings = false;
		for (;;)
		{
			Span<const Utf8Char_t> token;
			parser.RequireToken(token);

			parser.GetLocation(line, col);

			if (IsToken(token, u8"}"))
				break;

			if (IsToken(token, u8"VertexInputFeeds"))
			{
				parser.ExpectToken(u8"=");
				parser.ExpectToken(u8"{");

				parser.GetLocation(line, col);
				parser.RequireToken(token);

				uint32_t slotIndex = 0;

				for (;;)
				{
					if (IsToken(token, u8"}"))
						break;

					VertexInputFeedMapping feedMapping;
					feedMapping.m_name.Set(token);

					UniquePtr<render::InputLayoutVertexFeedDesc> feedDesc = New<render::InputLayoutVertexFeedDesc>();

					feedMapping.m_feedDesc = feedDesc.Get();
					m_vertexFeedDescs.Append(std::move(feedDesc));



					parser.GetLocation(line, col);
					parser.RequireToken(token);

					bool isExplicitlyNumbered = false;
					if (IsToken(token, u8"="))
					{
						parser.ExpectToken(u8"{");

						for (;;)
						{
							parser.GetLocation(line, col);
							parser.RequireToken(token);

							if (IsToken(token, u8"InputSlot"))
							{
								parser.ExpectToken(u8"=");
								parser.RequireToken(token);

								uint64_t value = 0;
								ParseUIntConstant(blamePath, line, col, token, std::numeric_limits<uint32_t>::max(), value);

								isExplicitlyNumbered = true;
								feedMapping.m_feedDesc->m_inputSlot = static_cast<uint32_t>(value);
							}
							else if (IsToken(token, u8"Stride"))
							{
								parser.ExpectToken(u8"=");
								parser.RequireToken(token);

								uint64_t value = 0;
								ParseUIntConstant(blamePath, line, col, token, std::numeric_limits<uint32_t>::max(), value);

								feedMapping.m_strideIsSet = true;
								feedMapping.m_feedDesc->m_byteStride.Set(static_cast<uint32_t>(value));
							}
							else if (IsToken(token, u8"Stepping"))
							{
								parser.ExpectToken(u8"=");

								const data::RenderRTTIEnumType *enumType = m_dataDriver->GetRenderDataHandler()->GetInputLayoutVertexInputSteppingRTTI();
								ParseEnum(blamePath, enumType, &feedMapping.m_feedDesc->m_stepping, true, parser);
							}
							else if (IsToken(token, u8"}"))
								break;
							else
							{
								rkit::log::ErrorFmt(u8"{} [{}:{}] Unknown field for input feed", blamePath, line, col);
								RKIT_THROW(ResultCode::kMalformedFile);
							}
						}

						parser.GetLocation(line, col);
						parser.RequireToken(token);
					}

					if (isExplicitlyNumbered)
					{
						if (hasSequentialMappings)
						{
							rkit::log::ErrorFmt(u8"{} [{}:{}] Can't mix numbered and sequential input feed mappings", blamePath, line, col);
							RKIT_THROW(ResultCode::kMalformedFile);
						}

						hasNumberedMappings = true;

						for (const VertexInputFeedMapping &existingMapping : feedMappings)
						{
							if (existingMapping.m_feedDesc->m_inputSlot == feedMapping.m_feedDesc->m_inputSlot)
							{
								rkit::log::ErrorFmt(u8"{} [{}:{}] Multiple feeds mapped to the same slot", blamePath, line, col);
								RKIT_THROW(ResultCode::kMalformedFile);
							}
						}
					}
					else
					{
						if (hasNumberedMappings)
						{
							rkit::log::ErrorFmt(u8"{} [{}:{}] Can't mix numbered and sequential input feed mappings", blamePath, line, col);
							RKIT_THROW(ResultCode::kMalformedFile);
						}

						hasSequentialMappings = true;

						if (slotIndex == std::numeric_limits<uint32_t>::max())
						{
							rkit::log::ErrorFmt(u8"{} [{}:{}] Too many input slots", blamePath, line, col);
							RKIT_THROW(ResultCode::kMalformedFile);
						}

						feedMapping.m_feedDesc->m_inputSlot = slotIndex++;
					}

					IndexString(feedMapping.m_name.ToSpan(), feedMapping.m_feedDesc->m_feedName);

					feedMappings.Append(std::move(feedMapping));
				}
			}
			else if (IsToken(token, u8"VertexInputs"))
			{
				parser.ExpectToken(u8"=");
				parser.ExpectToken(u8"{");

				for (;;)
				{
					parser.RequireToken(token);

					if (IsToken(token, u8"}"))
						break;

					CheckValidIdentifier(blamePath, token, parser);

					render::TempStringIndex_t inputNameStr;
					IndexString(token, inputNameStr);

					parser.ExpectToken(u8"=");
					parser.ExpectToken(u8"{");

					uint32_t baseOffset = 0;

					render::ValueType inputSourcesType;
					bool haveInputSourcesType = false;

					VertexInputFeedMapping *inputFeedMapping = nullptr;

					size_t defStartLine = 0;
					size_t defStartCol = 0;
					parser.GetLocation(defStartLine, defStartCol);

					for (;;)
					{
						parser.GetLocation(line, col);
						parser.RequireToken(token);

						if (IsToken(token, u8"}"))
							break;

						if (IsToken(token, u8"InputFeed"))
						{
							parser.ExpectToken(u8"=");

							parser.GetLocation(line, col);
							parser.RequireToken(token);

							for (VertexInputFeedMapping &feedMapping : feedMappings)
							{
								if (CompareSpansEqual(feedMapping.m_name.ToSpan(), token))
								{
									inputFeedMapping = &feedMapping;
									break;
								}
							}

							if (!inputFeedMapping)
							{
								rkit::log::ErrorFmt(u8"{} [{}:{}] Unknown input feed", blamePath, line, col);
								RKIT_THROW(ResultCode::kMalformedFile);
							}
						}
						else if (IsToken(token, u8"InputSources"))
						{
							parser.ExpectToken(u8"=");

							parser.RequireToken(token);

							ParseValueType(blamePath, inputSourcesType, token, parser);
							haveInputSourcesType = true;
						}
						else if (IsToken(token, u8"BaseOffset"))
						{
							parser.ExpectToken(u8"=");

							parser.GetLocation(line, col);
							parser.RequireToken(token);

							uint64_t baseOffsetValue = 0;
							ParseUIntConstant(blamePath, line, col, token, std::numeric_limits<uint32_t>::max(), baseOffsetValue);

							baseOffset = static_cast<uint32_t>(baseOffsetValue);
						}
						else
						{
							rkit::log::ErrorFmt(u8"{} [{}:{}] Invalid field for vertex input", blamePath, line, col);
							RKIT_THROW(ResultCode::kMalformedFile);
						}
					}

					if (!inputFeedMapping)
					{
						rkit::log::ErrorFmt(u8"{} [{}:{}] No input feeds were defined", blamePath, line, col);
						RKIT_THROW(ResultCode::kMalformedFile);
					}

					if (!haveInputSourcesType)
					{
						rkit::log::ErrorFmt(u8"{} [{}:{}] No input source type was defined", blamePath, line, col);
						RKIT_THROW(ResultCode::kMalformedFile);
					}

					ResolveInputLayoutVertexInputs(blamePath, defStartLine, defStartCol, inputNameStr, Span<const Utf8Char_t>(), il.GetVertexInputs(), baseOffset, inputSourcesType, inputFeedMapping->m_feedDesc);

					if (!inputFeedMapping->m_strideIsSet || inputFeedMapping->m_strideIsSetAutomatically)
					{
						uint32_t autoStride = ResolvePackedValueTypeSize(inputSourcesType);

						if (inputFeedMapping->m_strideIsSetAutomatically && autoStride != inputFeedMapping->m_feedDesc->m_byteStride.m_u.m_value)
						{
							rkit::log::ErrorFmt(u8"{} [{}:{}] Automatic stride mismatch", blamePath, line, col);
							RKIT_THROW(ResultCode::kMalformedFile);
						}

						if (!inputFeedMapping->m_strideIsSet)
						{
							inputFeedMapping->m_strideIsSet = true;
							inputFeedMapping->m_strideIsSetAutomatically = true;
							inputFeedMapping->m_feedDesc->m_byteStride.Set(autoStride);
						}
					}
				}
			}
			else
			{
				rkit::log::ErrorFmt(u8"{} [{}:{}] Invalid entry type in InputLayout", blamePath, line, col);
				RKIT_THROW(ResultCode::kMalformedFile);
			}
		}

		for (const VertexInputFeedMapping &feedMapping : feedMappings)
		{
			if (!feedMapping.m_strideIsSet)
			{
				parser.GetLocation(line, col);
				rkit::log::ErrorFmt(u8"{} [{}:{}] Feed mapping '{}' had no stride", blamePath, line, col, feedMapping.m_name.CStr());
				RKIT_THROW(ResultCode::kMalformedFile);
			}
		}

		il.GetDesc().m_vertexInputs = il.GetVertexInputs().ToSpan();

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::ParsePipelineLayout(const Utf8Char_t *filePath, StringView name, TextParser_t &parser, rpc_interchange::PipelineLayoutEntity &pl)
	{
		parser.ExpectToken(u8"{");

		const data::RenderRTTIStructType *pipelineLayoutRTTI = m_dataDriver->GetRenderDataHandler()->GetPipelineLayoutDescRTTI();
		const data::RenderRTTIStructType *inlineConstantRTTI = m_dataDriver->GetRenderDataHandler()->GetLayoutInlineConstantDescRTTI();
		const data::RenderRTTIStructType *constantBufferRTTI = m_dataDriver->GetRenderDataHandler()->GetLayoutConstantBufferRTTI();

		auto haveExistingCB = [&pl](const render::TempStringIndex_t& nameStr) -> bool
			{
				for (const render::LayoutConstantBufferDesc *existingDesc : pl.GetLayoutConstantBuffers())
				{
					if (existingDesc->m_name == nameStr)
						return true;
				}

				for (const render::LayoutInlineConstantDesc *existingDesc : pl.GetLayoutInlineConstants())
				{
					if (existingDesc->m_name == nameStr)
						return true;
				}

				return false;
			};

		for (;;)
		{
			Span<const Utf8Char_t> token;
			parser.RequireToken(token);

			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);

			if (IsToken(token, u8"}"))
				break;

			if (IsToken(token, u8"InlineConstants"))
			{
				parser.ExpectToken(u8"=");
				parser.ExpectToken(u8"{");

				for (;;)
				{
					parser.GetLocation(line, col);

					parser.RequireToken(token);

					if (IsToken(token, u8"}"))
						break;

					render::TempStringIndex_t nameStr;
					IndexString(token, nameStr);

					if (haveExistingCB(nameStr))
					{
						rkit::log::ErrorFmt(u8"{} [{}:{}] Constant buffer was specified multiple times", filePath, line, col);
						RKIT_THROW(ResultCode::kMalformedFile);
					}

					parser.ExpectToken(u8"=");

					rkit::UniquePtr<render::LayoutInlineConstantDesc> ic = rkit::New<render::LayoutInlineConstantDesc>();
					ParseStruct(filePath, inlineConstantRTTI, ic.Get(), parser);

					ic->m_name = nameStr;
					pl.GetLayoutInlineConstants().Append(ic.Get());

					m_licDescs.Append(std::move(ic));
				}
			}
			else if (IsToken(token, u8"ConstantBufferBindings"))
			{
				parser.ExpectToken(u8"=");
				parser.ExpectToken(u8"{");

				for (;;)
				{
					parser.GetLocation(line, col);

					parser.RequireToken(token);

					if (IsToken(token, u8"}"))
						break;

					render::TempStringIndex_t nameStr;
					IndexString(token, nameStr);

					if (haveExistingCB(nameStr))
					{
						rkit::log::ErrorFmt(u8"{} [{}:{}] Constant buffer was specified multiple times", filePath, line, col);
						RKIT_THROW(ResultCode::kMalformedFile);
					}

					parser.ExpectToken(u8"=");

					rkit::UniquePtr<render::LayoutConstantBufferDesc> cb = rkit::New<render::LayoutConstantBufferDesc>();
					ParseStruct(filePath, constantBufferRTTI, cb.Get(), parser);

					cb->m_name = nameStr;
					pl.GetLayoutConstantBuffers().Append(cb.Get());

					m_lcbDescs.Append(std::move(cb));
				}
			}
			else
			{
				ParseStructMember(filePath, token, pipelineLayoutRTTI, &pl.GetDesc(), parser);
			}
		}

		pl.GetDesc().m_constantBuffers = pl.GetLayoutConstantBuffers().ToSpan();
		pl.GetDesc().m_inlineConstants = pl.GetLayoutInlineConstants().ToSpan();
	}

	Result LibraryAnalyzer::ParseGraphicsPipeline(const Utf8Char_t *filePath, StringView name, TextParser_t &parser, rpc_interchange::GraphicsPipelineEntity &gp)
	{
		parser.ExpectToken(u8"{");

		const data::RenderRTTIStructType *pipelineRTTI = m_dataDriver->GetRenderDataHandler()->GetGraphicsPipelineDescRTTI();
		const data::RenderRTTIStructType *shaderDescRTTI = m_dataDriver->GetRenderDataHandler()->GetShaderDescRTTI();
		const data::RenderRTTIStructType *depthStencilOperationDescRTTI = m_dataDriver->GetRenderDataHandler()->GetDepthStencilOperationDescRTTI();
		const data::RenderRTTIEnumType *descriptorBindingTypeRTTI = m_dataDriver->GetRenderDataHandler()->GetDescriptorBindingTypeRTTI();

		HashMap<String, const render::RenderOperationDesc *> nameToRODesc;

		for (;;)
		{
			size_t line = 0;
			size_t col = 0;
			Span<const Utf8Char_t> token;
			parser.RequireToken(token);

			parser.GetLocation(line, col);

			if (IsToken(token, u8"}"))
				break;

			if (IsToken(token, u8"ConstantBuffers"))
			{
				parser.ExpectToken(u8"=");
				parser.ExpectToken(u8"{");

				for (;;)
				{
					parser.GetLocation(line, col);

					parser.RequireToken(token);

					if (IsToken(token, u8"}"))
						break;

					render::TempStringIndex_t nameStr;
					IndexString(token, nameStr);

					const render::PipelineLayoutDesc *pipelineLayout = gp.GetDesc().m_pipelineLayout;
					if (!pipelineLayout)
					{
						rkit::log::ErrorFmt(u8"{} [{}:{}] Pipeline layout must be specified before binding a constant buffer", filePath, line, col);
						RKIT_THROW(ResultCode::kMalformedFile);
					}

					size_t index = 0;
					for (const render::LayoutConstantBufferDesc *bufferDesc : pipelineLayout->m_constantBuffers)
					{
						if (bufferDesc->m_name == nameStr)
							break;

						index++;
					}

					if (index == pipelineLayout->m_constantBuffers.Count())
					{
						rkit::log::ErrorFmt(u8"{} [{}:{}] Constant buffer binding not found", filePath, line, col);
						RKIT_THROW(ResultCode::kMalformedFile);
					}

					parser.ExpectToken(u8"=");
					parser.RequireToken(token);

					render::ValueType valueType;
					ParseValueType(filePath, valueType, token, parser);

					rkit::UniquePtr<render::ConstantBufferBindingDesc> cbBinding = rkit::New<render::ConstantBufferBindingDesc>();
					cbBinding->m_bindingIndex = static_cast<uint32_t>(index);
					cbBinding->m_valueType = valueType;

					gp.GetConstantBufferBindings().Append(cbBinding.Get());

					m_cbbDescs.Append(std::move(cbBinding));
				}
			}
			else if (IsToken(token, u8"RenderTargets"))
			{
				parser.ExpectToken(u8"=");
				parser.ExpectToken(u8"{");

				for (;;)
				{
					parser.GetLocation(line, col);

					parser.RequireToken(token);

					if (IsToken(token, u8"}"))
						break;

					CheckValidIdentifier(filePath, token, parser);

					String rtName;
					rtName.Set(token);

					UniquePtr<render::RenderOperationDesc> roDesc = New<render::RenderOperationDesc>();

					parser.ExpectToken(u8"=");

					if (nameToRODesc.Find(rtName) != nameToRODesc.end())
					{
						rkit::log::ErrorFmt(u8"{} [{}:{}] Render target '{}' was specified multiple times", filePath, line, col, rtName.CStr());
						RKIT_THROW(ResultCode::kMalformedFile);
					}

					ParseStruct(filePath, m_dataDriver->GetRenderDataHandler()->GetRenderOperationDescRTTI(), roDesc.Get(), parser);

					nameToRODesc.Set(std::move(rtName), roDesc.Get());

					m_roDescs.Append(std::move(roDesc));
				}
			}
			else if (IsToken(token, u8"DescriptorHeaps"))
			{

				parser.ExpectToken(u8"=");
				parser.ExpectToken(u8"{");

				for (;;)
				{
					parser.GetLocation(line, col);

					parser.RequireToken(token);

					if (IsToken(token, u8"}"))
						break;

					render::TempStringIndex_t nameStr;
					IndexString(token, nameStr);

					parser.ExpectToken(u8"=");

					render::DescriptorBindingType bindingType = render::DescriptorBindingType::Count;
					ParseEnum(filePath, descriptorBindingTypeRTTI, &bindingType, false, parser);

					render::ValueType valueType;

					if (IsBindingTyped(bindingType))
					{
						parser.ExpectToken(u8"<");
						parser.RequireToken(token);
						ParseValueType(filePath, valueType, token, parser);
						parser.ExpectToken(u8">");
					}
					


					ParseValueType(filePath, valueType, token, parser);

					rkit::UniquePtr<render::DescriptorHeapBindingDesc> dhBinding = rkit::New<render::DescriptorHeapBindingDesc>();
					dhBinding->m_name = nameStr;
					dhBinding->m_type = bindingType;
					dhBinding->m_valueType = valueType;

					gp.GetDescriptorHeapBindings().Append(dhBinding.Get());

					m_dhbDescs.Append(std::move(dhBinding));
				}
			}
			else if (IsToken(token, u8"PipelineLayout"))
			{
				parser.ExpectToken(u8"=");

				parser.GetLocation(line, col);
				parser.RequireToken(token);

				HashMap<String, UniquePtr<rpc_interchange::Entity>>::ConstIterator_t it = m_entities.Find(StringSliceView(token));
				if (it == m_entities.end())
				{
					rkit::log::ErrorFmt(u8"{} [{}:{}] Unknown pipeline layout identifier", filePath, line, col);
					RKIT_THROW(ResultCode::kMalformedFile);
				}

				if (it.Value()->GetEntityType() != rpc_interchange::EntityType::PipelineLayout)
				{
					rkit::log::ErrorFmt(u8"{} [{}:{}] Value wasn't a pipeline layout", filePath, line, col);
					RKIT_THROW(ResultCode::kMalformedFile);
				}

				gp.GetDesc().m_pipelineLayout = &static_cast<rpc_interchange::PipelineLayoutEntity *>(it.Value().Get())->GetDesc();
			}
			else if (IsToken(token, u8"VertexShaderOutput"))
			{
				RKIT_THROW(ResultCode::kNotYetImplemented);
			}
			else if (IsToken(token, u8"InputLayout"))
			{
				parser.ExpectToken(u8"=");

				parser.GetLocation(line, col);
				parser.RequireToken(token);

				HashMap<String, UniquePtr<rpc_interchange::Entity>>::ConstIterator_t it = m_entities.Find(StringSliceView(token));
				if (it == m_entities.end())
				{
					rkit::log::ErrorFmt(u8"{} [{}:{}] Unknown input layout identifier", filePath, line, col);
					RKIT_THROW(ResultCode::kMalformedFile);
				}

				if (it.Value()->GetEntityType() != rpc_interchange::EntityType::InputLayout)
				{
					rkit::log::ErrorFmt(u8"{} [{}:{}] Value wasn't an input layout", filePath, line, col);
					RKIT_THROW(ResultCode::kMalformedFile);
				}

				gp.GetDesc().m_inputLayout = &static_cast<rpc_interchange::InputLayoutEntity *>(it.Value().Get())->GetDesc();
			}
			else if (IsToken(token, u8"VertexShader") || IsToken(token, u8"PixelShader"))
			{
				bool isVS = IsToken(token, u8"VertexShader");
				bool isPS = IsToken(token, u8"PixelShader");

				parser.ExpectToken(u8"=");

				UniquePtr<render::ShaderDesc> shaderDesc = New<render::ShaderDesc>();

				ParseStruct(filePath, shaderDescRTTI, shaderDesc.Get(), parser);

				if (isVS)
					gp.GetDesc().m_vertexShader = shaderDesc.Get();

				if (isPS)
					gp.GetDesc().m_pixelShader = shaderDesc.Get();

				m_shaderDescs.Append(std::move(shaderDesc));
			}
			else if (IsToken(token, u8"DepthStencil"))
			{
				parser.ExpectToken(u8"=");

				UniquePtr<render::DepthStencilOperationDesc> depthStencil = New<render::DepthStencilOperationDesc>();

				ParseStruct(filePath, depthStencilOperationDescRTTI, depthStencil.Get(), parser);

				gp.GetDesc().m_depthStencil = depthStencil.Get();

				m_depthStencilOperationDescs.Append(std::move(depthStencil));
			}
			else if (IsToken(token, u8"ExecuteInPass"))
			{
				parser.ExpectToken(u8"=");

				parser.GetLocation(line, col);
				parser.RequireToken(token);

				HashMap<String, UniquePtr<rpc_interchange::Entity>>::ConstIterator_t it = m_entities.Find(StringSliceView(token));
				if (it == m_entities.end())
				{
					rkit::log::ErrorFmt(u8"{} [{}:{}] Unknown execute in pass identifier", filePath, line, col);
					RKIT_THROW(ResultCode::kMalformedFile);
				}

				if (it.Value()->GetEntityType() != rpc_interchange::EntityType::RenderPass)
				{
					rkit::log::ErrorFmt(u8"{} [{}:{}] Value wasn't a render pass", filePath, line, col);
					RKIT_THROW(ResultCode::kMalformedFile);
				}

				gp.GetDesc().m_executeInPass = &static_cast<rpc_interchange::RenderPassEntity *>(it.Value().Get())->GetDesc();
			}
			else
			{
				ParseStructMember(filePath, token, pipelineRTTI, &gp.GetDesc(), parser);
			}
		}

		if (gp.GetDesc().m_pipelineLayout == nullptr)
		{
			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);
			rkit::log::ErrorFmt(u8"{} [{}:{}] Pipeline did not specify PipelineLayout", filePath, line, col);
			RKIT_THROW(ResultCode::kMalformedFile);
		}

		const render::RenderPassDesc *renderPass = gp.GetDesc().m_executeInPass;

		if (!renderPass)
		{
			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);
			rkit::log::ErrorFmt(u8"{} [{}:{}] Pipeline did not specify ExecuteInPass", filePath, line, col);
			RKIT_THROW(ResultCode::kMalformedFile);
		}

		if ((renderPass->m_depthStencilTarget == nullptr) && (gp.GetDesc().m_depthStencil != nullptr))
		{
			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);
			rkit::log::ErrorFmt(u8"{} [{}:{}] Pipeline has depth/stencil operations but there is no depth/stencil in the corresponding render pass", filePath, line, col);
			RKIT_THROW(ResultCode::kMalformedFile);
		}

		if ((renderPass->m_depthStencilTarget != nullptr) && (gp.GetDesc().m_depthStencil == nullptr))
		{
			UniquePtr<render::DepthStencilOperationDesc> depthStencilDesc = New<render::DepthStencilOperationDesc>();

			depthStencilDesc->m_depthTest.Set(false);
			depthStencilDesc->m_depthWrite.Set(false);

			gp.GetDesc().m_depthStencil = depthStencilDesc.Get();

			m_depthStencilOperationDescs.Append(std::move(depthStencilDesc));
		}

		{
			const ConstSpan<const render::RenderTargetDesc *> renderTargets = renderPass->m_renderTargets;

			size_t numRenderTargets = renderTargets.Count();

			Vector<bool> rtUsed;
			rtUsed.Resize(numRenderTargets);

			for (bool &b : rtUsed)
				b = false;

			gp.GetRenderOperations().Resize(numRenderTargets);

			for (HashMapKeyValueView<String, const render::RenderOperationDesc *> kv : nameToRODesc)
			{
				render::TempStringIndex_t nameStr;
				IndexString(kv.Key().ToSpan(), nameStr);

				bool matched = false;
				for (size_t rti = 0; rti < numRenderTargets; rti++)
				{
					if (renderTargets[rti]->m_name == nameStr)
					{
						rtUsed[rti] = true;
						gp.GetRenderOperations()[rti] = kv.Value();
						matched = true;
						break;
					}
				}

				if (!matched)
				{
					size_t line = 0;
					size_t col = 0;
					parser.GetLocation(line, col);
					rkit::log::ErrorFmt(u8"{} [{}:{}] Pipeline target '{}' didn't exist in the render pass", filePath, line, col, kv.Key().CStr());
				}
			}

			const render::RenderOperationDesc *defaultRODesc = nullptr;

			for (size_t i = 0; i < rtUsed.Count(); i++)
			{
				if (!rtUsed[i])
				{
					if (!defaultRODesc)
					{
						UniquePtr<render::RenderOperationDesc> defaultRODescNew = New<render::RenderOperationDesc>();

						defaultRODescNew->m_access = render::ReadWriteAccess::Read;	// FIXME: None?
						defaultRODescNew->m_srcBlend = render::ColorBlendFactor::Zero;
						defaultRODescNew->m_dstBlend = render::ColorBlendFactor::One;
						defaultRODescNew->m_srcAlphaBlend = render::AlphaBlendFactor::Zero;
						defaultRODescNew->m_dstAlphaBlend = render::AlphaBlendFactor::One;
						defaultRODescNew->m_writeAlpha = false;
						defaultRODescNew->m_writeRed = false;
						defaultRODescNew->m_writeGreen = false;
						defaultRODescNew->m_writeBlue = false;

						defaultRODesc = defaultRODescNew.Get();

						this->m_roDescs.Append(std::move(defaultRODescNew));
					}

					gp.GetRenderOperations()[i] = defaultRODesc;
				}
			}
		}

		gp.GetDesc().m_renderTargets = gp.GetRenderOperations().ToSpan();
		gp.GetDesc().m_constantBufferBindings = gp.GetConstantBufferBindings().ToSpan();
		gp.GetDesc().m_descriptorHeapBindings = gp.GetDescriptorHeapBindings().ToSpan();

		render::GraphicsPipelineNameLookup nameLookup;
		IndexString(gp.GetName().ToSpan(), nameLookup.m_name);
		nameLookup.m_pipeline = &gp.GetDesc();

		m_graphicsPipelines.Append(nameLookup);

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::ParseRenderPass(const Utf8Char_t *filePath, StringView name, TextParser_t &parser, rpc_interchange::RenderPassEntity &rp)
	{
		parser.ExpectToken(u8"{");

		const data::RenderRTTIStructType *renderPassDescRTTI = m_dataDriver->GetRenderDataHandler()->GetRenderPassDescRTTI();
		const data::RenderRTTIStructType *renderTargetDescRTTI = m_dataDriver->GetRenderDataHandler()->GetRenderTargetDescRTTI();
		const data::RenderRTTIStructType *depthStencilTargetDescRTTI = m_dataDriver->GetRenderDataHandler()->GetDepthStencilTargetDescRTTI();

		for (;;)
		{
			Span<const Utf8Char_t> token;
			parser.RequireToken(token);

			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);

			if (IsToken(token, u8"}"))
				break;

			if (IsToken(token, u8"RenderTargets"))
			{
				parser.ExpectToken(u8"=");
				parser.ExpectToken(u8"{");

				for (;;)
				{
					parser.GetLocation(line, col);

					parser.RequireToken(token);

					if (IsToken(token, u8"}"))
						break;

					CheckValidIdentifier(filePath, token, parser);

					UniquePtr<render::RenderTargetDesc> rtDesc = New<render::RenderTargetDesc>();

					IndexString(token, rtDesc->m_name);

					parser.ExpectToken(u8"=");

					ParseStruct(filePath, renderTargetDescRTTI, rtDesc.Get(), parser);

					rp.GetRenderTargetDescs().Append(rtDesc.Get());

					m_rtDescs.Append(std::move(rtDesc));
				}
			}
			else if (IsToken(token, u8"DepthStencil"))
			{
				parser.ExpectToken(u8"=");

				UniquePtr<render::DepthStencilTargetDesc> depthStencil = New<render::DepthStencilTargetDesc>();

				ParseStruct(filePath, depthStencilTargetDescRTTI, depthStencil.Get(), parser);

				rp.GetDesc().m_depthStencilTarget = depthStencil.Get();

				m_depthStencilTargetDescs.Append(std::move(depthStencil));
			}
			else
			{
				ParseStructMember(filePath, token, renderPassDescRTTI, &rp.GetDesc(), parser);
			}
		}

		rp.GetDesc().m_renderTargets = rp.GetRenderTargetDescs().ToSpan();

		render::RenderPassNameLookup nameLookup;
		IndexString(rp.GetName().ToSpan(), nameLookup.m_name);
		nameLookup.m_renderPass = &rp.GetDesc();

		m_renderPasses.Append(nameLookup);

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::ParseIncludeDirective(const Utf8Char_t *blamePath, TextParser_t &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		ShortTempToken path;

		Span<const Utf8Char_t> token;
		parser.RequireToken(token);

		ResolveQuotedString(path, token);

		IUtilitiesDriver &utils = *GetDrivers().m_utilitiesDriver;

		utils.NormalizeFilePath(path.GetChars());
		if (!utils.ValidateFilePath(path.GetChars(), false))
		{
			rkit::log::ErrorFmt(u8"{} [{}:{}] Invalid file path", blamePath, line, col);
			RKIT_THROW(ResultCode::kMalformedFile);
		}

		BuildFileLocation loc = m_includeStack[m_includeStack.Count() - 1].m_location;

		String str;
		str.Set(path.GetChars());

		CIPath ciPath;
		ciPath.Set(str);

		m_includeStack.Append(AnalyzerIncludeStack(loc, ciPath, true));

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::ResolveQuotedString(ShortTempToken &outToken, const Span<const Utf8Char_t> &inToken)
	{
		outToken.Set(inToken);

		size_t newLength = 0;
		GetDrivers().m_utilitiesDriver->EscapeCStringInPlace(outToken.GetChars(), newLength);

		outToken.Truncate(newLength);

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::ResolveInputLayoutVertexInputs(const Utf8Char_t *blamePath, size_t line, size_t col, render::TempStringIndex_t feedName, const Span<const Utf8Char_t> &nameBase, Vector<const render::InputLayoutVertexInputDesc *> &descsVector, uint32_t &inOutOffset, const render::VectorOrScalarNumericType &inputSourcesType, render::InputLayoutVertexFeedDesc *inputFeed)
	{
		uint32_t sz = ResolveVectorOrScalarNumericTypeSize(inputSourcesType.m_numericType, inputSourcesType.m_cols);

		render::TempStringIndex_t memberName;
		if (nameBase.Count() == 0)
		{
			IndexString(StringView(u8"Value").ToSpan(), memberName);
		}
		else
		{
			IndexString(nameBase, memberName);
		}

		const render::VectorOrScalarNumericType *deduplicated = nullptr;
		Deduplicate(m_vectorOrScalarTypes, inputSourcesType, deduplicated);

		UniquePtr<render::InputLayoutVertexInputDesc> vid = New<render::InputLayoutVertexInputDesc>();
		vid->m_byteOffset = inOutOffset;
		vid->m_inputFeed = inputFeed;
		vid->m_memberName = memberName;
		vid->m_numericType = deduplicated;

		descsVector.Append(vid.Get());

		m_vertexInputDescs.Append(std::move(vid));

		inOutOffset += sz;

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::ResolveInputLayoutVertexInputs(const Utf8Char_t *blamePath, size_t line, size_t col, render::TempStringIndex_t feedName, const Span<const Utf8Char_t> &nameBase, Vector<const render::InputLayoutVertexInputDesc *> &descsVector, uint32_t &inOutOffset, const render::ValueType &inputSourcesType, render::InputLayoutVertexFeedDesc *inputFeed)
	{
		switch (inputSourcesType.m_type)
		{
		case render::ValueTypeType::CompoundNumeric:
			rkit::log::ErrorFmt(u8"{} [{}:{}] Matrix types aren't allowed in vertex inputs", blamePath, line, col);
			RKIT_THROW(ResultCode::kMalformedFile);

		case render::ValueTypeType::VectorNumeric:
			{
				render::VectorOrScalarNumericType converted;
				converted.m_numericType = inputSourcesType.m_value.m_vectorNumericType->m_numericType;
				converted.m_cols = ConvertToMaybeScalar(inputSourcesType.m_value.m_vectorNumericType->m_cols);
				return ResolveInputLayoutVertexInputs(blamePath, line, col, feedName, nameBase, descsVector, inOutOffset, converted, inputFeed);
			}
			break;

		case render::ValueTypeType::Numeric:
			{
				render::VectorOrScalarNumericType converted;
				converted.m_numericType = inputSourcesType.m_value.m_numericType;
				converted.m_cols = render::VectorOrScalarDimension::Scalar;
				return ResolveInputLayoutVertexInputs(blamePath, line, col, feedName, nameBase, descsVector, inOutOffset, converted, inputFeed);
			}
			break;

		case render::ValueTypeType::Structure:
			{
				const render::StructureType *st = inputSourcesType.m_value.m_structureType;

				for (const render::StructureMemberDesc *memberDesc : st->m_members)
				{
					const render::ValueType &memberType = memberDesc->m_type;

					StringView memberName = m_globalStringPool.GetStringByIndex(m_tempStrings[memberDesc->m_name.GetIndex()].GetIndex());

					if (memberType.m_type == render::ValueTypeType::Structure)
					{
						Vector<Utf8Char_t> memberNameBaseChars;
						// FIXME: This line is wrong (appends itself) but I don't remember what it was supposed to be
						memberNameBaseChars.Append(memberNameBaseChars.ToSpan());
						memberNameBaseChars.Append('_');

						ResolveInputLayoutVertexInputs(blamePath, line, col, feedName, memberNameBaseChars.ToSpan(), descsVector, inOutOffset, memberType, inputFeed);

						// Fix this!
						RKIT_THROW(ResultCode::kNotYetImplemented);
					}
					else
					{
						ResolveInputLayoutVertexInputs(blamePath, line, col, feedName, memberName.ToSpan(), descsVector, inOutOffset, memberType, inputFeed);
					}
				}
			}
			break;

		default:
			RKIT_THROW(ResultCode::kMalformedFile);
		}

		RKIT_RETURN_OK;
	}

	uint32_t LibraryAnalyzer::ResolvePackedValueTypeSize(const render::ValueType &valueType)
	{
		switch (valueType.m_type)
		{
		case render::ValueTypeType::Numeric:
			return ResolveNumericTypeSize(valueType.m_value.m_numericType);

		case render::ValueTypeType::CompoundNumeric:
			return ResolveCompoundNumericTypeSize(valueType.m_value.m_compoundNumericType->m_numericType, valueType.m_value.m_compoundNumericType->m_rows, valueType.m_value.m_compoundNumericType->m_cols);

		case render::ValueTypeType::VectorNumeric:
			return ResolveVectorNumericTypeSize(valueType.m_value.m_vectorNumericType->m_numericType, valueType.m_value.m_vectorNumericType->m_cols);

		case render::ValueTypeType::Structure:
			return ResolvePackedStructureTypeSize(*valueType.m_value.m_structureType);

		default:
			RKIT_ASSERT(false);
			return 0;
		}
	}

	uint32_t LibraryAnalyzer::ResolvePackedStructureTypeSize(const render::StructureType &structureType)
	{
		uint32_t sz = 0;
		for (const render::StructureMemberDesc *member : structureType.m_members)
			sz += ResolvePackedValueTypeSize(member->m_type);

		return sz;
	}

	uint32_t LibraryAnalyzer::ResolveCompoundNumericTypeSize(render::NumericType numericType, render::VectorDimension rows, render::VectorDimension cols)
	{
		uint8_t rowsDimensionInt = static_cast<uint8_t>(rows) - static_cast<uint8_t>(render::VectorDimension::Dimension2) + 2;
		uint8_t colsDimensionInt = static_cast<uint8_t>(cols) - static_cast<uint8_t>(render::VectorDimension::Dimension2) + 2;

		return ResolveNumericTypeSize(numericType) * rowsDimensionInt * colsDimensionInt;
	}

	uint32_t LibraryAnalyzer::ResolveVectorOrScalarNumericTypeSize(render::NumericType numericType, render::VectorOrScalarDimension dimension)
	{
		uint8_t dimensionInt = static_cast<uint8_t>(dimension) - static_cast<uint8_t>(render::VectorOrScalarDimension::Scalar) + 1;
		return ResolveNumericTypeSize(numericType) * dimensionInt;
	}

	uint32_t LibraryAnalyzer::ResolveVectorNumericTypeSize(render::NumericType numericType, render::VectorDimension dimension)
	{
		uint8_t dimensionInt = static_cast<uint8_t>(dimension) - static_cast<uint8_t>(render::VectorDimension::Dimension2) + 2;
		return ResolveNumericTypeSize(numericType) * dimensionInt;
	}

	uint32_t LibraryAnalyzer::ResolveNumericTypeSize(render::NumericType numericType)
	{
		switch (numericType)
		{
		case render::NumericType::SInt8:
		case render::NumericType::UInt8:
		case render::NumericType::SNorm8:
		case render::NumericType::UNorm8:
			return 1;

		case render::NumericType::Float16:
		case render::NumericType::SInt16:
		case render::NumericType::UInt16:
		case render::NumericType::SNorm16:
		case render::NumericType::UNorm16:
			return 2;

		case render::NumericType::Float32:
		case render::NumericType::SInt32:
		case render::NumericType::UInt32:
			return 4;

		case render::NumericType::Float64:
		case render::NumericType::SInt64:
		case render::NumericType::UInt64:
			return 8;

		case render::NumericType::Bool:
			return 1;	// FIXME: Need alternate packing rules

		default:
			RKIT_ASSERT(false);
			return 0;
		}
	}

	render::VectorOrScalarDimension LibraryAnalyzer::ConvertToMaybeScalar(render::VectorDimension dimension)
	{
		switch (dimension)
		{
		case render::VectorDimension::Dimension2:
			return render::VectorOrScalarDimension::Dimension2;
		case render::VectorDimension::Dimension3:
			return render::VectorOrScalarDimension::Dimension3;
		case render::VectorDimension::Dimension4:
			return render::VectorOrScalarDimension::Dimension4;
		default:
			RKIT_ASSERT(false);
			return render::VectorOrScalarDimension::Scalar;
		}
	}

	Result LibraryAnalyzer::ExpectIdentifier(const Utf8Char_t *blamePath, Span<const Utf8Char_t> &outToken, TextParser_t &parser)
	{
		parser.RequireToken(outToken);

		return CheckValidIdentifier(blamePath, outToken, parser);
	}

	Result LibraryAnalyzer::CheckValidIdentifier(const Utf8Char_t *blamePath, const Span<const Utf8Char_t> &token, const TextParser_t &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		const Utf8Char_t *tokenChars = token.Ptr();
		size_t tokenLength = token.Count();

		for (size_t i = 0; i < tokenLength; i++)
		{
			char c = tokenChars[i];

			bool isValid = false;
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
				isValid = true;
			else if ((c >= '0' && c <= '9') && i != 0)
				isValid = true;

			if (!isValid)
			{
				rkit::log::ErrorFmt(u8"{} [{}:{}] Expected identifier", blamePath, line, col);
				RKIT_THROW(ResultCode::kMalformedFile);
			}
		}

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::ParseValue(const Utf8Char_t *blamePath, const data::RenderRTTITypeBase *rtti, void *obj, bool isConfigurable, TextParser_t &parser)
	{
		switch (rtti->m_type)
		{
		case data::RenderRTTIType::Enum:
			return ParseEnum(blamePath, reinterpret_cast<const data::RenderRTTIEnumType *>(rtti), obj, isConfigurable, parser);
		case data::RenderRTTIType::Number:
			return ParseNumber(blamePath, reinterpret_cast<const data::RenderRTTINumberType *>(rtti), obj, isConfigurable, parser);
		case data::RenderRTTIType::Structure:
			RKIT_ASSERT(!isConfigurable);
			return ParseStruct(blamePath, reinterpret_cast<const data::RenderRTTIStructType *>(rtti), obj, parser);
		case data::RenderRTTIType::ValueType:
			{
				RKIT_ASSERT(!isConfigurable);

				Span<const Utf8Char_t> token;
				ExpectIdentifier(blamePath, token, parser);

				return ParseValueType(blamePath, *static_cast<render::ValueType *>(obj), token, parser);
			}
			break;
		case data::RenderRTTIType::StringIndex:
			RKIT_ASSERT(!isConfigurable);
			return ParseStringIndex(blamePath, reinterpret_cast<const data::RenderRTTIStringIndexType *>(rtti), obj, parser);
		default:
			RKIT_THROW(ResultCode::kInternalError);
		}
	}

	Result LibraryAnalyzer::ParseValueType(const Utf8Char_t *blamePath, render::ValueType &valueType, const Span<const Utf8Char_t> &token, TextParser_t &parser)
	{
		for (const SimpleNumericTypeResolution &ntr : m_numericTypeResolutions)
		{
			const size_t ntrLength = ntr.m_name.Length();

			if (token.Count() < ntrLength)
				continue;

			const size_t extraChars = token.Count() - ntrLength;

			if (extraChars == 0 || extraChars == 1 || extraChars == 3)
			{
				if (memcmp(token.Ptr(), ntr.m_name.GetChars(), ntrLength))
					continue;
			}

			if (extraChars == 0)
			{
				valueType = render::ValueType(ntr.m_numericType);
				RKIT_RETURN_OK;
			}
			else if (extraChars == 1)
			{
				char c0 = token[ntrLength];
				if (c0 >= '2' && c0 <= '4')
				{
					render::VectorNumericType vectorType;
					vectorType.m_cols = static_cast<render::VectorDimension>(static_cast<int>(render::VectorDimension::Dimension2) + (c0 - '2'));
					vectorType.m_numericType = ntr.m_numericType;

					const render::VectorNumericType *deduplicated = nullptr;
					Deduplicate(m_vectorTypes, vectorType, deduplicated);

					valueType = render::ValueType(deduplicated);
					return;
				}
			}
			else if (extraChars == 3)
			{
				char c0 = token[ntrLength];
				char c1 = token[ntrLength + 1];
				char c2 = token[ntrLength + 2];
				if ((c0 >= '2' && c0 <= '4') && c1 == 'x' && (c2 >= '2' && c2 <= '4'))
				{
					render::CompoundNumericType compoundType;
					compoundType.m_rows = static_cast<render::VectorDimension>(static_cast<int>(render::VectorDimension::Dimension2) + (c0 - '2'));
					compoundType.m_cols = static_cast<render::VectorDimension>(static_cast<int>(render::VectorDimension::Dimension2) + (c2 - '2'));
					compoundType.m_numericType = ntr.m_numericType;

					const render::CompoundNumericType *deduplicated = nullptr;
					Deduplicate(m_compoundTypes, compoundType, deduplicated);

					valueType = render::ValueType(deduplicated);
					return;
				}
			}
		}

		HashMap<String, UniquePtr<rpc_interchange::Entity>>::ConstIterator_t it = m_entities.Find(StringSliceView(token));
		if (it == m_entities.end())
		{
			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);
			rkit::log::ErrorFmt(u8"{} [{}:{}] Unknown type identifier", blamePath, line, col);
			RKIT_THROW(ResultCode::kMalformedFile);
		}

		const rpc_interchange::Entity *entity = it.Value().Get();

		if (entity->GetEntityType() != rpc_interchange::EntityType::StructDef)
		{
			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);
			rkit::log::ErrorFmt(u8"{} [{}:{}] Identifier does not resolve to a structure", blamePath, line, col);
			RKIT_THROW(ResultCode::kMalformedFile);
		}

		valueType = render::ValueType(&static_cast<const rpc_interchange::StructDefEntity *>(entity)->GetDesc());

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::ParseConfigurable(const Utf8Char_t *blamePath, void *obj, data::RenderRTTIMainType mainType, void (*writeNameFunc)(void *, const render::ConfigStringIndex_t &), TextParser_t &parser)
	{
		Span<const Utf8Char_t> configName;

		parser.ExpectToken(u8"(");

		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		ExpectIdentifier(blamePath, configName, parser);
		parser.ExpectToken(u8")");

		render::GlobalStringIndex_t configNameSI;
		IndexString(configName, configNameSI);

		HashMap<render::GlobalStringIndex_t, render::ConfigStringIndex_t>::ConstIterator_t keyIt = m_configNameToKey.Find(configNameSI);
		if (keyIt != m_configNameToKey.end())
		{
			const ConfigKey &configKey = m_configKeys[keyIt.Value().GetIndex()];

			if (configKey.m_mainType != mainType)
			{
				rkit::log::ErrorFmt(u8"{} [{}:{}] Config key was already used for a different type", blamePath, line, col);
				RKIT_THROW(ResultCode::kMalformedFile);
			}

			writeNameFunc(obj, keyIt.Value());
			RKIT_RETURN_OK;
		}

		ConfigKey configKey;
		configKey.m_mainType = mainType;
		configKey.m_name = configNameSI;

		render::ConfigStringIndex_t configSI(m_configKeys.Count());

		m_configNameToKey.Set(configNameSI, configSI);
		m_configKeys.Append(std::move(configKey));

		writeNameFunc(obj, configSI);

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::ParseUIntConstant(const Utf8Char_t *blamePath, size_t blameLine, size_t blameCol, const Span<const Utf8Char_t> &token, uint64_t max, uint64_t &outValue)
	{
		uint64_t result = 0;

		if (token.Count() == 0)
		{
			outValue = 0;
			RKIT_RETURN_OK;
		}

		uint8_t base = 16;

		uint8_t firstDigit = 0;
		if (token[0] == '0')
		{
			if (token.Count() >= 2 && token[1] == 'x')
			{
				firstDigit = 2;
				base = 16;
			}
			else
				base = 8;
		}
		else
			base = 10;

		uint64_t maxBeforeMultiply = max / base;

		for (size_t i = firstDigit; i < token.Count(); i++)
		{
			char c = token[i];
			uint8_t digit = 0;
			if (c >= '0' && c <= '9')
				digit = static_cast<uint8_t>(c - '0');
			else if (c >= 'a' && c <= 'f')
				digit = (c - 'a') + 0xa;
			else if (c >= 'A' && c <= 'F')
				digit = (c - 'A') + 0xA;
			else
			{
				rkit::log::ErrorFmt(u8"{} [{}:{}] Expected numeric constant", blamePath, blameLine, blameCol);
				RKIT_THROW(ResultCode::kMalformedFile);
			}

			if (result >= maxBeforeMultiply)
			{
				rkit::log::ErrorFmt(u8"{} [{}:{}] Integer constant overflow", blamePath, blameLine, blameCol);
				RKIT_THROW(ResultCode::kMalformedFile);
			}

			if (digit >= base)
			{
				rkit::log::ErrorFmt(u8"{} [{}:{}] Invalid digit in numeric constant", blamePath, blameLine, blameCol);
				RKIT_THROW(ResultCode::kMalformedFile);
			}

			result *= base;

			if (max - result < digit)
			{
				rkit::log::ErrorFmt(u8"{} [{}:{}] Integer constant overflow", blamePath, blameLine, blameCol);
				RKIT_THROW(ResultCode::kMalformedFile);
			}

			result += digit;
		}

		outValue = result;

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::ParseStringIndex(const Utf8Char_t *blamePath, const data::RenderRTTIStringIndexType *rtti, void *obj, TextParser_t &parser)
	{
		size_t line = 0;
		size_t col = 0;

		parser.GetLocation(line, col);

		Span<const Utf8Char_t> token;
		parser.RequireToken(token);

		if (token.Count() == 0 || token[0] != '\"')
		{
			rkit::log::ErrorFmt(u8"{} [{}:{}] Expected string constant", blamePath, line, col);
			RKIT_THROW(ResultCode::kMalformedFile);
		}

		IUtilitiesDriver &utils = *GetDrivers().m_utilitiesDriver;

		Vector<Utf8Char_t> escapedStr;
		escapedStr.Append(token);

		size_t newLength = 0;
		utils.EscapeCStringInPlace(escapedStr.ToSpan(), newLength);

		escapedStr.Resize(newLength);

		render::GlobalStringIndex_t globalIndex;
		IndexString(escapedStr.ToSpan(), globalIndex);

		int purpose = rtti->m_getPurposeFunc();

		if (purpose == render::GlobalStringIndex_t::kPurpose)
			rtti->m_writeStringIndexFunc(obj, globalIndex.GetIndex());
		else if (purpose == render::TempStringIndex_t::kPurpose)
		{
			render::TempStringIndex_t tempIndex;
			IndexString(globalIndex, tempIndex);

			rtti->m_writeStringIndexFunc(obj, tempIndex.GetIndex());
		}
		else
			RKIT_THROW(ResultCode::kInternalError);

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::ParseEnum(const Utf8Char_t *blamePath, const data::RenderRTTIEnumType *rtti, void *obj, bool isConfigurable, TextParser_t &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		Span<const Utf8Char_t> option;
		parser.RequireToken(option);

		if (IsToken(option, u8"Config"))
		{
			if (!isConfigurable)
			{
				rkit::log::ErrorFmt(u8"{} [{}:{}] Option is not configurable", blamePath, line, col);
				RKIT_THROW(ResultCode::kMalformedFile);
			}

			return ParseConfigurable(blamePath, obj, rtti->m_base.m_mainType, rtti->m_writeConfigurableNameFunc, parser);
		}

		for (size_t i = 0; i < rtti->m_numOptions; i++)
		{
			const data::RenderRTTIEnumOption *candidate = rtti->m_options + i;
			if (option.Count() == candidate->m_nameLength && !memcmp(option.Ptr(), candidate->m_name, option.Count()))
			{
				if (isConfigurable)
					rtti->m_writeConfigurableValueFunc(obj, candidate->m_value);
				else
					rtti->m_writeValueFunc(obj, candidate->m_value);

				RKIT_RETURN_OK;
			}
		}

		rkit::log::ErrorFmt(u8"{} [{}:{}] Invalid value", blamePath, line, col);
		RKIT_THROW(ResultCode::kMalformedFile);
	}

	Result LibraryAnalyzer::ParseNumber(const Utf8Char_t *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, TextParser_t &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		Span<const Utf8Char_t> numberToken;
		parser.RequireToken(numberToken);

		switch (rtti->m_representation)
		{
		case data::RenderRTTINumberRepresentation::UnsignedInt:
			return ParseUInt(blamePath, rtti, obj, isConfigurable, numberToken, line, col);
		case data::RenderRTTINumberRepresentation::SignedInt:
			return ParseSInt(blamePath, rtti, obj, isConfigurable, numberToken, line, col);
		case data::RenderRTTINumberRepresentation::Float:
			return ParseFloat(blamePath, rtti, obj, isConfigurable, numberToken, line, col);
		default:
			RKIT_THROW(ResultCode::kInternalError);
		}
	}

	Result LibraryAnalyzer::ParseBool(const Utf8Char_t *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const Utf8Char_t> &token, size_t line, size_t col)
	{
		uint8_t number = 0;
		if (IsToken(token, u8"true"))
			number = 0;
		else if (IsToken(token, u8"false"))
			number = 1;
		else
		{
			rkit::log::ErrorFmt(u8"{} [{}:{}] Invalid boolean value", blamePath, line, col);
			RKIT_THROW(ResultCode::kMalformedFile);
		}

		if (isConfigurable)
			rtti->m_configurableFunctions.m_writeValueUIntFunc(obj, number);
		else
			rtti->m_valueFunctions.m_writeValueUIntFunc(obj, number);

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::ParseUInt(const Utf8Char_t *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const Utf8Char_t> &token, size_t line, size_t col)
	{
		if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize1)
			return ParseBool(blamePath, rtti, obj, isConfigurable, token, line, col);

		RKIT_THROW(ResultCode::kNotYetImplemented);
	}

	Result LibraryAnalyzer::ParseSInt(const Utf8Char_t *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const Utf8Char_t> &token, size_t line, size_t col)
	{
		RKIT_THROW(ResultCode::kNotYetImplemented);
	}

	Result LibraryAnalyzer::ParseFloat(const Utf8Char_t *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const Utf8Char_t> &token, size_t line, size_t col)
	{
		RKIT_THROW(ResultCode::kNotYetImplemented);
	}

	Result LibraryAnalyzer::ParseStruct(const Utf8Char_t *blamePath, const data::RenderRTTIStructType *rtti, void *obj, TextParser_t &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		parser.ExpectToken(u8"{");

		for (;;)
		{
			Span<const Utf8Char_t> nameToken;

			parser.RequireToken(nameToken);

			if (IsToken(nameToken, u8"}"))
				break;

			CheckValidIdentifier(blamePath, nameToken, parser);

			ParseStructMember(blamePath, nameToken, rtti, obj, parser);
		}

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::ParseStructMember(const Utf8Char_t *blamePath, const Span<const Utf8Char_t> &memberName, const data::RenderRTTIStructType *rtti, void *obj, TextParser_t &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		const data::RenderRTTIStructField *resolvedField = nullptr;

		for (size_t fi = 0; fi < rtti->m_numFields; fi++)
		{
			const data::RenderRTTIStructField *field = rtti->m_fields + fi;

			if (!field->m_isVisible)
				continue;

			const Utf8Char_t *fieldName = field->m_name;
			size_t fieldNameLength = field->m_nameLength;

			if (memberName.Count() != fieldNameLength)
				continue;

			char firstCharUpper = fieldName[0];
			if (firstCharUpper >= 'a' && firstCharUpper <= 'z')
				firstCharUpper = static_cast<char>(static_cast<int>(firstCharUpper) + 'A' - 'a');

			if (memberName[0] != firstCharUpper)
				continue;

			bool matched = true;
			for (size_t ci = 1; ci < fieldNameLength; ci++)
			{
				if (fieldName[ci] != memberName[ci])
				{
					matched = false;
					break;
				}
			}

			if (!matched)
				continue;

			resolvedField = field;
		}

		if (!resolvedField)
		{
			rkit::log::ErrorFmt(u8"{} [{}:{}] Invalid field", blamePath, line, col);
			RKIT_THROW(ResultCode::kMalformedFile);
		}

		const data::RenderRTTITypeBase *fieldType = resolvedField->m_getTypeFunc();

		void *fieldPtr = resolvedField->m_getMemberPtrFunc(obj);

		parser.ExpectToken(u8"=");

		ParseValue(blamePath, resolvedField->m_getTypeFunc(), fieldPtr, resolvedField->m_isConfigurable, parser);
	}

	template<class T>
	Result LibraryAnalyzer::ParseDynamicStructMember(const Utf8Char_t *blamePath, const Span<const Utf8Char_t> &memberName, const data::RenderRTTIStructType *rtti, T &obj, TextParser_t &parser)
	{
		return ParseStructMember(blamePath, memberName, rtti, &obj, parser);
	}

	template<size_t TSize>
	bool LibraryAnalyzer::IsToken(const Span<const Utf8Char_t> &span, const Utf8Char_t(&tokenChars)[TSize])
	{
		return IsTokenChars(span, tokenChars, TSize - 1);
	}

	bool LibraryAnalyzer::IsTokenChars(const Span<const Utf8Char_t> &span, const Utf8Char_t *tokenStr, size_t tokenLen)
	{
		if (span.Count() != tokenLen)
			return false;

		return !memcmp(span.Ptr(), tokenStr, tokenLen);
	}

	template<class T>
	Result LibraryAnalyzer::Deduplicate(Vector<UniquePtr<T>> &instVector, const T &instance, const T *&outDeduplicated)
	{
		for (const UniquePtr<T> &candidate : instVector)
		{
			if (instance == *candidate)
			{
				outDeduplicated = candidate.Get();
			}
		}

		UniquePtr<T> newInst = New<T>(instance);

		outDeduplicated = newInst.Get();

		instVector.Append(std::move(newInst));

		RKIT_RETURN_OK;
	}

	bool LibraryAnalyzer::IsBindingTyped(render::DescriptorBindingType descType)
	{
		switch (descType)
		{
		case render::DescriptorBindingType::Sampler:
		case render::DescriptorBindingType::ByteAddressBuffer:
		case render::DescriptorBindingType::RWByteAddressBuffer:
			return false;

		case render::DescriptorBindingType::Buffer:
		case render::DescriptorBindingType::RWBuffer:
		case render::DescriptorBindingType::Texture1D:
		case render::DescriptorBindingType::Texture1DArray:
		case render::DescriptorBindingType::Texture2D:
		case render::DescriptorBindingType::Texture2DArray:
		case render::DescriptorBindingType::Texture2DMS:
		case render::DescriptorBindingType::Texture2DMSArray:
		case render::DescriptorBindingType::Texture3D:
		case render::DescriptorBindingType::TextureCube:
		case render::DescriptorBindingType::TextureCubeArray:
		case render::DescriptorBindingType::RWTexture1D:
		case render::DescriptorBindingType::RWTexture1DArray:
		case render::DescriptorBindingType::RWTexture2D:
		case render::DescriptorBindingType::RWTexture2DArray:
		case render::DescriptorBindingType::RWTexture3D:
			return true;

		default:
			RKIT_ASSERT(false);
			return false;
		}
	}

	void LibraryAnalyzer::RemoveTopStackItem()
	{
		m_includeStack.RemoveRange(m_includeStack.Count() - 1, 1);
	}

	Result LibraryAnalyzer::ExportPackages(IDependencyNode *depsNode) const
	{
		rkit::buildsystem::IBuildSystemDriver *bsDriver = static_cast<rkit::buildsystem::IBuildSystemDriver *>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, u8"BuildSystem"));

		data::IRenderDataHandler *dataHandler = m_dataDriver->GetRenderDataHandler();

		size_t pipelineIndex = 0;
		for (const render::GraphicsPipelineNameLookup &graphicsPipeline : m_graphicsPipelines)
		{
			UniquePtr<IPackageObjectWriter> writer;
			bsDriver->CreatePackageObjectWriter(writer);

			UniquePtr<IPackageBuilder> pkgBuilder;
			bsDriver->CreatePackageBuilder(dataHandler, writer.Get(), true, pkgBuilder);

			const data::RenderRTTIStructType *pipelineType = dataHandler->GetGraphicsPipelineNameLookupRTTI();

			RKIT_ASSERT(pipelineType->m_indexableType == data::RenderRTTIIndexableStructType::GraphicsPipelineNameLookup);

			pkgBuilder->BeginSource(this);

			size_t index = 0;
			pkgBuilder->IndexObject(&graphicsPipeline, pipelineType, true, index);

			CIPath outPath;
			FormatGraphicPipelinePath(outPath, depsNode->GetIdentifier(), pipelineIndex);

			{
				UniquePtr<ISeekableReadWriteStream> stream;
				m_feedback->OpenOutput(BuildFileLocation::kIntermediateDir, outPath, stream);

				pkgBuilder->WritePackage(*stream);
			}

			m_feedback->AddNodeDependency(IModuleDriver::kDefaultNamespace, kRenderGraphicsPipelineNodeID, BuildFileLocation::kIntermediateDir, outPath.ToString());

			pipelineIndex++;
		}

		// Write globals
		{
			UniquePtr<IPackageObjectWriter> writer;
			bsDriver->CreatePackageObjectWriter(writer);

			UniquePtr<IPackageBuilder> pkgBuilder;
			bsDriver->CreatePackageBuilder(dataHandler, writer.Get(), false, pkgBuilder);

			const data::RenderRTTIStructType *renderPassType = dataHandler->GetRenderPassNameLookupRTTI();

			RKIT_ASSERT(renderPassType->m_indexableType == data::RenderRTTIIndexableStructType::RenderPassNameLookup);

			pkgBuilder->BeginSource(this);

			for (const render::RenderPassNameLookup &renderPass : m_renderPasses)
			{
				size_t index = 0;
				pkgBuilder->IndexObject(&renderPass, renderPassType, true, index);
			}

			CIPath outPath;
			FormatGlobalsPath(outPath, depsNode->GetIdentifier());

			{
				UniquePtr<ISeekableReadWriteStream> stream;
				m_feedback->OpenOutput(BuildFileLocation::kIntermediateDir, outPath, stream);

				pkgBuilder->WritePackage(*stream);
			}
		}


		CIPath indexPath;
		FormatIndexPath(indexPath, depsNode->GetIdentifier());

		UniquePtr<ISeekableReadWriteStream> indexStream;
		m_feedback->OpenOutput(BuildFileLocation::kIntermediateDir, indexPath, indexStream);

		indexStream->WriteAll(&rpc_common::kLibraryIndexID, sizeof(rpc_common::kLibraryIndexID));
		indexStream->WriteAll(&rpc_common::kLibraryIndexVersion, sizeof(rpc_common::kLibraryIndexVersion));

		uint64_t numGraphicsPipelines = m_graphicsPipelines.Count();
		indexStream->WriteAll(&numGraphicsPipelines, sizeof(numGraphicsPipelines));

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::IndexString(const Span<const Utf8Char_t> &span, render::GlobalStringIndex_t &outStringIndex)
	{
		String str;
		str.Set(span);

		size_t index = 0;
		m_globalStringPool.IndexString(str, index);

		outStringIndex = render::GlobalStringIndex_t(index);

		RKIT_RETURN_OK;
	}

	Result LibraryAnalyzer::IndexString(const Span<const Utf8Char_t> &span, render::TempStringIndex_t &outStringIndex)
	{
		render::GlobalStringIndex_t gsi;

		IndexString(span, gsi);

		return IndexString(gsi, outStringIndex);
	}

	Result LibraryAnalyzer::IndexString(const render::GlobalStringIndex_t &globalIndex, render::TempStringIndex_t &outStringIndex)
	{
		HashMap<render::GlobalStringIndex_t, render::TempStringIndex_t>::ConstIterator_t it = m_globalStringToTempString.Find(globalIndex);

		if (it != m_globalStringToTempString.end())
		{
			outStringIndex = it.Value();
			RKIT_RETURN_OK;
		}

		render::TempStringIndex_t tsi(m_tempStrings.Count());
		m_tempStrings.Append(globalIndex);
		m_globalStringToTempString.Set(globalIndex, tsi);

		outStringIndex = tsi;

		RKIT_RETURN_OK;
	}
} // rkit::buildsystem::rpc_analyzer

namespace rkit::buildsystem::rpc_combiner
{
	LibraryCombiner::LibraryCombiner(data::IDataDriver *dataDriver)
		: m_dataDriver(dataDriver)
	{
	}

	Result LibraryCombiner::AddInput(IReadStream &stream)
	{
		CheckInitObjects();

		UniquePtr<data::IRenderDataPackage> pkg;
		Vector<Vector<uint8_t>> binaryContent;

		m_dataDriver->GetRenderDataHandler()->LoadPackage(stream, false, nullptr, pkg, &binaryContent);

		PackageInputResolver resolver(*pkg, binaryContent.ToSpan());
		m_pkgBuilder->BeginSource(&resolver);

		{
			data::IRenderRTTIListBase *graphicPipelineLookups = pkg->GetIndexable(data::RenderRTTIIndexableStructType::GraphicsPipelineNameLookup);

			size_t numGraphicPipelineLookups = graphicPipelineLookups->GetCount();

			for (size_t i = 0; i < numGraphicPipelineLookups; i++)
			{
				const render::GraphicsPipelineNameLookup *nameLookup = static_cast<const render::GraphicsPipelineNameLookup *>(graphicPipelineLookups->GetElementPtr(i));

				StringView nameStrView = pkg->GetString(nameLookup->m_name.GetIndex());

				if (m_graphicPipelineNames.Contains(nameStrView))
				{
					rkit::log::ErrorFmt(u8"Duplicate graphic pipeline name '{}'", nameStrView.GetChars());
					RKIT_THROW(ResultCode::kOperationFailed);
				}
				else
				{
					String nameStr;
					nameStr.Set(nameStrView);

					m_graphicPipelineNames.Add(std::move(nameStr));
				}

				size_t index = 0;
				m_pkgBuilder->IndexObject(nameLookup, m_dataDriver->GetRenderDataHandler()->GetGraphicsPipelineNameLookupRTTI(), true, index);
			}
		}

		{
			data::IRenderRTTIListBase *renderPassLookups = pkg->GetIndexable(data::RenderRTTIIndexableStructType::RenderPassNameLookup);

			size_t numRenderPassLookups = renderPassLookups->GetCount();

			for (size_t i = 0; i < numRenderPassLookups; i++)
			{
				const render::RenderPassNameLookup *nameLookup = static_cast<const render::RenderPassNameLookup *>(renderPassLookups->GetElementPtr(i));

				size_t index = 0;
				m_pkgBuilder->IndexObject(nameLookup, m_dataDriver->GetRenderDataHandler()->GetRenderPassNameLookupRTTI(), true, index);
			}
		}

		RKIT_RETURN_OK;
	}

	Result LibraryCombiner::WritePackage(ISeekableWriteStream &stream)
	{
		CheckInitObjects();

		return m_pkgBuilder->WritePackage(stream);
	}

	Result LibraryCombiner::CheckInitObjects()
	{
		if (!m_pkgObjectWriter.IsValid())
		{
			rkit::buildsystem::IBuildSystemDriver *bsDriver = static_cast<rkit::buildsystem::IBuildSystemDriver *>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, u8"BuildSystem"));
			bsDriver->CreatePackageObjectWriter(m_pkgObjectWriter);
			bsDriver->CreatePackageBuilder(m_dataDriver->GetRenderDataHandler(), m_pkgObjectWriter.Get(), false, m_pkgBuilder);
		}

		RKIT_RETURN_OK;
	}

	LibraryCombiner::PackageInputResolver::PackageInputResolver(data::IRenderDataPackage &pkg, const Span<const Vector<uint8_t>> &binaryContent)
		: m_pkg(pkg)
		, m_binaryContent(binaryContent)
	{
	}

	StringSliceView LibraryCombiner::PackageInputResolver::ResolveGlobalString(size_t index) const
	{
		return m_pkg.GetString(index);
	}

	StringSliceView LibraryCombiner::PackageInputResolver::ResolveConfigKey(size_t index) const
	{
		return m_pkg.GetString(m_pkg.GetConfigKey(index).m_stringIndex);
	}

	StringSliceView LibraryCombiner::PackageInputResolver::ResolveTempString(size_t index) const
	{
		RKIT_ASSERT(false);
		return StringSliceView();
	}

	Span<const uint8_t> LibraryCombiner::PackageInputResolver::ResolveBinaryContent(size_t index) const
	{
		return m_binaryContent[index].ToSpan();
	}
} // rkit::buildsystem::rpc_combiner

namespace rkit::buildsystem::rpc_compiler
{
	LibraryCompiler::LibraryCompiler(data::IDataDriver *dataDriver, IDependencyNodeCompilerFeedback *feedback)
		: m_combiner(dataDriver)
		, m_feedback(feedback)
	{
	}

	Result LibraryCompiler::Run(IDependencyNode *depsNode)
	{
		CIPath indexPath;
		FormatIndexPath(indexPath, depsNode->GetIdentifier());

		size_t numGraphicsPipelines = 0;

		{
			UniquePtr<ISeekableReadStream> indexFile;
			m_feedback->OpenInput(BuildFileLocation::kIntermediateDir, indexPath, indexFile);

			uint32_t header[2];
			indexFile->ReadAll(header, sizeof(header));

			if (header[0] != rpc_common::kLibraryIndexID || header[1] != rpc_common::kLibraryIndexVersion)
			{
				rkit::log::Error(u8"Invalid library index header");
				RKIT_RETURN_OK;
			}

			uint64_t pipelineCounts[1];
			indexFile->ReadAll(pipelineCounts, sizeof(pipelineCounts));

			for (uint64_t pipelineCount : pipelineCounts)
			{
				if (pipelineCount > std::numeric_limits<size_t>::max())
					RKIT_THROW(ResultCode::kOutOfMemory);
			}

			numGraphicsPipelines = static_cast<size_t>(pipelineCounts[0]);
		}

		// Add pipelines
		for (size_t i = 0; i < numGraphicsPipelines; i++)
		{
			CIPath pipelinePath;
			FormatGraphicPipelinePath(pipelinePath, depsNode->GetIdentifier(), i);

			CIPath compiledPipelinePath;
			compiledPipelinePath.Set(GetCompiledPipelineIntermediateBasePath());
			compiledPipelinePath.Append(pipelinePath);

			UniquePtr<ISeekableReadStream> inStream;
			m_feedback->OpenInput(BuildFileLocation::kIntermediateDir, compiledPipelinePath, inStream);

			m_combiner.AddInput(*inStream);
		}

		// Add globals
		{
			CIPath globalsPath;
			FormatGlobalsPath(globalsPath, depsNode->GetIdentifier());

			UniquePtr<ISeekableReadStream> inStream;
			m_feedback->OpenInput(BuildFileLocation::kIntermediateDir, globalsPath, inStream);

			m_combiner.AddInput(*inStream);
		}

		CIPath outPath;
		FormatCombinedOutputPath(outPath, depsNode->GetIdentifier());

		UniquePtr<ISeekableReadWriteStream> outStream;
		m_feedback->OpenOutput(BuildFileLocation::kIntermediateDir, outPath, outStream);

		m_combiner.WritePackage(*outStream);

		RKIT_RETURN_OK;
	}
} // rkit::buildsystem::rpc_compiler

namespace rkit::buildsystem
{
	RenderPipelineLibraryCompiler::RenderPipelineLibraryCompiler()
	{
	}

	bool RenderPipelineLibraryCompiler::HasAnalysisStage() const
	{
		return true;
	}

	Result RenderPipelineLibraryCompiler::RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::IModule *dataModule = rkit::GetDrivers().m_moduleDriver->LoadModule(IModuleDriver::kDefaultNamespace, u8"Data");
		if (!dataModule)
		{
			rkit::log::Error(u8"Couldn't load data module");
			RKIT_THROW(ResultCode::kModuleLoadFailed);
		}

		data::IDataDriver *dataDriver = static_cast<data::IDataDriver *>(rkit::GetDrivers().FindDriver(IModuleDriver::kDefaultNamespace, u8"Data"));

		UniquePtr<rpc_analyzer::LibraryAnalyzer> analyzer = New<rpc_analyzer::LibraryAnalyzer>(dataDriver, feedback);

		analyzer->Run(depsNode);

		feedback->CheckFault();

		RKIT_RETURN_OK;
	}

	Result RenderPipelineLibraryCompiler::RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::IModule *dataModule = rkit::GetDrivers().m_moduleDriver->LoadModule(IModuleDriver::kDefaultNamespace, u8"Data");
		if (!dataModule)
		{
			rkit::log::Error(u8"Couldn't load data module");
			RKIT_THROW(ResultCode::kModuleLoadFailed);
		}

		data::IDataDriver *dataDriver = static_cast<data::IDataDriver *>(rkit::GetDrivers().FindDriver(IModuleDriver::kDefaultNamespace, u8"Data"));


		UniquePtr<rpc_compiler::LibraryCompiler> compiler = New<rpc_compiler::LibraryCompiler>(dataDriver, feedback);

		compiler->Run(depsNode);

		feedback->CheckFault();

		RKIT_RETURN_OK;
	}

	uint32_t RenderPipelineLibraryCompiler::GetVersion() const
	{
		return 1;
	}

	Result PipelineLibraryCombinerBase::Create(UniquePtr<IPipelineLibraryCombiner> &outCombiner)
	{
		rkit::IModule *dataModule = rkit::GetDrivers().m_moduleDriver->LoadModule(IModuleDriver::kDefaultNamespace, u8"Data");
		if (!dataModule)
		{
			rkit::log::Error(u8"Couldn't load data module");
			RKIT_THROW(ResultCode::kModuleLoadFailed);
		}

		data::IDataDriver *dataDriver = static_cast<data::IDataDriver *>(rkit::GetDrivers().FindDriver(IModuleDriver::kDefaultNamespace, u8"Data"));

		outCombiner = New<rpc_combiner::LibraryCombiner>(dataDriver);
	}
} // rkit::buildsystem
