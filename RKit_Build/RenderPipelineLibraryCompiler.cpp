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

namespace rkit { namespace buildsystem { namespace rpc_interchange
{
	enum class EntityType
	{
		StaticSampler,
		PushConstants,
		StructDef,
		InputLayout,
		GraphicsPipeline,
		DescriptorLayout,
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

	class StaticSamplerEntity final : public Entity
	{
	public:
		explicit StaticSamplerEntity() {}

		render::SamplerDesc &GetDesc() { return m_desc; }
		const render::SamplerDesc &GetDesc() const { return m_desc; }
		EntityType GetEntityType() const override { return EntityType::StaticSampler; }

	private:
		render::SamplerDesc m_desc;
	};

	class PushConstantsEntity final : public Entity
	{
	public:
		explicit PushConstantsEntity() {}

		render::PushConstantListDesc &GetDesc() { return m_desc; }

		Vector<const render::PushConstantDesc *> &GetPushConstantsVector() { return m_pushConstants; }

		EntityType GetEntityType() const override { return EntityType::PushConstants; }

	private:
		render::PushConstantListDesc m_desc;
		Vector<const render::PushConstantDesc *> m_pushConstants;
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

	class DescriptorLayoutEntity final : public Entity
	{
	public:
		explicit DescriptorLayoutEntity() {}

		render::DescriptorLayoutDesc &GetDesc() { return m_desc; }
		Vector<const render::DescriptorDesc *> &GetDescriptorDescs() { return m_descriptorDescs; }

		EntityType GetEntityType() const override { return EntityType::DescriptorLayout; }

	private:
		render::DescriptorLayoutDesc m_desc;
		Vector<const render::DescriptorDesc *> m_descriptorDescs;
	};

	class GraphicsPipelineEntity final : public Entity
	{
	public:
		explicit GraphicsPipelineEntity() {}

		render::GraphicsPipelineDesc &GetDesc() { return m_desc; }
		render::PipelineLayoutDesc &GetPipelineLayout() { return m_pipelineLayout; }

		Vector<const render::DescriptorLayoutDesc *> &GetDescriptorLayouts() { return m_descriptorLayouts; }
		Vector<const render::RenderOperationDesc *> &GetRenderOperations() { return m_renderOperations; }

		EntityType GetEntityType() const override { return EntityType::GraphicsPipeline; }

	private:
		render::GraphicsPipelineDesc m_desc;
		render::PipelineLayoutDesc m_pipelineLayout;
		Vector<const render::DescriptorLayoutDesc *> m_descriptorLayouts;
		Vector<const render::RenderTargetDesc *> m_renderTargets;
		Vector<const render::RenderOperationDesc *> m_renderOperations;
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
} } } // rkit::buildsystem::rpc_interchange

namespace rkit { namespace buildsystem { namespace rpc_common
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
} } } // rkit::buildsystem::rpc_common

namespace rkit { namespace buildsystem { namespace rpc_analyzer
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
		Result Set(const Span<const char> &span);
		Span<char> GetChars();
		void Truncate(size_t truncatedSize);

	private:
		static const size_t kStaticBufferSize = 32;

		Vector<char> m_chars;
		char m_staticBuffer[kStaticBufferSize] = {};
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

		UniquePtr<rkit::utils::ITextParser> m_textParser;
		Vector<uint8_t> m_fileContents;
	};

	class LibraryAnalyzer final : public IStringResolver, public rpc_common::LibraryCompilerBase
	{
	public:
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

		enum class DescriptorTypeClassification
		{
			Unknown,

			Sampler,
			ConstantBuffer,
			Buffer,
			ByteAddressBuffer,
			Texture,
			RWTexture,
		};

		Result ScanTopStackItem(AnalyzerIncludeStack &item);
		Result ParseTopStackItem(AnalyzerIncludeStack &item);
		Result ParseDirective(const char *filePath, utils::ITextParser &parser, bool &outHaveDirective);

		Result ParseIncludeDirective(const char *filePath, utils::ITextParser &parser);

		template<class T>
		Result ParseEntity(const char *blamePath, utils::ITextParser &parser, Result(LibraryAnalyzer:: *parseFunc)(const char *, utils::ITextParser &, T &));

		Result ParseStaticSampler(const char *filePath, utils::ITextParser &parser, rpc_interchange::StaticSamplerEntity &ss);
		Result ParsePushConstants(const char *filePath, utils::ITextParser &parser, rpc_interchange::PushConstantsEntity &pc);
		Result ParseStructDef(const char *filePath, utils::ITextParser &parser, rpc_interchange::StructDefEntity &pc);
		Result ParseInputLayout(const char *filePath, utils::ITextParser &parser, rpc_interchange::InputLayoutEntity &il);
		Result ParseDescriptorLayout(const char *filePath, utils::ITextParser &parser, rpc_interchange::DescriptorLayoutEntity &dl);
		Result ParseGraphicsPipeline(const char *filePath, utils::ITextParser &parser, rpc_interchange::GraphicsPipelineEntity &gp);
		Result ParseRenderPass(const char *filePath, utils::ITextParser &parser, rpc_interchange::RenderPassEntity &gp);

		Result ResolveQuotedString(ShortTempToken &outToken, const Span<const char> &inToken);
		Result ResolveInputLayoutVertexInputs(const char *filePath, size_t line, size_t col, render::TempStringIndex_t feedName, const Span<const char> &nameBase, Vector<const render::InputLayoutVertexInputDesc *> &descsVector, uint32_t &inOutOffset, const render::ValueType &inputSourcesType, render::InputLayoutVertexFeedDesc *inputFeed);
		Result ResolveInputLayoutVertexInputs(const char *filePath, size_t line, size_t col, render::TempStringIndex_t feedName, const Span<const char> &nameBase, Vector<const render::InputLayoutVertexInputDesc *> &descsVector, uint32_t &inOutOffset, const render::VectorOrScalarNumericType &inputSourcesType, render::InputLayoutVertexFeedDesc *inputFeed);
		static uint32_t ResolvePackedValueTypeSize(const render::ValueType &valueType);
		static uint32_t ResolvePackedStructureTypeSize(const render::StructureType &structureType);
		static uint32_t ResolveCompoundNumericTypeSize(render::NumericType numericType, render::VectorDimension rows, render::VectorDimension cols);
		static uint32_t ResolveVectorOrScalarNumericTypeSize(render::NumericType numericType, render::VectorOrScalarDimension dimension);
		static uint32_t ResolveVectorNumericTypeSize(render::NumericType numericType, render::VectorDimension dimension);
		static uint32_t ResolveNumericTypeSize(render::NumericType numericType);
		static render::VectorOrScalarDimension ConvertToMaybeScalar(render::VectorDimension dimension);

		Result ExpectIdentifier(const char *blamePath, Span<const char> &outToken, utils::ITextParser &parser);
		static Result CheckValidIdentifier(const char *blamePath, const Span<const char> &token, const utils::ITextParser &parser);

		Result ParseEnum(const char *blamePath, const data::RenderRTTIEnumType *rtti, void *obj, bool isConfigurable, utils::ITextParser &parser);
		Result ParseNumber(const char *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, utils::ITextParser &parser);
		Result ParseUInt(const char *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const char> &token, size_t line, size_t col);
		Result ParseSInt(const char *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const char> &token, size_t line, size_t col);
		Result ParseFloat(const char *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const char> &token, size_t line, size_t col);
		Result ParseBool(const char *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const char> &token, size_t line, size_t col);
		Result ParseStruct(const char *blamePath, const data::RenderRTTIStructType *rtti, void *obj, utils::ITextParser &parser);
		Result ParseValue(const char *blamePath, const data::RenderRTTITypeBase *rtti, void *obj, bool isConfigurable, utils::ITextParser &parser);
		Result ParseValueType(const char *blamePath, render::ValueType &valueType, const Span<const char> &token, utils::ITextParser &parser);
		Result ParseConfigurable(const char *blamePath, void *obj, data::RenderRTTIMainType mainType, void (*writeNameFunc)(void *, const render::ConfigStringIndex_t &), utils::ITextParser &parser);
		Result ParseUIntConstant(const char *blamePath, size_t blameLine, size_t blameCol, const Span<const char> &token, uint64_t max, uint64_t &outValue);
		Result ParseStringIndex(const char *blamePath, const data::RenderRTTIStringIndexType *rtti, void *obj, utils::ITextParser &parser);

		Result ParseStructMember(const char *blamePath, const Span<const char> &memberName, const data::RenderRTTIStructType *rtti, void *obj, utils::ITextParser &parser);

		template<class T>
		Result ParseDynamicStructMember(const char *blamePath, const Span<const char> &memberName, const data::RenderRTTIStructType *rtti, T &obj, utils::ITextParser &parser);

		static bool IsTokenChars(const Span<const char> &span, const char *tokenStr, size_t tokenLength);

		template<size_t TSize>
		static bool IsToken(const Span<const char> &span, const char(&tokenChars)[TSize]);

		template<class T>
		static Result Deduplicate(Vector<UniquePtr<T>> &instVector, const T &instance, const T *&outDeduplicated);

		Result IndexString(const Span<const char> &span, render::GlobalStringIndex_t &outStringIndex);
		Result IndexString(const Span<const char> &span, render::TempStringIndex_t &outStringIndex);

		Result IndexString(const render::GlobalStringIndex_t &globalIndex, render::TempStringIndex_t &outStringIndex);

		static DescriptorTypeClassification ClassifyDescriptorType(render::DescriptorType descType);

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

		Vector<UniquePtr<render::PushConstantDesc>> m_pcDescs;
		Vector<UniquePtr<render::StructureMemberDesc>> m_smDescs;
		Vector<UniquePtr<render::InputLayoutVertexInputDesc>> m_vertexInputDescs;
		Vector<UniquePtr<render::DescriptorDesc>> m_dDescs;
		Vector<UniquePtr<render::RenderTargetDesc>> m_rtDescs;
		Vector<UniquePtr<render::RenderOperationDesc>> m_roDescs;
		Vector<UniquePtr<render::ShaderDesc>> m_shaderDescs;
		Vector<UniquePtr<render::DepthStencilOperationDesc>> m_depthStencilOperationDescs;
		Vector<UniquePtr<render::DepthStencilTargetDesc>> m_depthStencilTargetDescs;

		Vector<render::GraphicsPipelineNameLookup> m_graphicsPipelines;
		Vector<render::RenderPassNameLookup> m_renderPasses;

		Vector<SimpleNumericTypeResolution> m_numericTypeResolutions;

		Vector<UniquePtr<render::CompoundNumericType>> m_compoundTypes;
		Vector<UniquePtr<render::VectorNumericType>> m_vectorTypes;
		Vector<UniquePtr<render::VectorOrScalarNumericType>> m_vectorOrScalarTypes;

		Vector<UniquePtr<render::InputLayoutVertexFeedDesc>> m_vertexFeedDescs;
	};
} } } // rkit::buildsystem::rpc_analyzer

namespace rkit { namespace buildsystem { namespace rpc_combiner
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
} } } // rkit::buildsystem::rpc_combiner

namespace rkit { namespace buildsystem { namespace rpc_compiler
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
} } } // rkit::buildsystem::rpc_compiler

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

namespace rkit { namespace buildsystem { namespace rpc_common
{
	Result LibraryCompilerBase::FormatGraphicPipelinePath(CIPath &path, const StringView &identifier, size_t pipelineIndex)
	{
		String str;
		RKIT_CHECK(str.Format("rpll/g_%zu/%s", pipelineIndex, identifier.GetChars()));

		return path.Set(str);
	}

	Result LibraryCompilerBase::FormatGlobalsPath(CIPath &path, const StringView &identifier)
	{
		String str;
		RKIT_CHECK(str.Format("rpll/globs/%s", identifier.GetChars()));

		return path.Set(str);
	}

	Result LibraryCompilerBase::FormatIndexPath(CIPath &path, const StringView &identifier)
	{
		String str;
		RKIT_CHECK(str.Format("rpll/idx/%s", identifier.GetChars()));

		return path.Set(str);
	}

	Result LibraryCompilerBase::FormatCombinedOutputPath(CIPath &path, const StringView &identifier)
	{
		String str;
		RKIT_CHECK(str.Format("rpll/out/%s", identifier.GetChars()));

		return path.Set(str);
	}
} } } // rkit::buildsystem::rpc_common

namespace rkit { namespace buildsystem { namespace rpc_analyzer
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


	Result ShortTempToken::Set(const Span<const char> &span)
	{
		m_chars.Reset();
		m_length = 0;

		if (span.Count() <= kStaticBufferSize)
			memcpy(m_staticBuffer, span.Ptr(), span.Count());
		else
		{
			RKIT_CHECK(m_chars.Append(span));
		}

		m_length = span.Count();

		return ResultCode::kOK;
	}

	Span<char> ShortTempToken::GetChars()
	{
		if (m_chars.Count() > 0)
			return Span<char>(m_chars.GetBuffer(), m_length);
		else
			return Span<char>(m_staticBuffer, m_length);
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
			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("float", render::NumericType::Float32)));
			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("half", render::NumericType::Float16)));
			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("int", render::NumericType::SInt32)));
			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("uint", render::NumericType::UInt32)));

			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("double", render::NumericType::Float64)));
			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("ulong", render::NumericType::UInt64)));
			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("long", render::NumericType::SInt64)));

			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("bool", render::NumericType::Bool)));

			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("byte", render::NumericType::UInt8)));
			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("sbyte", render::NumericType::SInt8)));

			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("short", render::NumericType::SInt16)));
			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("ushort", render::NumericType::UInt16)));

			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("nbyte", render::NumericType::UNorm8)));
			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("nushort", render::NumericType::UNorm16)));

			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("nsbyte", render::NumericType::SNorm8)));
			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("nshort", render::NumericType::SNorm16)));
		}

		CIPath path;
		RKIT_CHECK(path.Set(depsNode->GetIdentifier()));

		RKIT_CHECK(m_includeStack.Append(AnalyzerIncludeStack(depsNode->GetInputFileLocation(), path, false)));

		while (m_includeStack.Count() > 0)
		{
			AnalyzerIncludeStack &topItem = m_includeStack[m_includeStack.Count() - 1];

			if (topItem.m_isScanning)
			{
				RKIT_CHECK(ScanTopStackItem(topItem));
			}
			else
			{
				RKIT_CHECK(ParseTopStackItem(topItem));
			}
		}

		RKIT_CHECK(ExportPackages(depsNode));

		// Generate graphics pipeline shader jobs

		return ResultCode::kOK;
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
			return ResultCode::kOK;
		}

		RKIT_CHECK(m_includedFiles.Add(fKey));

		UniquePtr<ISeekableReadStream> stream;
		RKIT_CHECK(m_feedback->TryOpenInput(item.m_location, item.m_path, stream));

		if (!stream.IsValid())
		{
			if (item.m_canTryAlternate)
			{
				if (item.m_location == BuildFileLocation::kIntermediateDir)
				{
					item.m_location = BuildFileLocation::kSourceDir;
					return ResultCode::kOK;
				}

				rkit::log::ErrorFmt("Could not open input file '%s'", item.m_path.CStr());
				return ResultCode::kFileOpenError;
			}
		}

		IUtilitiesDriver *utils = GetDrivers().m_utilitiesDriver;

		Vector<uint8_t> streamBytes;
		RKIT_CHECK(utils->ReadEntireFile(*stream, streamBytes));

		item.m_fileContents = std::move(streamBytes);

		RKIT_CHECK(utils->CreateTextParser(Span<const char>(reinterpret_cast<char *>(item.m_fileContents.GetBuffer()), item.m_fileContents.Count()), utils::TextParserCommentType::kC, utils::TextParserLexerType::kC, item.m_textParser));
		item.m_isScanning = false;

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseTopStackItem(AnalyzerIncludeStack &item)
	{
		utils::ITextParser *parser = item.m_textParser.Get();

		RKIT_CHECK(parser->SkipWhitespace());

		bool haveMore = true;
		while (haveMore)
		{
			haveMore = false;

			size_t oldStackCount = m_includeStack.Count();
			RKIT_CHECK(ParseDirective(item.m_path.CStr(), *item.m_textParser.Get(), haveMore));

			if (oldStackCount != m_includeStack.Count())
				return ResultCode::kOK;
		}

		// Parsing completed
		RemoveTopStackItem();

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseDirective(const char *path, utils::ITextParser &parser, bool &outHaveDirective)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		bool haveToken = false;
		rkit::Span<const char> directiveToken;
		RKIT_CHECK(parser.ReadToken(haveToken, directiveToken));

		if (!haveToken)
		{
			outHaveDirective = false;
			return ResultCode::kOK;
		}

		outHaveDirective = true;

		Result parseResult(ResultCode::kOK);

		if (IsToken(directiveToken, "include"))
			parseResult = ParseIncludeDirective(path, parser);
		else if (IsToken(directiveToken, "StaticSampler"))
			parseResult = ParseEntity(path, parser, &LibraryAnalyzer::ParseStaticSampler);
		else if (IsToken(directiveToken, "PushConstants"))
			parseResult = ParseEntity(path, parser, &LibraryAnalyzer::ParsePushConstants);
		else if (IsToken(directiveToken, "struct"))
			parseResult = ParseEntity(path, parser, &LibraryAnalyzer::ParseStructDef);
		else if (IsToken(directiveToken, "InputLayout"))
			parseResult = ParseEntity(path, parser, &LibraryAnalyzer::ParseInputLayout);
		else if (IsToken(directiveToken, "DescriptorLayout"))
			parseResult = ParseEntity(path, parser, &LibraryAnalyzer::ParseDescriptorLayout);
		else if (IsToken(directiveToken, "GraphicsPipeline"))
			parseResult = ParseEntity(path, parser, &LibraryAnalyzer::ParseGraphicsPipeline);
		else if (IsToken(directiveToken, "RenderPass"))
			parseResult = ParseEntity(path, parser, &LibraryAnalyzer::ParseRenderPass);
		else
		{
			rkit::log::ErrorFmt("%s [%zu:%zu] Invalid directive", path, line, col);
			return ResultCode::kMalformedFile;
		}

		if (!utils::ResultIsOK(parseResult))
			rkit::log::ErrorFmt("%s [%zu:%zu] Directive parsing failed", path, line, col);

		return parseResult;
	}

	template<class T>
	Result LibraryAnalyzer::ParseEntity(const char *blamePath, utils::ITextParser &parser, Result(LibraryAnalyzer:: *parseFunc)(const char *, utils::ITextParser &, T &))
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		Span<const char> entityNameSpan;
		RKIT_CHECK(ExpectIdentifier(blamePath, entityNameSpan, parser));

		String entityName;
		RKIT_CHECK(entityName.Set(entityNameSpan));

		if (m_entities.Find(entityName) != m_entities.end())
		{
			rkit::log::ErrorFmt("%s [%zu:%zu] Object with this name already exists", blamePath, line, col);
			return ResultCode::kMalformedFile;
		}

		UniquePtr<rpc_interchange::Entity> entity;
		RKIT_CHECK(New<T>(entity));

		RKIT_CHECK(entity->SetName(entityName));

		T *obj = static_cast<T *>(entity.Get());

		RKIT_CHECK((this->*parseFunc)(blamePath, parser, *obj));

		RKIT_CHECK(m_entities.Set(entityName, std::move(entity)));

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseStaticSampler(const char *blamePath, utils::ITextParser &parser, rpc_interchange::StaticSamplerEntity &ss)
	{
		const data::RenderRTTIStructType *rtti = m_dataDriver->GetRenderDataHandler()->GetSamplerDescRTTI();

		RKIT_CHECK(parser.ExpectToken("{"));

		for (;;)
		{
			Span<const char> token;
			RKIT_CHECK(parser.RequireToken(token));

			if (IsToken(token, "}"))
				break;

			RKIT_CHECK(ParseDynamicStructMember(blamePath, token, rtti, ss.GetDesc(), parser));
		}

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParsePushConstants(const char *blamePath, utils::ITextParser &parser, rpc_interchange::PushConstantsEntity &pc)
	{
		const data::RenderRTTIStructType *rtti = m_dataDriver->GetRenderDataHandler()->GetPushConstantDescRTTI();

		RKIT_CHECK(parser.ExpectToken("{"));

		for (;;)
		{
			Span<const char> nameToken;

			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);

			RKIT_CHECK(parser.RequireToken(nameToken));

			if (IsToken(nameToken, "}"))
				break;

			render::TempStringIndex_t tsi;
			RKIT_CHECK(IndexString(nameToken, tsi));

			for (const render::PushConstantDesc *pcDesc : pc.GetPushConstantsVector())
			{
				if (pcDesc->m_name == tsi)
				{
					rkit::log::ErrorFmt("%s [%zu:%zu] Push constant with this name already exists", blamePath, line, col);
					return ResultCode::kMalformedFile;
				}
			}

			RKIT_CHECK(CheckValidIdentifier(blamePath, nameToken, parser));

			RKIT_CHECK(parser.ExpectToken("="));

			UniquePtr<render::PushConstantDesc> pcDesc;
			RKIT_CHECK(New<render::PushConstantDesc>(pcDesc));

			RKIT_CHECK(ParseValue(blamePath, &rtti->m_base, pcDesc.Get(), false, parser));

			RKIT_CHECK(pc.GetPushConstantsVector().Append(pcDesc.Get()));
			RKIT_CHECK(this->m_pcDescs.Append(std::move(pcDesc)));
		}

		pc.GetDesc().m_pushConstants = pc.GetPushConstantsVector().ToSpan();

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseStructDef(const char *blamePath, utils::ITextParser &parser, rpc_interchange::StructDefEntity &sd)
	{
		RKIT_CHECK(parser.ExpectToken("{"));

		for (;;)
		{
			Span<const char> typeToken;
			Span<const char> nameToken;

			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);

			RKIT_CHECK(parser.RequireToken(typeToken));

			if (IsToken(typeToken, "}"))
				break;

			RKIT_CHECK(CheckValidIdentifier(blamePath, typeToken, parser));

			render::ValueType valueType;
			RKIT_CHECK(ParseValueType(blamePath, valueType, typeToken, parser));

			RKIT_CHECK(ExpectIdentifier(blamePath, nameToken, parser));

			render::TempStringIndex_t tsi;
			RKIT_CHECK(IndexString(nameToken, tsi));

			for (const render::StructureMemberDesc *smDesc : sd.GetStructMembersVector())
			{
				if (smDesc->m_name == tsi)
				{
					rkit::log::ErrorFmt("%s [%zu:%zu] Struct member with this name already exists", blamePath, line, col);
					return ResultCode::kMalformedFile;
				}
			}

			if (IsToken(nameToken, "}"))
				break;

			UniquePtr<render::StructureMemberDesc> smDesc;
			RKIT_CHECK(New<render::StructureMemberDesc>(smDesc));

			RKIT_CHECK(IndexString(nameToken, smDesc->m_name));
			smDesc->m_type = valueType;

			RKIT_CHECK(sd.GetStructMembersVector().Append(smDesc.Get()));
			RKIT_CHECK(this->m_smDescs.Append(std::move(smDesc)));
		}

		sd.GetDesc().m_members = sd.GetStructMembersVector().ToSpan();

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseInputLayout(const char *blamePath, utils::ITextParser &parser, rpc_interchange::InputLayoutEntity &il)
	{
		RKIT_CHECK(parser.ExpectToken("{"));

		Vector<VertexInputFeedMapping> feedMappings;

		size_t line = 0;
		size_t col = 0;

		bool hasNumberedMappings = false;
		bool hasSequentialMappings = false;
		for (;;)
		{
			Span<const char> token;
			RKIT_CHECK(parser.RequireToken(token));

			parser.GetLocation(line, col);

			if (IsToken(token, "}"))
				break;

			if (IsToken(token, "VertexInputFeeds"))
			{
				RKIT_CHECK(parser.ExpectToken("="));
				RKIT_CHECK(parser.ExpectToken("{"));

				parser.GetLocation(line, col);
				RKIT_CHECK(parser.RequireToken(token));

				uint32_t slotIndex = 0;

				for (;;)
				{
					if (IsToken(token, "}"))
						break;

					VertexInputFeedMapping feedMapping;
					RKIT_CHECK(feedMapping.m_name.Set(token));

					UniquePtr<render::InputLayoutVertexFeedDesc> feedDesc;
					RKIT_CHECK(New<render::InputLayoutVertexFeedDesc>(feedDesc));

					feedMapping.m_feedDesc = feedDesc.Get();
					RKIT_CHECK(m_vertexFeedDescs.Append(std::move(feedDesc)));



					parser.GetLocation(line, col);
					RKIT_CHECK(parser.RequireToken(token));

					bool isExplicitlyNumbered = false;
					if (IsToken(token, "="))
					{
						RKIT_CHECK(parser.ExpectToken("{"));

						for (;;)
						{
							parser.GetLocation(line, col);
							RKIT_CHECK(parser.RequireToken(token));

							if (IsToken(token, "InputSlot"))
							{
								RKIT_CHECK(parser.ExpectToken("="));
								RKIT_CHECK(parser.RequireToken(token));

								uint64_t value = 0;
								RKIT_CHECK(ParseUIntConstant(blamePath, line, col, token, std::numeric_limits<uint32_t>::max(), value));

								isExplicitlyNumbered = true;
								feedMapping.m_feedDesc->m_inputSlot = static_cast<uint32_t>(value);
							}
							else if (IsToken(token, "Stride"))
							{
								RKIT_CHECK(parser.ExpectToken("="));
								RKIT_CHECK(parser.RequireToken(token));

								uint64_t value = 0;
								RKIT_CHECK(ParseUIntConstant(blamePath, line, col, token, std::numeric_limits<uint32_t>::max(), value));

								feedMapping.m_strideIsSet = true;
								feedMapping.m_feedDesc->m_byteStride.Set(static_cast<uint32_t>(value));
							}
							else if (IsToken(token, "Stepping"))
							{
								RKIT_CHECK(parser.ExpectToken("="));

								const data::RenderRTTIEnumType *enumType = m_dataDriver->GetRenderDataHandler()->GetInputLayoutVertexInputSteppingRTTI();
								RKIT_CHECK(ParseEnum(blamePath, enumType, &feedMapping.m_feedDesc->m_stepping, true, parser));
							}
							else if (IsToken(token, "}"))
								break;
							else
							{
								rkit::log::ErrorFmt("%s [%zu:%zu] Unknown field for input feed", blamePath, line, col);
								return ResultCode::kMalformedFile;
							}
						}

						parser.GetLocation(line, col);
						RKIT_CHECK(parser.RequireToken(token));
					}

					if (isExplicitlyNumbered)
					{
						if (hasSequentialMappings)
						{
							rkit::log::ErrorFmt("%s [%zu:%zu] Can't mix numbered and sequential input feed mappings", blamePath, line, col);
							return ResultCode::kMalformedFile;
						}

						hasNumberedMappings = true;

						for (const VertexInputFeedMapping &existingMapping : feedMappings)
						{
							if (existingMapping.m_feedDesc->m_inputSlot == feedMapping.m_feedDesc->m_inputSlot)
							{
								rkit::log::ErrorFmt("%s [%zu:%zu] Multiple feeds mapped to the same slot", blamePath, line, col);
								return ResultCode::kMalformedFile;
							}
						}
					}
					else
					{
						if (hasNumberedMappings)
						{
							rkit::log::ErrorFmt("%s [%zu:%zu] Can't mix numbered and sequential input feed mappings", blamePath, line, col);
							return ResultCode::kMalformedFile;
						}

						hasSequentialMappings = true;

						if (slotIndex == std::numeric_limits<uint32_t>::max())
						{
							rkit::log::ErrorFmt("%s [%zu:%zu] Too many input slots", blamePath, line, col);
							return ResultCode::kMalformedFile;
						}

						feedMapping.m_feedDesc->m_inputSlot = slotIndex++;
					}

					RKIT_CHECK(IndexString(feedMapping.m_name.ToSpan(), feedMapping.m_feedDesc->m_feedName));

					RKIT_CHECK(feedMappings.Append(std::move(feedMapping)));
				}
			}
			else if (IsToken(token, "VertexInputs"))
			{
				RKIT_CHECK(parser.ExpectToken("="));
				RKIT_CHECK(parser.ExpectToken("{"));

				for (;;)
				{
					RKIT_CHECK(parser.RequireToken(token));

					if (IsToken(token, "}"))
						break;

					RKIT_CHECK(CheckValidIdentifier(blamePath, token, parser));

					render::TempStringIndex_t inputNameStr;
					RKIT_CHECK(IndexString(token, inputNameStr));

					RKIT_CHECK(parser.ExpectToken("="));
					RKIT_CHECK(parser.ExpectToken("{"));

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
						RKIT_CHECK(parser.RequireToken(token));

						if (IsToken(token, "}"))
							break;

						if (IsToken(token, "InputFeed"))
						{
							RKIT_CHECK(parser.ExpectToken("="));

							parser.GetLocation(line, col);
							RKIT_CHECK(parser.RequireToken(token));

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
								rkit::log::ErrorFmt("%s [%zu:%zu] Unknown input feed", blamePath, line, col);
								return ResultCode::kMalformedFile;
							}
						}
						else if (IsToken(token, "InputSources"))
						{
							RKIT_CHECK(parser.ExpectToken("="));

							RKIT_CHECK(parser.RequireToken(token));

							RKIT_CHECK(ParseValueType(blamePath, inputSourcesType, token, parser));
							haveInputSourcesType = true;
						}
						else if (IsToken(token, "BaseOffset"))
						{
							RKIT_CHECK(parser.ExpectToken("="));

							parser.GetLocation(line, col);
							RKIT_CHECK(parser.RequireToken(token));

							uint64_t baseOffsetValue = 0;
							RKIT_CHECK(ParseUIntConstant(blamePath, line, col, token, std::numeric_limits<uint32_t>::max(), baseOffsetValue));

							baseOffset = static_cast<uint32_t>(baseOffsetValue);
						}
						else
						{
							rkit::log::ErrorFmt("%s [%zu:%zu] Invalid field for vertex input", blamePath, line, col);
							return ResultCode::kMalformedFile;
						}
					}

					if (!inputFeedMapping)
					{
						rkit::log::ErrorFmt("%s [%zu:%zu] No input feeds were defined", blamePath, line, col);
						return ResultCode::kMalformedFile;
					}

					if (!haveInputSourcesType)
					{
						rkit::log::ErrorFmt("%s [%zu:%zu] No input source type was defined", blamePath, line, col);
						return ResultCode::kMalformedFile;
					}

					RKIT_CHECK(ResolveInputLayoutVertexInputs(blamePath, defStartLine, defStartCol, inputNameStr, Span<const char>(), il.GetVertexInputs(), baseOffset, inputSourcesType, inputFeedMapping->m_feedDesc));

					if (!inputFeedMapping->m_strideIsSet || inputFeedMapping->m_strideIsSetAutomatically)
					{
						uint32_t autoStride = ResolvePackedValueTypeSize(inputSourcesType);

						if (inputFeedMapping->m_strideIsSetAutomatically && autoStride != inputFeedMapping->m_feedDesc->m_byteStride.m_u.m_value)
						{
							rkit::log::ErrorFmt("%s [%zu:%zu] Automatic stride mismatch", blamePath, line, col);
							return ResultCode::kMalformedFile;
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
				rkit::log::ErrorFmt("%s [%zu:%zu] Invalid entry type in InputLayout", blamePath, line, col);
				return ResultCode::kMalformedFile;
			}
		}

		for (const VertexInputFeedMapping &feedMapping : feedMappings)
		{
			if (!feedMapping.m_strideIsSet)
			{
				parser.GetLocation(line, col);
				rkit::log::ErrorFmt("%s [%zu:%zu] Feed mapping '%s' had no stride", blamePath, line, col, feedMapping.m_name.CStr());
				return ResultCode::kMalformedFile;
			}
		}

		il.GetDesc().m_vertexInputs = il.GetVertexInputs().ToSpan();

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseDescriptorLayout(const char *filePath, utils::ITextParser &parser, rpc_interchange::DescriptorLayoutEntity &dl)
	{
		RKIT_CHECK(parser.ExpectToken("{"));

		const data::RenderRTTIStructType *descRTTI = m_dataDriver->GetRenderDataHandler()->GetDescriptorDescRTTI();

		for (;;)
		{
			Span<const char> token;
			RKIT_CHECK(parser.RequireToken(token));

			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);

			if (IsToken(token, "}"))
				break;

			render::TempStringIndex_t descNameIndex;
			RKIT_CHECK(IndexString(token, descNameIndex));

			for (const render::DescriptorDesc *existingDesc : dl.GetDescriptorDescs())
			{
				if (existingDesc->m_name == descNameIndex)
				{
					rkit::log::ErrorFmt("%s [%zu:%zu] Descriptor with that name already exists", filePath, line, col);
					return ResultCode::kMalformedFile;
				}
			}

			RKIT_CHECK(parser.ExpectToken("="));

			UniquePtr<render::DescriptorDesc> descDesc;
			RKIT_CHECK(New<render::DescriptorDesc>(descDesc));

			RKIT_CHECK(parser.ExpectToken("{"));

			RKIT_CHECK(parser.RequireToken(token));

			bool typeWasSpecified = false;
			for (;;)
			{
				parser.GetLocation(line, col);

				if (IsToken(token, "}"))
					break;

				if (IsToken(token, "Type"))
				{
					if (typeWasSpecified)
					{
						rkit::log::ErrorFmt("%s [%zu:%zu] Type was already specified", filePath, line, col);
						return ResultCode::kMalformedFile;
					}

					typeWasSpecified = true;

					RKIT_CHECK(parser.ExpectToken("="));

					parser.GetLocation(line, col);
					RKIT_CHECK(parser.RequireToken(token));

					const data::RenderRTTIEnumType *typeEnumType = m_dataDriver->GetRenderDataHandler()->GetDescriptorTypeRTTI();
					render::DescriptorType descType = render::DescriptorType::Count;

					for (size_t i = 0; i < typeEnumType->m_numOptions; i++)
					{
						const data::RenderRTTIEnumOption &option = typeEnumType->m_options[i];
						if (IsTokenChars(token, option.m_name, option.m_nameLength))
						{
							descType = static_cast<render::DescriptorType>(option.m_value);
							break;
						}
					}

					if (descType == render::DescriptorType::Count)
					{
						rkit::log::ErrorFmt("%s [%zu:%zu] Invalid descriptor type", filePath, line, col);
						return ResultCode::kMalformedFile;
					}

					descDesc->m_descriptorType = descType;

					DescriptorTypeClassification classification = ClassifyDescriptorType(descType);

					switch (classification)
					{
					case DescriptorTypeClassification::Texture:
					case DescriptorTypeClassification::RWTexture:
						RKIT_CHECK(parser.RequireToken(token));
						if (IsToken(token, "<"))
						{
							parser.GetLocation(line, col);

							RKIT_CHECK(parser.RequireToken(token));
							RKIT_CHECK(ParseValueType(filePath, descDesc->m_valueType, token, parser));
							RKIT_CHECK(parser.ExpectToken(">"));

							if (descDesc->m_valueType.m_type != render::ValueTypeType::Numeric && descDesc->m_valueType.m_type != render::ValueTypeType::VectorNumeric)
							{
								rkit::log::ErrorFmt("%s [%zu:%zu] Invalid type for texture", filePath, line, col);
								return ResultCode::kMalformedFile;
							}

							RKIT_CHECK(parser.RequireToken(token));
						}
						break;
					case DescriptorTypeClassification::Buffer:
					case DescriptorTypeClassification::ConstantBuffer:
						RKIT_CHECK(parser.ExpectToken("<"));

						parser.GetLocation(line, col);

						RKIT_CHECK(parser.RequireToken(token));
						RKIT_CHECK(ParseValueType(filePath, descDesc->m_valueType, token, parser));
						RKIT_CHECK(parser.ExpectToken(">"));

						RKIT_CHECK(parser.RequireToken(token));
						break;
					case DescriptorTypeClassification::ByteAddressBuffer:
					case DescriptorTypeClassification::Sampler:
						RKIT_CHECK(parser.RequireToken(token));
						break;

					default:
						return ResultCode::kInternalError;
					}

					if (IsToken(token, "["))
					{
						parser.GetLocation(line, col);
						RKIT_CHECK(parser.RequireToken(token));

						if (IsToken(token, "]"))
							descDesc->m_arraySize = 0;
						else
						{
							uint64_t arraySize = 0;
							RKIT_CHECK(ParseUIntConstant(filePath, line, col, token, std::numeric_limits<uint32_t>::max(), arraySize));

							if (arraySize < 2)
							{
								rkit::log::ErrorFmt("%s [%zu:%zu] Invalid descriptor array size", filePath, line, col);
								return ResultCode::kMalformedFile;
							}

							descDesc->m_arraySize = static_cast<uint32_t>(arraySize);

							RKIT_CHECK(parser.RequireToken(token));
						}
					}
				}
				else if (IsToken(token, "Sampler"))
				{
					if (!typeWasSpecified)
					{
						rkit::log::ErrorFmt("%s [%zu:%zu] Static sampler must be after type", filePath, line, col);
						return ResultCode::kMalformedFile;
					}

					DescriptorTypeClassification classification = ClassifyDescriptorType(descDesc->m_descriptorType);
					if (classification != DescriptorTypeClassification::Texture)
					{
						rkit::log::ErrorFmt("%s [%zu:%zu] Static sampler is only valid for texture types", filePath, line, col);
						return ResultCode::kMalformedFile;
					}

					RKIT_CHECK(parser.ExpectToken("="));

					parser.GetLocation(line, col);

					RKIT_CHECK(parser.RequireToken(token));

					HashMap<String, UniquePtr<rpc_interchange::Entity>>::ConstIterator_t it = m_entities.Find(BaseStringSliceView<char>(token));
					if (it == m_entities.end())
					{
						rkit::log::ErrorFmt("%s [%zu:%zu] Unknown static sampler", filePath, line, col);
						return ResultCode::kMalformedFile;
					}

					if (it.Value()->GetEntityType() != rpc_interchange::EntityType::StaticSampler)
					{
						rkit::log::ErrorFmt("%s [%zu:%zu] Value wasn't a static sampler", filePath, line, col);
						return ResultCode::kMalformedFile;
					}

					descDesc->m_staticSamplerDesc = &static_cast<const rpc_interchange::StaticSamplerEntity *>(it.Value().Get())->GetDesc();

					RKIT_CHECK(parser.RequireToken(token));
				}
				else
				{
					rkit::log::ErrorFmt("%s [%zu:%zu] Invalid descriptor desc property", filePath, line, col);
					return ResultCode::kMalformedFile;
				}
			}

			if (!typeWasSpecified)
			{
				rkit::log::ErrorFmt("%s [%zu:%zu] Descriptor missing type", filePath, line, col);
				return ResultCode::kMalformedFile;
			}

			descDesc->m_name = descNameIndex;

			RKIT_CHECK(dl.GetDescriptorDescs().Append(descDesc.Get()));

			RKIT_CHECK(m_dDescs.Append(std::move(descDesc)));
		}

		dl.GetDesc().m_descriptors = dl.GetDescriptorDescs().ToSpan();

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseGraphicsPipeline(const char *filePath, utils::ITextParser &parser, rpc_interchange::GraphicsPipelineEntity &gp)
	{
		RKIT_CHECK(parser.ExpectToken("{"));

		const data::RenderRTTIStructType *pipelineRTTI = m_dataDriver->GetRenderDataHandler()->GetGraphicsPipelineDescRTTI();
		const data::RenderRTTIStructType *shaderDescRTTI = m_dataDriver->GetRenderDataHandler()->GetShaderDescRTTI();
		const data::RenderRTTIStructType *depthStencilOperationDescRTTI = m_dataDriver->GetRenderDataHandler()->GetDepthStencilOperationDescRTTI();

		HashMap<String, const render::RenderOperationDesc *> nameToRODesc;

		for (;;)
		{
			Span<const char> token;
			RKIT_CHECK(parser.RequireToken(token));

			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);

			if (IsToken(token, "}"))
				break;

			if (IsToken(token, "DescriptorLayouts"))
			{
				RKIT_CHECK(parser.ExpectToken("="));
				RKIT_CHECK(parser.ExpectToken("{"));

				for (;;)
				{
					parser.GetLocation(line, col);

					RKIT_CHECK(parser.RequireToken(token));

					if (IsToken(token, "}"))
						break;

					HashMap<String, UniquePtr<rpc_interchange::Entity>>::ConstIterator_t it = m_entities.Find(StringSliceView(token));

					if (it == m_entities.end())
					{
						rkit::log::ErrorFmt("%s [%zu:%zu] Couldn't resolve descriptor layout", filePath, line, col);
						return ResultCode::kMalformedFile;
					}

					if (it.Value()->GetEntityType() != rpc_interchange::EntityType::DescriptorLayout)
					{
						rkit::log::ErrorFmt("%s [%zu:%zu] Value wasn't a descriptor layout", filePath, line, col);
						return ResultCode::kMalformedFile;
					}

					RKIT_CHECK(gp.GetDescriptorLayouts().Append(&static_cast<rpc_interchange::DescriptorLayoutEntity *>(it.Value().Get())->GetDesc()));
				}
			}
			else if (IsToken(token, "RenderTargets"))
			{
				RKIT_CHECK(parser.ExpectToken("="));
				RKIT_CHECK(parser.ExpectToken("{"));

				for (;;)
				{
					parser.GetLocation(line, col);

					RKIT_CHECK(parser.RequireToken(token));

					if (IsToken(token, "}"))
						break;

					RKIT_CHECK(CheckValidIdentifier(filePath, token, parser));

					String rtName;
					RKIT_CHECK(rtName.Set(token));

					UniquePtr<render::RenderOperationDesc> roDesc;
					RKIT_CHECK(New<render::RenderOperationDesc>(roDesc));

					RKIT_CHECK(parser.ExpectToken("="));

					if (nameToRODesc.Find(rtName) != nameToRODesc.end())
					{
						rkit::log::ErrorFmt("%s [%zu:%zu] Render target '%s' was specified multiple times", filePath, line, col, rtName.CStr());
						return ResultCode::kMalformedFile;
					}

					RKIT_CHECK(ParseStruct(filePath, m_dataDriver->GetRenderDataHandler()->GetRenderOperationDescRTTI(), roDesc.Get(), parser));

					RKIT_CHECK(nameToRODesc.Set(std::move(rtName), roDesc.Get()));

					RKIT_CHECK(m_roDescs.Append(std::move(roDesc)));
				}
			}
			else if (IsToken(token, "PushConstants"))
			{
				return ResultCode::kNotYetImplemented;
			}
			else if (IsToken(token, "VertexShaderOutput"))
			{
				return ResultCode::kNotYetImplemented;
			}
			else if (IsToken(token, "InputLayout"))
			{
				RKIT_CHECK(parser.ExpectToken("="));

				parser.GetLocation(line, col);
				RKIT_CHECK(parser.RequireToken(token));

				HashMap<String, UniquePtr<rpc_interchange::Entity>>::ConstIterator_t it = m_entities.Find(StringSliceView(token));
				if (it == m_entities.end())
				{
					rkit::log::ErrorFmt("%s [%zu:%zu] Unknown input layout identifier", filePath, line, col);
					return ResultCode::kMalformedFile;
				}

				if (it.Value()->GetEntityType() != rpc_interchange::EntityType::InputLayout)
				{
					rkit::log::ErrorFmt("%s [%zu:%zu] Value wasn't an input layout", filePath, line, col);
					return ResultCode::kMalformedFile;
				}

				gp.GetDesc().m_inputLayout = &static_cast<rpc_interchange::InputLayoutEntity *>(it.Value().Get())->GetDesc();
			}
			else if (IsToken(token, "VertexShader") || IsToken(token, "PixelShader"))
			{
				bool isVS = IsToken(token, "VertexShader");
				bool isPS = IsToken(token, "PixelShader");

				RKIT_CHECK(parser.ExpectToken("="));

				UniquePtr<render::ShaderDesc> shaderDesc;
				RKIT_CHECK(New<render::ShaderDesc>(shaderDesc));

				RKIT_CHECK(ParseStruct(filePath, shaderDescRTTI, shaderDesc.Get(), parser));

				if (isVS)
					gp.GetDesc().m_vertexShader = shaderDesc.Get();

				if (isPS)
					gp.GetDesc().m_pixelShader = shaderDesc.Get();

				RKIT_CHECK(m_shaderDescs.Append(std::move(shaderDesc)));
			}
			else if (IsToken(token, "DepthStencil"))
			{
				RKIT_CHECK(parser.ExpectToken("="));

				UniquePtr<render::DepthStencilOperationDesc> depthStencil;
				RKIT_CHECK(New<render::DepthStencilOperationDesc>(depthStencil));

				RKIT_CHECK(ParseStruct(filePath, depthStencilOperationDescRTTI, depthStencil.Get(), parser));

				gp.GetDesc().m_depthStencil = depthStencil.Get();

				RKIT_CHECK(m_depthStencilOperationDescs.Append(std::move(depthStencil)));
			}
			else if (IsToken(token, "ExecuteInPass"))
			{
				RKIT_CHECK(parser.ExpectToken("="));

				parser.GetLocation(line, col);
				RKIT_CHECK(parser.RequireToken(token));

				HashMap<String, UniquePtr<rpc_interchange::Entity>>::ConstIterator_t it = m_entities.Find(StringSliceView(token));
				if (it == m_entities.end())
				{
					rkit::log::ErrorFmt("%s [%zu:%zu] Unknown execute in pass identifier", filePath, line, col);
					return ResultCode::kMalformedFile;
				}

				if (it.Value()->GetEntityType() != rpc_interchange::EntityType::RenderPass)
				{
					rkit::log::ErrorFmt("%s [%zu:%zu] Value wasn't a render pass", filePath, line, col);
					return ResultCode::kMalformedFile;
				}

				gp.GetDesc().m_executeInPass = &static_cast<rpc_interchange::RenderPassEntity *>(it.Value().Get())->GetDesc();
			}
			else
			{
				RKIT_CHECK(ParseStructMember(filePath, token, pipelineRTTI, &gp.GetDesc(), parser));
			}
		}

		gp.GetPipelineLayout().m_descriptorLayouts = gp.GetDescriptorLayouts().ToSpan();

		gp.GetDesc().m_pipelineLayout = &gp.GetPipelineLayout();

		const render::RenderPassDesc *renderPass = gp.GetDesc().m_executeInPass;

		if (!renderPass)
		{
			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);
			rkit::log::ErrorFmt("%s [%zu:%zu] Pipeline did not specify ExecuteInPass", filePath, line, col);
			return ResultCode::kMalformedFile;
		}

		if ((renderPass->m_depthStencilTarget == nullptr) && (gp.GetDesc().m_depthStencil != nullptr))
		{
			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);
			rkit::log::ErrorFmt("%s [%zu:%zu] Pipeline has depth/stencil operations but there is no depth/stencil in the corresponding render pass", filePath, line, col);
			return ResultCode::kMalformedFile;
		}

		if ((renderPass->m_depthStencilTarget != nullptr) && (gp.GetDesc().m_depthStencil == nullptr))
		{
			UniquePtr<render::DepthStencilOperationDesc> depthStencilDesc;
			RKIT_CHECK(New<render::DepthStencilOperationDesc>(depthStencilDesc));

			depthStencilDesc->m_depthTest.Set(false);
			depthStencilDesc->m_depthWrite.Set(false);

			gp.GetDesc().m_depthStencil = depthStencilDesc.Get();

			RKIT_CHECK(m_depthStencilOperationDescs.Append(std::move(depthStencilDesc)));
		}

		{
			const ConstSpan<const render::RenderTargetDesc *> renderTargets = renderPass->m_renderTargets;

			size_t numRenderTargets = renderTargets.Count();

			Vector<bool> rtUsed;
			RKIT_CHECK(rtUsed.Resize(numRenderTargets));

			for (bool &b : rtUsed)
				b = false;

			RKIT_CHECK(gp.GetRenderOperations().Resize(numRenderTargets));

			for (HashMapKeyValueView<String, const render::RenderOperationDesc *> kv : nameToRODesc)
			{
				render::TempStringIndex_t nameStr;
				RKIT_CHECK(IndexString(kv.Key().ToSpan(), nameStr));

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
					rkit::log::ErrorFmt("%s [%zu:%zu] Pipeline target '%s' didn't exist in the render pass", filePath, line, col, kv.Key().CStr());
				}
			}

			const render::RenderOperationDesc *defaultRODesc = nullptr;

			for (size_t i = 0; i < rtUsed.Count(); i++)
			{
				if (!rtUsed[i])
				{
					if (!defaultRODesc)
					{
						UniquePtr<render::RenderOperationDesc> defaultRODescNew;
						RKIT_CHECK(New<render::RenderOperationDesc>(defaultRODescNew));

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

						RKIT_CHECK(this->m_roDescs.Append(std::move(defaultRODescNew)));
					}

					gp.GetRenderOperations()[i] = defaultRODesc;
				}
			}
		}

		gp.GetDesc().m_renderTargets = gp.GetRenderOperations().ToSpan();

		render::GraphicsPipelineNameLookup nameLookup;
		RKIT_CHECK(IndexString(gp.GetName().ToSpan(), nameLookup.m_name));
		nameLookup.m_pipeline = &gp.GetDesc();

		RKIT_CHECK(m_graphicsPipelines.Append(nameLookup));

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseRenderPass(const char *filePath, utils::ITextParser &parser, rpc_interchange::RenderPassEntity &rp)
	{
		RKIT_CHECK(parser.ExpectToken("{"));

		const data::RenderRTTIStructType *renderPassDescRTTI = m_dataDriver->GetRenderDataHandler()->GetRenderPassDescRTTI();
		const data::RenderRTTIStructType *renderTargetDescRTTI = m_dataDriver->GetRenderDataHandler()->GetRenderTargetDescRTTI();
		const data::RenderRTTIStructType *depthStencilTargetDescRTTI = m_dataDriver->GetRenderDataHandler()->GetDepthStencilTargetDescRTTI();

		for (;;)
		{
			Span<const char> token;
			RKIT_CHECK(parser.RequireToken(token));

			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);

			if (IsToken(token, "}"))
				break;

			if (IsToken(token, "RenderTargets"))
			{
				RKIT_CHECK(parser.ExpectToken("="));
				RKIT_CHECK(parser.ExpectToken("{"));

				for (;;)
				{
					parser.GetLocation(line, col);

					RKIT_CHECK(parser.RequireToken(token));

					if (IsToken(token, "}"))
						break;

					RKIT_CHECK(CheckValidIdentifier(filePath, token, parser));

					UniquePtr<render::RenderTargetDesc> rtDesc;
					RKIT_CHECK(New<render::RenderTargetDesc>(rtDesc));

					RKIT_CHECK(IndexString(token, rtDesc->m_name));

					RKIT_CHECK(parser.ExpectToken("="));

					RKIT_CHECK(ParseStruct(filePath, renderTargetDescRTTI, rtDesc.Get(), parser));

					RKIT_CHECK(rp.GetRenderTargetDescs().Append(rtDesc.Get()));

					RKIT_CHECK(m_rtDescs.Append(std::move(rtDesc)));
				}
			}
			else if (IsToken(token, "DepthStencil"))
			{
				RKIT_CHECK(parser.ExpectToken("="));

				UniquePtr<render::DepthStencilTargetDesc> depthStencil;
				RKIT_CHECK(New<render::DepthStencilTargetDesc>(depthStencil));

				RKIT_CHECK(ParseStruct(filePath, depthStencilTargetDescRTTI, depthStencil.Get(), parser));

				rp.GetDesc().m_depthStencilTarget = depthStencil.Get();

				RKIT_CHECK(m_depthStencilTargetDescs.Append(std::move(depthStencil)));
			}
			else
			{
				RKIT_CHECK(ParseStructMember(filePath, token, renderPassDescRTTI, &rp.GetDesc(), parser));
			}
		}

		rp.GetDesc().m_renderTargets = rp.GetRenderTargetDescs().ToSpan();

		render::RenderPassNameLookup nameLookup;
		RKIT_CHECK(IndexString(rp.GetName().ToSpan(), nameLookup.m_name));
		nameLookup.m_renderPass = &rp.GetDesc();

		RKIT_CHECK(m_renderPasses.Append(nameLookup));

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseIncludeDirective(const char *blamePath, utils::ITextParser &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		ShortTempToken path;

		Span<const char> token;
		RKIT_CHECK(parser.RequireToken(token));

		RKIT_CHECK(ResolveQuotedString(path, token));

		IUtilitiesDriver *utils = GetDrivers().m_utilitiesDriver;

		utils->NormalizeFilePath(path.GetChars());
		if (!utils->ValidateFilePath(path.GetChars(), false))
		{
			rkit::log::ErrorFmt("%s [%zu:%zu] Invalid file path", blamePath, line, col);
			return ResultCode::kMalformedFile;
		}

		BuildFileLocation loc = m_includeStack[m_includeStack.Count() - 1].m_location;

		String str;
		RKIT_CHECK(str.Set(path.GetChars()));

		CIPath ciPath;
		RKIT_CHECK(ciPath.Set(str));

		RKIT_CHECK(m_includeStack.Append(AnalyzerIncludeStack(loc, ciPath, true)));

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ResolveQuotedString(ShortTempToken &outToken, const Span<const char> &inToken)
	{
		RKIT_CHECK(outToken.Set(inToken));

		size_t newLength = 0;
		RKIT_CHECK(GetDrivers().m_utilitiesDriver->EscapeCStringInPlace(outToken.GetChars(), newLength));

		outToken.Truncate(newLength);

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ResolveInputLayoutVertexInputs(const char *blamePath, size_t line, size_t col, render::TempStringIndex_t feedName, const Span<const char> &nameBase, Vector<const render::InputLayoutVertexInputDesc *> &descsVector, uint32_t &inOutOffset, const render::VectorOrScalarNumericType &inputSourcesType, render::InputLayoutVertexFeedDesc *inputFeed)
	{
		uint32_t sz = ResolveVectorOrScalarNumericTypeSize(inputSourcesType.m_numericType, inputSourcesType.m_cols);

		render::TempStringIndex_t memberName;
		if (nameBase.Count() == 0)
		{
			RKIT_CHECK(IndexString(StringView("Value").ToSpan(), memberName));
		}
		else
		{
			RKIT_CHECK(IndexString(nameBase, memberName));
		}

		const render::VectorOrScalarNumericType *deduplicated = nullptr;
		RKIT_CHECK(Deduplicate(m_vectorOrScalarTypes, inputSourcesType, deduplicated));

		UniquePtr<render::InputLayoutVertexInputDesc> vid;
		RKIT_CHECK(New<render::InputLayoutVertexInputDesc>(vid));
		vid->m_byteOffset = inOutOffset;
		vid->m_inputFeed = inputFeed;
		vid->m_memberName = memberName;
		vid->m_numericType = deduplicated;

		RKIT_CHECK(descsVector.Append(vid.Get()));

		RKIT_CHECK(m_vertexInputDescs.Append(std::move(vid)));

		inOutOffset += sz;

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ResolveInputLayoutVertexInputs(const char *blamePath, size_t line, size_t col, render::TempStringIndex_t feedName, const Span<const char> &nameBase, Vector<const render::InputLayoutVertexInputDesc *> &descsVector, uint32_t &inOutOffset, const render::ValueType &inputSourcesType, render::InputLayoutVertexFeedDesc *inputFeed)
	{
		switch (inputSourcesType.m_type)
		{
		case render::ValueTypeType::CompoundNumeric:
			rkit::log::ErrorFmt("%s [%zu:%zu] Matrix types aren't allowed in vertex inputs", blamePath, line, col);
			return ResultCode::kMalformedFile;

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
						Vector<char> memberNameBaseChars;
						RKIT_CHECK(memberNameBaseChars.Append(memberNameBaseChars.ToSpan()));
						RKIT_CHECK(memberNameBaseChars.Append('_'));

						RKIT_CHECK(ResolveInputLayoutVertexInputs(blamePath, line, col, feedName, memberNameBaseChars.ToSpan(), descsVector, inOutOffset, memberType, inputFeed));
					}
					else
					{
						RKIT_CHECK(ResolveInputLayoutVertexInputs(blamePath, line, col, feedName, memberName.ToSpan(), descsVector, inOutOffset, memberType, inputFeed));
					}
				}
			}
			break;

		default:
			return ResultCode::kMalformedFile;
		}

		return ResultCode::kOK;
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

	Result LibraryAnalyzer::ExpectIdentifier(const char *blamePath, Span<const char> &outToken, utils::ITextParser &parser)
	{
		RKIT_CHECK(parser.RequireToken(outToken));

		return CheckValidIdentifier(blamePath, outToken, parser);
	}

	Result LibraryAnalyzer::CheckValidIdentifier(const char *blamePath, const Span<const char> &token, const utils::ITextParser &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		const char *tokenChars = token.Ptr();
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
				rkit::log::ErrorFmt("%s [%zu:%zu] Expected identifier", blamePath, line, col);
				return ResultCode::kMalformedFile;
			}
		}

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseValue(const char *blamePath, const data::RenderRTTITypeBase *rtti, void *obj, bool isConfigurable, utils::ITextParser &parser)
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

				Span<const char> token;
				RKIT_CHECK(ExpectIdentifier(blamePath, token, parser));

				return ParseValueType(blamePath, *static_cast<render::ValueType *>(obj), token, parser);
			}
			break;
		case data::RenderRTTIType::StringIndex:
			RKIT_ASSERT(!isConfigurable);
			return ParseStringIndex(blamePath, reinterpret_cast<const data::RenderRTTIStringIndexType *>(rtti), obj, parser);
		default:
			return ResultCode::kInternalError;
		}
	}

	Result LibraryAnalyzer::ParseValueType(const char *blamePath, render::ValueType &valueType, const Span<const char> &token, utils::ITextParser &parser)
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
				return ResultCode::kOK;
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
					RKIT_CHECK(Deduplicate(m_vectorTypes, vectorType, deduplicated));

					valueType = render::ValueType(deduplicated);
					return ResultCode::kOK;
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
					compoundType.m_cols = static_cast<render::VectorDimension>(static_cast<int>(render::VectorDimension::Dimension2) + (c0 - '2'));
					compoundType.m_rows = static_cast<render::VectorDimension>(static_cast<int>(render::VectorDimension::Dimension2) + (c2 - '2'));
					compoundType.m_numericType = ntr.m_numericType;

					const render::CompoundNumericType *deduplicated = nullptr;
					RKIT_CHECK(Deduplicate(m_compoundTypes, compoundType, deduplicated));

					valueType = render::ValueType(deduplicated);
					return ResultCode::kOK;
				}
			}
		}

		HashMap<String, UniquePtr<rpc_interchange::Entity>>::ConstIterator_t it = m_entities.Find(StringSliceView(token));
		if (it == m_entities.end())
		{
			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);
			rkit::log::ErrorFmt("%s [%zu:%zu] Unknown type identifier", blamePath, line, col);
			return ResultCode::kMalformedFile;
		}

		const rpc_interchange::Entity *entity = it.Value().Get();

		if (entity->GetEntityType() != rpc_interchange::EntityType::StructDef)
		{
			size_t line = 0;
			size_t col = 0;
			parser.GetLocation(line, col);
			rkit::log::ErrorFmt("%s [%zu:%zu] Identifier does not resolve to a structure", blamePath, line, col);
			return ResultCode::kMalformedFile;
		}

		valueType = render::ValueType(&static_cast<const rpc_interchange::StructDefEntity *>(entity)->GetDesc());

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseConfigurable(const char *blamePath, void *obj, data::RenderRTTIMainType mainType, void (*writeNameFunc)(void *, const render::ConfigStringIndex_t &), utils::ITextParser &parser)
	{
		Span<const char> configName;

		RKIT_CHECK(parser.ExpectToken("("));

		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		RKIT_CHECK(ExpectIdentifier(blamePath, configName, parser));
		RKIT_CHECK(parser.ExpectToken(")"));

		render::GlobalStringIndex_t configNameSI;
		RKIT_CHECK(IndexString(configName, configNameSI));

		HashMap<render::GlobalStringIndex_t, render::ConfigStringIndex_t>::ConstIterator_t keyIt = m_configNameToKey.Find(configNameSI);
		if (keyIt != m_configNameToKey.end())
		{
			const ConfigKey &configKey = m_configKeys[keyIt.Value().GetIndex()];

			if (configKey.m_mainType != mainType)
			{
				rkit::log::ErrorFmt("%s [%zu:%zu] Config key was already used for a different type", blamePath, line, col);
				return ResultCode::kMalformedFile;
			}

			writeNameFunc(obj, keyIt.Value());
			return ResultCode::kOK;
		}

		ConfigKey configKey;
		configKey.m_mainType = mainType;
		configKey.m_name = configNameSI;

		render::ConfigStringIndex_t configSI(m_configKeys.Count());

		RKIT_CHECK(m_configNameToKey.Set(configNameSI, configSI));
		RKIT_CHECK(m_configKeys.Append(std::move(configKey)));

		writeNameFunc(obj, configSI);

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseUIntConstant(const char *blamePath, size_t blameLine, size_t blameCol, const Span<const char> &token, uint64_t max, uint64_t &outValue)
	{
		uint64_t result = 0;

		if (token.Count() == 0)
		{
			outValue = 0;
			return ResultCode::kOK;
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
				rkit::log::ErrorFmt("%s [%zu:%zu] Expected numeric constant", blamePath, blameLine, blameCol);
				return ResultCode::kMalformedFile;
			}

			if (result >= maxBeforeMultiply)
			{
				rkit::log::ErrorFmt("%s [%zu:%zu] Integer constant overflow", blamePath, blameLine, blameCol);
				return ResultCode::kMalformedFile;
			}

			if (digit >= base)
			{
				rkit::log::ErrorFmt("%s [%zu:%zu] Invalid digit in numeric constant", blamePath, blameLine, blameCol);
				return ResultCode::kMalformedFile;
			}

			result *= base;

			if (max - result < digit)
			{
				rkit::log::ErrorFmt("%s [%zu:%zu] Integer constant overflow", blamePath, blameLine, blameCol);
				return ResultCode::kMalformedFile;
			}

			result += digit;
		}

		outValue = result;

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseStringIndex(const char *blamePath, const data::RenderRTTIStringIndexType *rtti, void *obj, utils::ITextParser &parser)
	{
		size_t line = 0;
		size_t col = 0;

		parser.GetLocation(line, col);

		Span<const char> token;
		RKIT_CHECK(parser.RequireToken(token));

		if (token.Count() == 0 || token[0] != '\"')
		{
			rkit::log::ErrorFmt("%s [%zu:%zu] Expected string constant", blamePath, line, col);
			return ResultCode::kMalformedFile;
		}

		rkit::IUtilitiesDriver *utils = GetDrivers().m_utilitiesDriver;

		Vector<char> escapedStr;
		RKIT_CHECK(escapedStr.Append(token));

		size_t newLength = 0;
		RKIT_CHECK(utils->EscapeCStringInPlace(escapedStr.ToSpan(), newLength));

		RKIT_CHECK(escapedStr.Resize(newLength));

		render::GlobalStringIndex_t globalIndex;
		RKIT_CHECK(IndexString(escapedStr.ToSpan(), globalIndex));

		int purpose = rtti->m_getPurposeFunc();

		if (purpose == render::GlobalStringIndex_t::kPurpose)
			rtti->m_writeStringIndexFunc(obj, globalIndex.GetIndex());
		else if (purpose == render::TempStringIndex_t::kPurpose)
		{
			render::TempStringIndex_t tempIndex;
			RKIT_CHECK(IndexString(globalIndex, tempIndex));

			rtti->m_writeStringIndexFunc(obj, tempIndex.GetIndex());
		}
		else
			return ResultCode::kInternalError;

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseEnum(const char *blamePath, const data::RenderRTTIEnumType *rtti, void *obj, bool isConfigurable, utils::ITextParser &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		Span<const char> option;
		RKIT_CHECK(parser.RequireToken(option));

		if (IsToken(option, "Config"))
		{
			if (!isConfigurable)
			{
				rkit::log::ErrorFmt("%s [%zu:%zu] Option is not configurable", blamePath, line, col);
				return ResultCode::kMalformedFile;
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

				return ResultCode::kOK;
			}
		}

		rkit::log::ErrorFmt("%s [%zu:%zu] Invalid value", blamePath, line, col);
		return ResultCode::kMalformedFile;
	}

	Result LibraryAnalyzer::ParseNumber(const char *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, utils::ITextParser &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		Span<const char> numberToken;
		RKIT_CHECK(parser.RequireToken(numberToken));

		switch (rtti->m_representation)
		{
		case data::RenderRTTINumberRepresentation::UnsignedInt:
			return ParseUInt(blamePath, rtti, obj, isConfigurable, numberToken, line, col);
		case data::RenderRTTINumberRepresentation::SignedInt:
			return ParseSInt(blamePath, rtti, obj, isConfigurable, numberToken, line, col);
		case data::RenderRTTINumberRepresentation::Float:
			return ParseFloat(blamePath, rtti, obj, isConfigurable, numberToken, line, col);
		default:
			return ResultCode::kInternalError;
		}
	}

	Result LibraryAnalyzer::ParseBool(const char *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const char> &token, size_t line, size_t col)
	{
		uint8_t number = 0;
		if (IsToken(token, "true"))
			number = 0;
		else if (IsToken(token, "false"))
			number = 1;
		else
		{
			rkit::log::ErrorFmt("%s [%zu:%zu] Invalid boolean value", blamePath, line, col);
			return ResultCode::kMalformedFile;
		}

		if (isConfigurable)
			rtti->m_configurableFunctions.m_writeValueUIntFunc(obj, number);
		else
			rtti->m_valueFunctions.m_writeValueUIntFunc(obj, number);

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseUInt(const char *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const char> &token, size_t line, size_t col)
	{
		if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize1)
			return ParseBool(blamePath, rtti, obj, isConfigurable, token, line, col);

		return ResultCode::kNotYetImplemented;
	}

	Result LibraryAnalyzer::ParseSInt(const char *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const char> &token, size_t line, size_t col)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result LibraryAnalyzer::ParseFloat(const char *blamePath, const data::RenderRTTINumberType *rtti, void *obj, bool isConfigurable, const Span<const char> &token, size_t line, size_t col)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result LibraryAnalyzer::ParseStruct(const char *blamePath, const data::RenderRTTIStructType *rtti, void *obj, utils::ITextParser &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		RKIT_CHECK(parser.ExpectToken("{"));

		for (;;)
		{
			Span<const char> nameToken;

			RKIT_CHECK(parser.RequireToken(nameToken));

			if (IsToken(nameToken, "}"))
				break;

			RKIT_CHECK(CheckValidIdentifier(blamePath, nameToken, parser));

			RKIT_CHECK(ParseStructMember(blamePath, nameToken, rtti, obj, parser));
		}

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::ParseStructMember(const char *blamePath, const Span<const char> &memberName, const data::RenderRTTIStructType *rtti, void *obj, utils::ITextParser &parser)
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

			const char *fieldName = field->m_name;
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
			rkit::log::ErrorFmt("%s [%zu:%zu] Invalid field", blamePath, line, col);
			return ResultCode::kMalformedFile;
		}

		const data::RenderRTTITypeBase *fieldType = resolvedField->m_getTypeFunc();

		void *fieldPtr = resolvedField->m_getMemberPtrFunc(obj);

		RKIT_CHECK(parser.ExpectToken("="));

		RKIT_CHECK(ParseValue(blamePath, resolvedField->m_getTypeFunc(), fieldPtr, resolvedField->m_isConfigurable, parser));

		return ResultCode::kOK;
	}

	template<class T>
	Result LibraryAnalyzer::ParseDynamicStructMember(const char *blamePath, const Span<const char> &memberName, const data::RenderRTTIStructType *rtti, T &obj, utils::ITextParser &parser)
	{
		return ParseStructMember(blamePath, memberName, rtti, &obj, parser);
	}

	template<size_t TSize>
	bool LibraryAnalyzer::IsToken(const Span<const char> &span, const char(&tokenChars)[TSize])
	{
		return IsTokenChars(span, tokenChars, TSize - 1);
	}

	bool LibraryAnalyzer::IsTokenChars(const Span<const char> &span, const char *tokenStr, size_t tokenLen)
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

		UniquePtr<T> newInst;
		RKIT_CHECK(New<T>(newInst, instance));

		outDeduplicated = newInst.Get();

		RKIT_CHECK(instVector.Append(std::move(newInst)));

		return ResultCode::kOK;
	}

	LibraryAnalyzer::DescriptorTypeClassification LibraryAnalyzer::ClassifyDescriptorType(render::DescriptorType descType)
	{
		switch (descType)
		{
		case render::DescriptorType::Sampler:
			return DescriptorTypeClassification::Sampler;

		case render::DescriptorType::StaticConstantBuffer:
		case render::DescriptorType::DynamicConstantBuffer:
			return DescriptorTypeClassification::ConstantBuffer;

		case render::DescriptorType::Buffer:
		case render::DescriptorType::RWBuffer:
			return DescriptorTypeClassification::Buffer;

		case render::DescriptorType::ByteAddressBuffer:
		case render::DescriptorType::RWByteAddressBuffer:
			return DescriptorTypeClassification::ByteAddressBuffer;

		case render::DescriptorType::Texture1D:
		case render::DescriptorType::Texture1DArray:
		case render::DescriptorType::Texture2D:
		case render::DescriptorType::Texture2DArray:
		case render::DescriptorType::Texture2DMS:
		case render::DescriptorType::Texture2DMSArray:
		case render::DescriptorType::Texture3D:
		case render::DescriptorType::TextureCube:
		case render::DescriptorType::TextureCubeArray:
		case render::DescriptorType::RWTexture1D:
		case render::DescriptorType::RWTexture1DArray:
		case render::DescriptorType::RWTexture2D:
		case render::DescriptorType::RWTexture2DArray:
		case render::DescriptorType::RWTexture3D:
			return DescriptorTypeClassification::Texture;

		default:
			return DescriptorTypeClassification::Unknown;
		}
	}

	void LibraryAnalyzer::RemoveTopStackItem()
	{
		m_includeStack.RemoveRange(m_includeStack.Count() - 1, 1);
	}

	Result LibraryAnalyzer::ExportPackages(IDependencyNode *depsNode) const
	{
		rkit::buildsystem::IBuildSystemDriver *bsDriver = static_cast<rkit::buildsystem::IBuildSystemDriver *>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, "BuildSystem"));

		data::IRenderDataHandler *dataHandler = m_dataDriver->GetRenderDataHandler();

		size_t pipelineIndex = 0;
		for (const render::GraphicsPipelineNameLookup &graphicsPipeline : m_graphicsPipelines)
		{
			UniquePtr<IPackageObjectWriter> writer;
			RKIT_CHECK(bsDriver->CreatePackageObjectWriter(writer));

			UniquePtr<IPackageBuilder> pkgBuilder;
			RKIT_CHECK(bsDriver->CreatePackageBuilder(dataHandler, writer.Get(), true, pkgBuilder));

			const data::RenderRTTIStructType *pipelineType = dataHandler->GetGraphicsPipelineNameLookupRTTI();

			RKIT_ASSERT(pipelineType->m_indexableType == data::RenderRTTIIndexableStructType::GraphicsPipelineNameLookup);

			pkgBuilder->BeginSource(this);

			size_t index = 0;
			RKIT_CHECK(pkgBuilder->IndexObject(&graphicsPipeline, pipelineType, true, index));

			CIPath outPath;
			RKIT_CHECK(FormatGraphicPipelinePath(outPath, depsNode->GetIdentifier(), pipelineIndex));

			{
				UniquePtr<ISeekableReadWriteStream> stream;
				RKIT_CHECK(m_feedback->OpenOutput(BuildFileLocation::kIntermediateDir, outPath, stream));

				RKIT_CHECK(pkgBuilder->WritePackage(*stream));
			}

			RKIT_CHECK(m_feedback->AddNodeDependency(IModuleDriver::kDefaultNamespace, kRenderGraphicsPipelineNodeID, BuildFileLocation::kIntermediateDir, outPath.ToString()));

			pipelineIndex++;
		}

		// Write globals
		{
			UniquePtr<IPackageObjectWriter> writer;
			RKIT_CHECK(bsDriver->CreatePackageObjectWriter(writer));

			UniquePtr<IPackageBuilder> pkgBuilder;
			RKIT_CHECK(bsDriver->CreatePackageBuilder(dataHandler, writer.Get(), false, pkgBuilder));

			const data::RenderRTTIStructType *renderPassType = dataHandler->GetRenderPassNameLookupRTTI();

			RKIT_ASSERT(renderPassType->m_indexableType == data::RenderRTTIIndexableStructType::RenderPassNameLookup);

			pkgBuilder->BeginSource(this);

			for (const render::RenderPassNameLookup &renderPass : m_renderPasses)
			{
				size_t index = 0;
				RKIT_CHECK(pkgBuilder->IndexObject(&renderPass, renderPassType, true, index));
			}

			CIPath outPath;
			RKIT_CHECK(FormatGlobalsPath(outPath, depsNode->GetIdentifier()));

			{
				UniquePtr<ISeekableReadWriteStream> stream;
				RKIT_CHECK(m_feedback->OpenOutput(BuildFileLocation::kIntermediateDir, outPath, stream));

				RKIT_CHECK(pkgBuilder->WritePackage(*stream));
			}
		}


		CIPath indexPath;
		RKIT_CHECK(FormatIndexPath(indexPath, depsNode->GetIdentifier()));

		UniquePtr<ISeekableReadWriteStream> indexStream;
		RKIT_CHECK(m_feedback->OpenOutput(BuildFileLocation::kIntermediateDir, indexPath, indexStream));

		RKIT_CHECK(indexStream->WriteAll(&rpc_common::kLibraryIndexID, sizeof(rpc_common::kLibraryIndexID)));
		RKIT_CHECK(indexStream->WriteAll(&rpc_common::kLibraryIndexVersion, sizeof(rpc_common::kLibraryIndexVersion)));

		uint64_t numGraphicsPipelines = m_graphicsPipelines.Count();
		RKIT_CHECK(indexStream->WriteAll(&numGraphicsPipelines, sizeof(numGraphicsPipelines)));

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::IndexString(const Span<const char> &span, render::GlobalStringIndex_t &outStringIndex)
	{
		String str;
		RKIT_CHECK(str.Set(span));

		size_t index = 0;
		RKIT_CHECK(m_globalStringPool.IndexString(str, index));

		outStringIndex = render::GlobalStringIndex_t(index);

		return ResultCode::kOK;
	}

	Result LibraryAnalyzer::IndexString(const Span<const char> &span, render::TempStringIndex_t &outStringIndex)
	{
		render::GlobalStringIndex_t gsi;

		RKIT_CHECK(IndexString(span, gsi));

		return IndexString(gsi, outStringIndex);
	}

	Result LibraryAnalyzer::IndexString(const render::GlobalStringIndex_t &globalIndex, render::TempStringIndex_t &outStringIndex)
	{
		HashMap<render::GlobalStringIndex_t, render::TempStringIndex_t>::ConstIterator_t it = m_globalStringToTempString.Find(globalIndex);

		if (it != m_globalStringToTempString.end())
		{
			outStringIndex = it.Value();
			return ResultCode::kOK;
		}

		render::TempStringIndex_t tsi(m_tempStrings.Count());
		RKIT_CHECK(m_tempStrings.Append(globalIndex));
		RKIT_CHECK(m_globalStringToTempString.Set(globalIndex, tsi));

		outStringIndex = tsi;

		return ResultCode::kOK;
	}
} } } // rkit::buildsystem::rpc_analyzer

namespace rkit { namespace buildsystem { namespace rpc_combiner
{
	LibraryCombiner::LibraryCombiner(data::IDataDriver *dataDriver)
		: m_dataDriver(dataDriver)
	{
	}

	Result LibraryCombiner::AddInput(IReadStream &stream)
	{
		RKIT_CHECK(CheckInitObjects());

		UniquePtr<data::IRenderDataPackage> pkg;
		Vector<Vector<uint8_t>> binaryContent;

		RKIT_CHECK(m_dataDriver->GetRenderDataHandler()->LoadPackage(stream, false, nullptr, pkg, &binaryContent));

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
					rkit::log::ErrorFmt("Duplicate graphic pipeline name '%s'", nameStrView.GetChars());
					return ResultCode::kOperationFailed;
				}
				else
				{
					String nameStr;
					RKIT_CHECK(nameStr.Set(nameStrView));

					RKIT_CHECK(m_graphicPipelineNames.Add(std::move(nameStr)));
				}

				size_t index = 0;
				RKIT_CHECK(m_pkgBuilder->IndexObject(nameLookup, m_dataDriver->GetRenderDataHandler()->GetGraphicsPipelineNameLookupRTTI(), true, index));
			}
		}

		{
			data::IRenderRTTIListBase *renderPassLookups = pkg->GetIndexable(data::RenderRTTIIndexableStructType::RenderPassNameLookup);

			size_t numRenderPassLookups = renderPassLookups->GetCount();

			for (size_t i = 0; i < numRenderPassLookups; i++)
			{
				const render::RenderPassNameLookup *nameLookup = static_cast<const render::RenderPassNameLookup *>(renderPassLookups->GetElementPtr(i));

				size_t index = 0;
				RKIT_CHECK(m_pkgBuilder->IndexObject(nameLookup, m_dataDriver->GetRenderDataHandler()->GetRenderPassNameLookupRTTI(), true, index));
			}
		}

		return ResultCode::kOK;
	}

	Result LibraryCombiner::WritePackage(ISeekableWriteStream &stream)
	{
		RKIT_CHECK(CheckInitObjects());

		return m_pkgBuilder->WritePackage(stream);
	}

	Result LibraryCombiner::CheckInitObjects()
	{
		if (!m_pkgObjectWriter.IsValid())
		{
			rkit::buildsystem::IBuildSystemDriver *bsDriver = static_cast<rkit::buildsystem::IBuildSystemDriver *>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, "BuildSystem"));
			RKIT_CHECK(bsDriver->CreatePackageObjectWriter(m_pkgObjectWriter));
			RKIT_CHECK(bsDriver->CreatePackageBuilder(m_dataDriver->GetRenderDataHandler(), m_pkgObjectWriter.Get(), false, m_pkgBuilder));
		}

		return ResultCode::kOK;
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
} } } // rkit::buildsystem::rpc_combiner

namespace rkit { namespace buildsystem { namespace rpc_compiler
{
	LibraryCompiler::LibraryCompiler(data::IDataDriver *dataDriver, IDependencyNodeCompilerFeedback *feedback)
		: m_combiner(dataDriver)
		, m_feedback(feedback)
	{
	}

	Result LibraryCompiler::Run(IDependencyNode *depsNode)
	{
		CIPath indexPath;
		RKIT_CHECK(FormatIndexPath(indexPath, depsNode->GetIdentifier()));

		size_t numGraphicsPipelines = 0;

		{
			UniquePtr<ISeekableReadStream> indexFile;
			RKIT_CHECK(m_feedback->OpenInput(BuildFileLocation::kIntermediateDir, indexPath, indexFile));

			uint32_t header[2];
			RKIT_CHECK(indexFile->ReadAll(header, sizeof(header)));

			if (header[0] != rpc_common::kLibraryIndexID || header[1] != rpc_common::kLibraryIndexVersion)
			{
				rkit::log::Error("Invalid library index header");
				return ResultCode::kOK;
			}

			uint64_t pipelineCounts[1];
			RKIT_CHECK(indexFile->ReadAll(pipelineCounts, sizeof(pipelineCounts)));

			for (uint64_t pipelineCount : pipelineCounts)
			{
				if (pipelineCount > std::numeric_limits<size_t>::max())
					return ResultCode::kOutOfMemory;
			}

			numGraphicsPipelines = static_cast<size_t>(pipelineCounts[0]);
		}

		// Add pipelines
		for (size_t i = 0; i < numGraphicsPipelines; i++)
		{
			CIPath pipelinePath;
			RKIT_CHECK(FormatGraphicPipelinePath(pipelinePath, depsNode->GetIdentifier(), i));

			CIPath compiledPipelinePath;
			RKIT_CHECK(compiledPipelinePath.Set(GetCompiledPipelineIntermediateBasePath()));
			RKIT_CHECK(compiledPipelinePath.Append(pipelinePath));

			UniquePtr<ISeekableReadStream> inStream;
			RKIT_CHECK(m_feedback->OpenInput(BuildFileLocation::kIntermediateDir, compiledPipelinePath, inStream));

			RKIT_CHECK(m_combiner.AddInput(*inStream));
		}

		// Add globals
		{
			CIPath globalsPath;
			RKIT_CHECK(FormatGlobalsPath(globalsPath, depsNode->GetIdentifier()));

			UniquePtr<ISeekableReadStream> inStream;
			RKIT_CHECK(m_feedback->OpenInput(BuildFileLocation::kIntermediateDir, globalsPath, inStream));

			RKIT_CHECK(m_combiner.AddInput(*inStream));
		}

		CIPath outPath;
		RKIT_CHECK(FormatCombinedOutputPath(outPath, depsNode->GetIdentifier()));

		UniquePtr<ISeekableReadWriteStream> outStream;
		RKIT_CHECK(m_feedback->OpenOutput(BuildFileLocation::kIntermediateDir, outPath, outStream));

		RKIT_CHECK(m_combiner.WritePackage(*outStream));

		return ResultCode::kOK;
	}
} } } // rkit::buildsystem::rpc_compiler

namespace rkit { namespace buildsystem
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
		rkit::IModule *dataModule = rkit::GetDrivers().m_moduleDriver->LoadModule(IModuleDriver::kDefaultNamespace, "Data");
		if (!dataModule)
		{
			rkit::log::Error("Couldn't load data module");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		data::IDataDriver *dataDriver = static_cast<data::IDataDriver *>(rkit::GetDrivers().FindDriver(IModuleDriver::kDefaultNamespace, "Data"));

		UniquePtr<rpc_analyzer::LibraryAnalyzer> analyzer;
		RKIT_CHECK(New<rpc_analyzer::LibraryAnalyzer>(analyzer, dataDriver, feedback));

		RKIT_CHECK(analyzer->Run(depsNode));

		RKIT_CHECK(feedback->CheckFault());

		return ResultCode::kOK;
	}

	Result RenderPipelineLibraryCompiler::RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::IModule *dataModule = rkit::GetDrivers().m_moduleDriver->LoadModule(IModuleDriver::kDefaultNamespace, "Data");
		if (!dataModule)
		{
			rkit::log::Error("Couldn't load data module");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		data::IDataDriver *dataDriver = static_cast<data::IDataDriver *>(rkit::GetDrivers().FindDriver(IModuleDriver::kDefaultNamespace, "Data"));


		UniquePtr<rpc_compiler::LibraryCompiler> compiler;
		RKIT_CHECK(New<rpc_compiler::LibraryCompiler>(compiler, dataDriver, feedback));

		RKIT_CHECK(compiler->Run(depsNode));

		RKIT_CHECK(feedback->CheckFault());

		return ResultCode::kOK;
	}

	uint32_t RenderPipelineLibraryCompiler::GetVersion() const
	{
		return 1;
	}

	Result PipelineLibraryCombinerBase::Create(UniquePtr<IPipelineLibraryCombiner> &outCombiner)
	{
		rkit::IModule *dataModule = rkit::GetDrivers().m_moduleDriver->LoadModule(IModuleDriver::kDefaultNamespace, "Data");
		if (!dataModule)
		{
			rkit::log::Error("Couldn't load data module");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		data::IDataDriver *dataDriver = static_cast<data::IDataDriver *>(rkit::GetDrivers().FindDriver(IModuleDriver::kDefaultNamespace, "Data"));

		return New<rpc_combiner::LibraryCombiner>(outCombiner, dataDriver);
	}
} } // rpc::buildsystem
