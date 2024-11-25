#include "RenderPipelineCompiler.h"

#include "rkit/Utilities/TextParser.h"

#include "rkit/Data/DataDriver.h"
#include "rkit/Data/RenderDataHandler.h"

#include "rkit/Core/LogDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/StringPool.h"
#include "rkit/Core/Vector.h"

#include <cstring>

namespace rkit::buildsystem::rpc_interchange
{
	enum class EntityType
	{
		StaticSampler,
		PushConstants,
		StructDef,
		InputLayout,
		GraphicsPipeline,
		DescriptorLayout,
	};

	class Entity : public NoCopy
	{
	public:
		virtual ~Entity() {}

		virtual EntityType GetEntityType() const = 0;
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
		Vector<const render::DescriptorLayoutDesc *> &GetDescriptorLayouts() { return m_descriptorLayouts; }
		Vector<const render::RenderTargetDesc *> &GetRenderTargets() { return m_renderTargets; }

		EntityType GetEntityType() const override { return EntityType::GraphicsPipeline; }

	private:
		render::GraphicsPipelineDesc m_desc;
		Vector<const render::DescriptorLayoutDesc *> m_descriptorLayouts;
		Vector<const render::RenderTargetDesc *> m_renderTargets;
	};
}

namespace rkit::buildsystem::rpc_package
{
	class PackageBuilder;

	struct IPackageObjectWriter
	{
		virtual Result WriteObject(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTITypeBase *rtti, IWriteStream &stream) const = 0;
	};

	struct IStringResolver
	{
		virtual StringSliceView ResolveGlobalString(size_t index) const = 0;
		virtual StringSliceView ResolveConfigKey(size_t index, data::RenderRTTIMainType &outMainType) const = 0;
		virtual StringSliceView ResolveTempString(size_t index) const = 0;
	};

	class BinaryBlob
	{
	public:
		BinaryBlob();
		explicit BinaryBlob(Vector<uint8_t> &&blob);

		bool operator==(const BinaryBlob &other) const;
		bool operator!=(const BinaryBlob &other) const;

		Span<const uint8_t> GetBytes() const;

		Result Append(const void *data, size_t size);

	private:
		Vector<uint8_t> m_blob;
	};

	class BinaryBlobRef : public NoCopy
	{
	public:
		BinaryBlobRef(BinaryBlobRef &&other);

		explicit BinaryBlobRef(UniquePtr<BinaryBlob> &&blob);

		Span<const uint8_t> GetBytes() const;
		const BinaryBlob *GetBlob() const;

		bool operator==(const BinaryBlobRef &other) const;
		bool operator!=(const BinaryBlobRef &other) const;

	private:
		BinaryBlobRef() = delete;

		UniquePtr<BinaryBlob> m_blob;
	};

	class BinaryBlobBuilder final : public IWriteStream
	{
	public:
		BinaryBlobBuilder();

		Result WritePartial(const void *data, size_t count, size_t &outCountWritten) override;

		BinaryBlobRef Finish();

	private:
		UniquePtr<BinaryBlob> m_blob;
	};

	class IndexableObjectBlobCollection : public NoCopy
	{
	public:
		IndexableObjectBlobCollection();

		Result IndexObject(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIStructType *rtti, bool cached, size_t &outIndex);

		void ClearObjectAddressCache();

	private:
		Vector<const BinaryBlob*> m_blobs;
		HashMap<BinaryBlobRef, size_t> m_blobToIndex;
		HashMap<const void *, size_t> m_cachedObjectToBlob;
	};

	class PackageBuilder
	{
	public:
		explicit PackageBuilder(const data::IRenderDataHandler *dataHandler, const IPackageObjectWriter *writer, bool writeTempStrings);

		Result IndexObject(const void *obj, const data::RenderRTTIStructType *rtti, bool cached, size_t &outIndex);
		Result IndexString(const StringSliceView &str, size_t &outIndex);
		Result IndexConfigKey(size_t globalStringIndex, data::RenderRTTIMainType mainType, size_t &outIndex);

		const IStringResolver *GetStringResolver() const;
		const data::IRenderDataHandler *GetDataHandler() const;
		const IPackageObjectWriter *GetWriter() const;
		bool IsWritingTempStrings() const;

		void BeginSource(const IStringResolver *resolver);

	private:
		struct ConfigKey
		{
			size_t m_globalStringIndex = 0;
			data::RenderRTTIMainType m_mainType = data::RenderRTTIMainType::Invalid;
		};

		PackageBuilder() = delete;

		static const size_t kNumIndexables = static_cast<size_t>(data::RenderRTTIIndexableStructType::Count);

		IndexableObjectBlobCollection m_indexables[kNumIndexables];

		const data::IRenderDataHandler *m_dataHandler;
		const IPackageObjectWriter *m_writer;
		const IStringResolver *m_resolver;
		bool m_writeTempStrings;

		HashMap<String, size_t> m_stringToIndex;

		HashMap<size_t, size_t> m_stringIndexToConfigKeyIndex;
		Vector<ConfigKey> m_configKeys;
	};

	class PackageObjectWriter final : public IPackageObjectWriter
	{
	public:
		Result WriteObject(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTITypeBase *rtti, IWriteStream &stream) const override;

	private:
		static Result StaticWriteObject(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTITypeBase *rtti, bool isConfigurable, bool isNullable, IWriteStream &stream);
		static Result WriteEnum(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIEnumType *rtti, bool isConfigurable, IWriteStream &stream);
		static Result WriteStructure(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIStructType *rtti, IWriteStream &stream);
		static Result WriteNumber(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTINumberType *rtti, bool isConfigurable, IWriteStream &stream);
		static Result WriteValueType(PackageBuilder &pkgBuilder, const void *obj, IWriteStream &stream);
		static Result WriteStringIndex(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIStringIndexType *rtti, IWriteStream &stream);
		static Result WriteObjectPtr(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIObjectPtrType *rtti, bool isNullable, IWriteStream &stream);
		static Result WriteObjectPtrSpan(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIObjectPtrSpanType *rtti, bool isNullable, IWriteStream &stream);

		static Result WriteConfigurationKey(PackageBuilder &pkgBuilder, const render::ConfigStringIndex_t &str, data::RenderRTTIMainType mainType, IWriteStream &stream);

		static Result WriteUIntForSize(uint64_t ui, uint64_t max, IWriteStream &stream);
		static Result WriteCompactIndex(size_t index, IWriteStream &stream);

		static Result WriteUInt8(uint8_t ui, IWriteStream &stream);
		static Result WriteUInt16(uint16_t ui, IWriteStream &stream);
		static Result WriteUInt32(uint32_t ui, IWriteStream &stream);
		static Result WriteUInt64(uint64_t ui, IWriteStream &stream);

		static Result WriteSInt8(int8_t i, IWriteStream &stream);
		static Result WriteSInt16(int16_t i, IWriteStream &stream);
		static Result WriteSInt32(int32_t i, IWriteStream &stream);
		static Result WriteSInt64(int64_t i, IWriteStream &stream);

		static Result WriteFloat32(float f, IWriteStream &stream);
		static Result WriteFloat64(double f, IWriteStream &stream);
	};
}

namespace rkit
{
	template<>
	struct Hasher<rkit::buildsystem::rpc_package::BinaryBlobRef>
		: public DefaultSpanHasher<rkit::buildsystem::rpc_package::BinaryBlobRef>
	{
		static HashValue_t ComputeHash(HashValue_t baseHash, const rkit::buildsystem::rpc_package::BinaryBlobRef &blob)
		{
			return Hasher<uint8_t>::ComputeHash(baseHash, blob.GetBytes());
		}
	};
}

namespace rkit::buildsystem::rpc_analyzer
{
	class IncludedFileKey
	{
	public:
		IncludedFileKey(BuildFileLocation location, const String &str);

		bool operator==(const IncludedFileKey &other) const;
		bool operator!=(const IncludedFileKey &other) const;

		HashValue_t ComputeHash(HashValue_t baseHash) const;

	private:
		BuildFileLocation m_location = BuildFileLocation::kInvalid;
		String m_string;
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
		AnalyzerIncludeStack(BuildFileLocation location, const String &path, bool canTryAlternate);

		String m_path;
		BuildFileLocation m_location = BuildFileLocation::kInvalid;
		bool m_canTryAlternate = false;
		bool m_isScanning = true;

		UniquePtr<rkit::utils::ITextParser> m_textParser;
		Vector<uint8_t> m_fileContents;
	};

	class Analyzer
	{
	public:
		explicit Analyzer(data::IDataDriver *dataDriver, IDependencyNodeCompilerFeedback *feedback);

		Result Run(IDependencyNode *depsNode);

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
			uint32_t m_slot = 0;
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
		Result ParseEntity(const char *blamePath, utils::ITextParser &parser, Result(Analyzer:: *parseFunc)(const char *, utils::ITextParser &, T &));

		Result ParseStaticSampler(const char *filePath, utils::ITextParser &parser, rpc_interchange::StaticSamplerEntity &ss);
		Result ParsePushConstants(const char *filePath, utils::ITextParser &parser, rpc_interchange::PushConstantsEntity &pc);
		Result ParseStructDef(const char *filePath, utils::ITextParser &parser, rpc_interchange::StructDefEntity &pc);
		Result ParseInputLayout(const char *filePath, utils::ITextParser &parser, rpc_interchange::InputLayoutEntity &il);
		Result ParseDescriptorLayout(const char *filePath, utils::ITextParser &parser, rpc_interchange::DescriptorLayoutEntity &dl);
		Result ParseGraphicsPipeline(const char *filePath, utils::ITextParser &parser, rpc_interchange::GraphicsPipelineEntity &gp);

		Result ResolveQuotedString(ShortTempToken &outToken, const Span<const char> &inToken);
		Result ResolveInputLayoutVertexInputs(const char *filePath, size_t line, size_t col, render::TempStringIndex_t feedName, const Span<const char> &nameBase, Vector<const render::InputLayoutVertexInputDesc *> &descsVector, uint32_t &inOutOffset, render::ValueType inputSourcesType, uint32_t inputFeedSlot, render::InputLayoutVertexInputStepping stepping);
		static uint32_t ResolveNumericTypeSize(render::NumericType numericType, render::VectorDimension dimension);

		Result ExpectIdentifier(const char *blamePath, Span<const char> &outToken, utils::ITextParser &parser);
		static Result CheckValidIdentifier(const char *blamePath, const Span<const char> &token, const utils::ITextParser &parser);

		Result ParseEnum(const char *blamePath, const data::RenderRTTIEnumType *rtti, void *obj, bool isConfigurable, utils::ITextParser &parser);
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

		Result ExportPackage(IDependencyNode *depsNode) const;

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
		Vector<UniquePtr<render::ShaderDesc>> m_shaderDescs;
		Vector<UniquePtr<render::DepthStencilDesc>> m_depthStencilDescs;

		Vector<const render::GraphicsPipelineDesc *> m_graphicsPipelines;

		Vector<SimpleNumericTypeResolution> m_numericTypeResolutions;

		Vector<UniquePtr<render::CompoundNumericType>> m_compoundTypes;
		Vector<UniquePtr<render::VectorNumericType>> m_vectorTypes;
	};
}

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


namespace rkit::buildsystem::rpc_package
{
	BinaryBlob::BinaryBlob()
	{
	}

	BinaryBlob::BinaryBlob(Vector<uint8_t> &&blob)
		: m_blob(std::move(blob))
	{
	}

	bool BinaryBlob::operator==(const BinaryBlob &other) const
	{
		if (m_blob.Count() == other.m_blob.Count())
			return false;

		if (m_blob.Count() == 0)
			return true;

		return !memcmp(&m_blob[0], &other.m_blob[0], m_blob.Count());
	}

	bool BinaryBlob::operator!=(const BinaryBlob &other) const
	{
		return !((*this) == other);
	}

	Span<const uint8_t> BinaryBlob::GetBytes() const
	{
		return m_blob.ToSpan();
	}

	Result BinaryBlob::Append(const void *data, size_t size)
	{
		return m_blob.Append(Span<const uint8_t>(static_cast<const uint8_t *>(data), size));
	}

	BinaryBlobRef::BinaryBlobRef(BinaryBlobRef &&other)
		: m_blob(std::move(other.m_blob))
	{
	}

	BinaryBlobRef::BinaryBlobRef(UniquePtr<BinaryBlob> &&blob)
		: m_blob(std::move(blob))
	{
	}

	Span<const uint8_t> BinaryBlobRef::GetBytes() const
	{
		const BinaryBlob *blob = m_blob.Get();
		if (!blob)
			return Span<const uint8_t>();

		return blob->GetBytes();
	}

	const BinaryBlob *BinaryBlobRef::GetBlob() const
	{
		return m_blob.Get();
	}

	bool BinaryBlobRef::operator==(const BinaryBlobRef &other) const
	{
		return (*m_blob) == *other.m_blob;
	}

	bool BinaryBlobRef::operator!=(const BinaryBlobRef &other) const
	{
		return !((*this) == other);
	}

	BinaryBlobBuilder::BinaryBlobBuilder()
	{
	}

	Result BinaryBlobBuilder::WritePartial(const void *data, size_t count, size_t &outCountWritten)
	{
		if (!m_blob.Get())
		{
			RKIT_CHECK(New<BinaryBlob>(m_blob));
		}

		RKIT_CHECK(m_blob->Append(data, count));

		return ResultCode::kOK;
	}

	BinaryBlobRef BinaryBlobBuilder::Finish()
	{
		return BinaryBlobRef(std::move(m_blob));
	}

	IndexableObjectBlobCollection::IndexableObjectBlobCollection()
	{
	}

	Result IndexableObjectBlobCollection::IndexObject(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIStructType *rtti, bool cached, size_t &outIndex)
	{
		if (cached)
		{
			HashMap<const void *, size_t>::ConstIterator_t checkIt = m_cachedObjectToBlob.Find(obj);
			if (checkIt != m_cachedObjectToBlob.end())
			{
				outIndex = checkIt.Value();
				return ResultCode::kOK;
			}
		}

		const IPackageObjectWriter *writer = pkgBuilder.GetWriter();

		BinaryBlobBuilder blobBuilder;
		RKIT_CHECK(writer->WriteObject(pkgBuilder, obj, &rtti->m_base, blobBuilder));

		BinaryBlobRef finishedBlob = blobBuilder.Finish();

		HashMap<BinaryBlobRef, size_t>::ConstIterator_t blobCheckIt = m_blobToIndex.Find(finishedBlob);
		if (blobCheckIt != m_blobToIndex.end())
		{
			size_t index = blobCheckIt.Value();
			RKIT_CHECK(m_cachedObjectToBlob.Set(obj, index));

			outIndex = blobCheckIt.Value();
			return ResultCode::kOK;
		}

		size_t newIndex = m_blobs.Count();

		RKIT_CHECK(m_blobs.Append(finishedBlob.GetBlob()));

		RKIT_CHECK(m_blobToIndex.Set(std::move(finishedBlob), newIndex));

		if (cached)
		{
			RKIT_CHECK(m_cachedObjectToBlob.Set(obj, newIndex));
		}

		outIndex = newIndex;
		return ResultCode::kOK;
	}

	void IndexableObjectBlobCollection::ClearObjectAddressCache()
	{
		m_cachedObjectToBlob.Clear();
	}

	PackageBuilder::PackageBuilder(const data::IRenderDataHandler *dataHandler, const IPackageObjectWriter *writer, bool writeTempStrings)
		: m_dataHandler(dataHandler)
		, m_writer(writer)
		, m_resolver(nullptr)
		, m_writeTempStrings(writeTempStrings)
	{
	}

	Result PackageBuilder::IndexObject(const void *obj, const data::RenderRTTIStructType *rtti, bool cached, size_t &outIndex)
	{
		RKIT_ASSERT(static_cast<size_t>(rtti->m_indexableType) < kNumIndexables);

		return m_indexables[static_cast<size_t>(rtti->m_indexableType)].IndexObject(*this, obj, rtti, cached, outIndex);
	}

	Result PackageBuilder::IndexString(const StringSliceView &str, size_t &outIndex)
	{
		HashMap<String, size_t>::ConstIterator_t it = m_stringToIndex.Find(str);
		if (it != m_stringToIndex.end())
		{
			outIndex = it.Value();
			return ResultCode::kOK;
		}

		String newStr;
		RKIT_CHECK(newStr.Set(str));

		size_t newIndex = m_stringToIndex.Count();
		RKIT_CHECK(m_stringToIndex.Set(std::move(newStr), newIndex));

		RKIT_ASSERT(m_stringToIndex.Count() == newIndex + 1);

		return ResultCode::kOK;
	}

	Result IndexConfigKey(size_t globalStringIndex, data::RenderRTTIMainType mainType, size_t &outIndex);

	const IStringResolver *PackageBuilder::GetStringResolver() const
	{
		return m_resolver;
	}

	const data::IRenderDataHandler *PackageBuilder::GetDataHandler() const
	{
		return m_dataHandler;
	}

	const IPackageObjectWriter *PackageBuilder::GetWriter() const
	{
		return m_writer;
	}

	bool PackageBuilder::IsWritingTempStrings() const
	{
		return m_writeTempStrings;
	}

	void PackageBuilder::BeginSource(const IStringResolver *resolver)
	{
		m_resolver = resolver;

		for (IndexableObjectBlobCollection &blobCollection : m_indexables)
			blobCollection.ClearObjectAddressCache();
	}

	Result PackageObjectWriter::WriteObject(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTITypeBase *rtti, IWriteStream &stream) const
	{
		return StaticWriteObject(pkgBuilder, obj, rtti, false, false, stream);
	}

	Result PackageObjectWriter::StaticWriteObject(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTITypeBase *rtti, bool isConfigurable, bool isNullable, IWriteStream &stream)
	{
		switch (rtti->m_type)
		{
		case data::RenderRTTIType::Enum:
			return WriteEnum(pkgBuilder, obj, reinterpret_cast<const data::RenderRTTIEnumType *>(rtti), isConfigurable, stream);
		case data::RenderRTTIType::Structure:
			RKIT_ASSERT(!isConfigurable);
			return WriteStructure(pkgBuilder, obj, reinterpret_cast<const data::RenderRTTIStructType *>(rtti), stream);
		case data::RenderRTTIType::Number:
			return WriteNumber(pkgBuilder, obj, reinterpret_cast<const data::RenderRTTINumberType *>(rtti), isConfigurable, stream);
		case data::RenderRTTIType::ValueType:
			RKIT_ASSERT(!isConfigurable);
			return WriteValueType(pkgBuilder, obj, stream);
		case data::RenderRTTIType::StringIndex:
			RKIT_ASSERT(!isConfigurable);
			return WriteStringIndex(pkgBuilder, obj, reinterpret_cast<const data::RenderRTTIStringIndexType *>(rtti), stream);
		case data::RenderRTTIType::ObjectPtr:
			RKIT_ASSERT(!isConfigurable);
			return WriteObjectPtr(pkgBuilder, obj, reinterpret_cast<const data::RenderRTTIObjectPtrType *>(rtti), isNullable, stream);
		case data::RenderRTTIType::ObjectPtrSpan:
			RKIT_ASSERT(!isConfigurable);
			return WriteObjectPtrSpan(pkgBuilder, obj, reinterpret_cast<const data::RenderRTTIObjectPtrSpanType *>(rtti), isNullable, stream);
		default:
			return ResultCode::kInternalError;
		}
	}

	Result PackageObjectWriter::WriteEnum(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIEnumType *rtti, bool isConfigurable, IWriteStream &stream)
	{
		if (isConfigurable)
		{
			uint8_t state = rtti->m_getConfigurableStateFunc(obj);

			RKIT_CHECK(WriteUInt8(state, stream));

			switch (state)
			{
			case static_cast<uint8_t>(render::ConfigurableValueState::Default):
				return ResultCode::kOK;
			case static_cast<uint8_t>(render::ConfigurableValueState::Configured):
				return WriteConfigurationKey(pkgBuilder, rtti->m_readConfigurableNameFunc(obj), rtti->m_base.m_mainType, stream);
			case static_cast<uint8_t>(render::ConfigurableValueState::Explicit):
				return WriteUIntForSize(rtti->m_readConfigurableValueFunc(obj), rtti->m_maxValueExclusive - 1, stream);
			default:
				return ResultCode::kInternalError;
			}
		}
		else
			return WriteUIntForSize(rtti->m_readValueFunc(obj), rtti->m_maxValueExclusive - 1, stream);
	}

	Result PackageObjectWriter::WriteStructure(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIStructType *rtti, IWriteStream &stream)
	{
		for (size_t i = 0; i < rtti->m_numFields; i++)
		{
			const data::RenderRTTIStructField *field = rtti->m_fields + i;

			void *memberPtr = field->m_getMemberPtrFunc(const_cast<void *>(obj));
			const data::RenderRTTITypeBase *fieldRTTI = field->m_getTypeFunc();

			RKIT_CHECK(StaticWriteObject(pkgBuilder, memberPtr, fieldRTTI, field->m_isConfigurable, field->m_isNullable, stream));
		}

		return ResultCode::kOK;
	}

	Result PackageObjectWriter::WriteNumber(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTINumberType *rtti, bool isConfigurable, IWriteStream &stream)
	{
		const data::RenderRTTINumberTypeIOFunctions *ioFuncs = nullptr;

		if (isConfigurable)
		{
			uint8_t state = rtti->m_getConfigurableStateFunc(obj);

			RKIT_CHECK(WriteUInt8(state, stream));

			switch (state)
			{
			case static_cast<uint8_t>(render::ConfigurableValueState::Default):
				return ResultCode::kOK;
			case static_cast<uint8_t>(render::ConfigurableValueState::Configured):
				return WriteConfigurationKey(pkgBuilder, rtti->m_readConfigurableNameFunc(obj), rtti->m_base.m_mainType, stream);
			case static_cast<uint8_t>(render::ConfigurableValueState::Explicit):
				ioFuncs = &rtti->m_configurableFunctions;
				break;
			default:
				return ResultCode::kInternalError;
			}
		}
		else
			ioFuncs = &rtti->m_valueFunctions;

		switch (rtti->m_representation)
		{
		case data::RenderRTTINumberRepresentation::Float:
		{
			double value = ioFuncs->m_readFloatFunc(obj);
			if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize32)
				return WriteFloat32(static_cast<float>(value), stream);
			else if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize64)
				return WriteFloat64(value, stream);
			else
				return ResultCode::kInternalError;
		}
		break;
		case data::RenderRTTINumberRepresentation::SignedInt:
		{
			int64_t value = ioFuncs->m_readValueSIntFunc(obj);
			if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize1 || rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize8)
				return WriteSInt8(static_cast<int8_t>(value), stream);
			else if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize16)
				return WriteSInt16(static_cast<int16_t>(value), stream);
			else if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize32)
				return WriteSInt32(static_cast<int32_t>(value), stream);
			else if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize64)
				return WriteSInt64(value, stream);
			else
				return ResultCode::kInternalError;
		}
		case data::RenderRTTINumberRepresentation::UnsignedInt:
		{
			uint64_t value = ioFuncs->m_readValueUIntFunc(obj);
			if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize1 || rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize8)
				return WriteUInt8(static_cast<uint8_t>(value), stream);
			else if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize16)
				return WriteUInt16(static_cast<uint16_t>(value), stream);
			else if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize32)
				return WriteUInt32(static_cast<uint32_t>(value), stream);
			else if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize64)
				return WriteUInt64(value, stream);
			else
				return ResultCode::kInternalError;
		}
		default:
			return ResultCode::kInternalError;
		}
	}

	Result PackageObjectWriter::WriteValueType(PackageBuilder &pkgBuilder, const void *obj, IWriteStream &stream)
	{
		const render::ValueType *valueType = static_cast<const render::ValueType *>(obj);

		RKIT_CHECK(WriteUInt8(static_cast<uint8_t>(valueType->m_type), stream));

		switch (valueType->m_type)
		{
		case render::ValueTypeType::Numeric:
			return WriteEnum(pkgBuilder, &valueType->m_value.m_numericType, pkgBuilder.GetDataHandler()->GetNumericTypeRTTI(), false, stream);
		case render::ValueTypeType::VectorNumeric:
			return WriteObjectPtr(pkgBuilder, &valueType->m_value.m_vectorNumericType, pkgBuilder.GetDataHandler()->GetVectorNumericTypePtrRTTI(), false, stream);
		case render::ValueTypeType::CompoundNumeric:
			return WriteObjectPtr(pkgBuilder, &valueType->m_value.m_compoundNumericType, pkgBuilder.GetDataHandler()->GetCompoundNumericTypePtrRTTI(), false, stream);
		case render::ValueTypeType::Structure:
			return WriteObjectPtr(pkgBuilder, &valueType->m_value.m_structureType, pkgBuilder.GetDataHandler()->GetStructureTypePtrRTTI(), false, stream);
		default:
			return ResultCode::kInternalError;
		}
	}

	Result PackageObjectWriter::WriteStringIndex(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIStringIndexType *rtti, IWriteStream &stream)
	{
		int purpose = rtti->m_getPurposeFunc();

		StringSliceView str;
		size_t index = 0;
		if (purpose == render::TempStringIndex_t::kPurpose)
		{
			if (!pkgBuilder.IsWritingTempStrings())
				return ResultCode::kOK;

			str = pkgBuilder.GetStringResolver()->ResolveTempString(rtti->m_readStringIndexFunc(obj));
		}
		else if (purpose == render::GlobalStringIndex_t::kPurpose)
			str = pkgBuilder.GetStringResolver()->ResolveGlobalString(rtti->m_readStringIndexFunc(obj));
		else
			return ResultCode::kInternalError;

		RKIT_CHECK(pkgBuilder.IndexString(str, index));
		RKIT_CHECK(WriteCompactIndex(index, stream));

		return ResultCode::kOK;
	}

	Result PackageObjectWriter::WriteObjectPtr(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIObjectPtrType *rtti, bool isNullable, IWriteStream &stream)
	{
		size_t index = 0;
		const void *objPtr = rtti->m_readFunc(obj);

		if (objPtr == nullptr)
		{
			if (!isNullable)
				return ResultCode::kInternalError;
		}
		else
		{
			RKIT_CHECK(pkgBuilder.IndexObject(objPtr, rtti->m_getTypeFunc(), true, index));

			if (isNullable)
				index++;
		}

		return WriteCompactIndex(index, stream);
	}

	Result PackageObjectWriter::WriteObjectPtrSpan(PackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIObjectPtrSpanType *rtti, bool isNullable, IWriteStream &stream)
	{
		void *currentElement = nullptr;
		size_t count = 0;
		size_t ptrSize = rtti->m_ptrSize;
		const data::RenderRTTIObjectPtrType *ptrType = rtti->m_getPtrTypeFunc();

		rtti->m_getFunc(obj, currentElement, count);

		for (size_t i = 0; i < count; i++)
		{
			RKIT_CHECK(WriteObjectPtr(pkgBuilder, currentElement, ptrType, isNullable, stream));

			currentElement = static_cast<uint8_t *>(currentElement) + ptrSize;
		}

		return ResultCode::kOK;
	}
}

namespace rkit::buildsystem::rpc_analyzer
{
	IncludedFileKey::IncludedFileKey(BuildFileLocation location, const String &str)
		: m_location(location)
		, m_string(str)
	{
	}

	HashValue_t IncludedFileKey::ComputeHash(HashValue_t baseHash) const
	{
		HashValue_t hash = baseHash;

		hash = Hasher<int32_t>::ComputeHash(hash, static_cast<int32_t>(m_location));
		hash = Hasher<String>::ComputeHash(hash, m_string);

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

	AnalyzerIncludeStack::AnalyzerIncludeStack(BuildFileLocation location, const String &path, bool canTryAlternate)
		: m_location(location)
		, m_path(path)
		, m_canTryAlternate(canTryAlternate)
	{
	}

	Analyzer::Analyzer(data::IDataDriver *dataDriver, IDependencyNodeCompilerFeedback *feedback)
		: m_feedback(feedback)
		, m_dataDriver(dataDriver)
	{
	}

	Result Analyzer::Run(IDependencyNode *depsNode)
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
			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("nuint", render::NumericType::UNorm32)));

			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("nsbyte", render::NumericType::SNorm8)));
			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("nshort", render::NumericType::SNorm16)));
			RKIT_CHECK(m_numericTypeResolutions.Append(SimpleNumericTypeResolution("nint", render::NumericType::SNorm32)));
		}

		String path;
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

		return ExportPackage(depsNode);
	}

	Result Analyzer::ScanTopStackItem(AnalyzerIncludeStack &item)
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

	Result Analyzer::ParseTopStackItem(AnalyzerIncludeStack &item)
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

	Result Analyzer::ParseDirective(const char *path, utils::ITextParser &parser, bool &outHaveDirective)
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

		Result parseResult;
		if (IsToken(directiveToken, "include"))
			parseResult = ParseIncludeDirective(path, parser);
		else if (IsToken(directiveToken, "StaticSampler"))
			parseResult = ParseEntity(path, parser, &Analyzer::ParseStaticSampler);
		else if (IsToken(directiveToken, "PushConstants"))
			parseResult = ParseEntity(path, parser, &Analyzer::ParsePushConstants);
		else if (IsToken(directiveToken, "struct"))
			parseResult = ParseEntity(path, parser, &Analyzer::ParseStructDef);
		else if (IsToken(directiveToken, "InputLayout"))
			parseResult = ParseEntity(path, parser, &Analyzer::ParseInputLayout);
		else if (IsToken(directiveToken, "DescriptorLayout"))
			parseResult = ParseEntity(path, parser, &Analyzer::ParseDescriptorLayout);
		else if (IsToken(directiveToken, "GraphicsPipeline"))
			parseResult = ParseEntity(path, parser, &Analyzer::ParseGraphicsPipeline);
		else
		{
			rkit::log::ErrorFmt("%s [%zu:%zu] Invalid directive", path, line, col);
			return ResultCode::kMalformedFile;
		}

		if (!parseResult.IsOK())
			rkit::log::ErrorFmt("%s [%zu:%zu] Directive parsing failed", path, line, col);

		return parseResult;
	}

	template<class T>
	Result Analyzer::ParseEntity(const char *blamePath, utils::ITextParser &parser, Result(Analyzer:: *parseFunc)(const char *, utils::ITextParser &, T &))
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

		T *obj = static_cast<T *>(entity.Get());

		RKIT_CHECK((this->*parseFunc)(blamePath, parser, *obj));

		RKIT_CHECK(m_entities.Set(entityName, std::move(entity)));

		return ResultCode::kOK;
	}

	Result Analyzer::ParseStaticSampler(const char *blamePath, utils::ITextParser &parser, rpc_interchange::StaticSamplerEntity &ss)
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

	Result Analyzer::ParsePushConstants(const char *blamePath, utils::ITextParser &parser, rpc_interchange::PushConstantsEntity &pc)
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

	Result Analyzer::ParseStructDef(const char *blamePath, utils::ITextParser &parser, rpc_interchange::StructDefEntity &sd)
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

	Result Analyzer::ParseInputLayout(const char *blamePath, utils::ITextParser &parser, rpc_interchange::InputLayoutEntity &il)
	{
		RKIT_CHECK(parser.ExpectToken("{"));

		Vector<VertexInputFeedMapping> feedMappings;

		bool hasNumberedMappings = false;
		bool hasSequentialMappings = false;
		for (;;)
		{
			Span<const char> token;
			RKIT_CHECK(parser.RequireToken(token));

			size_t line = 0;
			size_t col = 0;
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

					parser.GetLocation(line, col);
					RKIT_CHECK(parser.RequireToken(token));

					if (IsToken(token, "="))
					{
						if (hasSequentialMappings)
						{
							rkit::log::ErrorFmt("%s [%zu:%zu] Can't mix numbered and sequential input feed mappings", blamePath, line, col);
							return ResultCode::kMalformedFile;
						}

						hasNumberedMappings = true;

						parser.GetLocation(line, col);
						RKIT_CHECK(parser.RequireToken(token));

						uint64_t value = 0;
						RKIT_CHECK(ParseUIntConstant(blamePath, line, col, token, std::numeric_limits<uint32_t>::max(), value));

						feedMapping.m_slot = static_cast<uint32_t>(value);

						RKIT_CHECK(parser.RequireToken(token));

						for (const VertexInputFeedMapping &existingMapping : feedMappings)
						{
							if (existingMapping.m_slot == feedMapping.m_slot)
							{
								rkit::log::ErrorFmt("%s [%zu:%zu] Multiple feeds mapped to the same slot", blamePath, line, col);
								return ResultCode::kMalformedFile;
							}
						}
					}
					else
					{
						if (!IsToken(token, "}") && hasNumberedMappings)
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

						feedMapping.m_slot = slotIndex++;
					}

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

					bool haveInputFeed = 0;
					uint32_t inputFeedSlot = 0;

					render::InputLayoutVertexInputStepping stepping = render::InputLayoutVertexInputStepping::Vertex;

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

							for (const VertexInputFeedMapping &feedMapping : feedMappings)
							{
								if (CompareSpansEqual(feedMapping.m_name.ToSpan(), token))
								{
									haveInputFeed = true;
									inputFeedSlot = feedMapping.m_slot;
									break;
								}
							}

							if (!haveInputFeed)
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
						else if (IsToken(token, "Stepping"))
						{
							RKIT_CHECK(parser.ExpectToken("="));

							const data::RenderRTTIEnumType *steppingRTTI = m_dataDriver->GetRenderDataHandler()->GetInputLayoutVertexInputSteppingRTTI();

							RKIT_CHECK(ParseEnum(blamePath, steppingRTTI, &stepping, false, parser));
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
							rkit::log::ErrorFmt("%s [%zu:%zu] Too many input slots", blamePath, line, col);
							return ResultCode::kMalformedFile;
						}
					}

					if (!haveInputFeed)
					{
						rkit::log::ErrorFmt("%s [%zu:%zu] No input feeds were defined", blamePath, line, col);
						return ResultCode::kMalformedFile;
					}

					if (!haveInputSourcesType)
					{
						rkit::log::ErrorFmt("%s [%zu:%zu] No input source type was defined", blamePath, line, col);
						return ResultCode::kMalformedFile;
					}

					RKIT_CHECK(ResolveInputLayoutVertexInputs(blamePath, defStartLine, defStartCol, inputNameStr, Span<const char>(), il.GetVertexInputs(), baseOffset, inputSourcesType, inputFeedSlot, stepping));
				}
			}
			else
			{
				rkit::log::ErrorFmt("%s [%zu:%zu] Invalid entry type in InputLayout", blamePath, line, col);
				return ResultCode::kMalformedFile;
			}
		}

		il.GetDesc().m_vertexFeeds = il.GetVertexInputs().ToSpan();

		return ResultCode::kOK;
	}

	Result Analyzer::ParseDescriptorLayout(const char *filePath, utils::ITextParser &parser, rpc_interchange::DescriptorLayoutEntity &dl)
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

					HashMap<String, UniquePtr<rpc_interchange::Entity>>::ConstIterator_t it = m_entities.Find(BaseStringSliceView(token));
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

			RKIT_CHECK(m_dDescs.Append(std::move(descDesc)));
		}

		dl.GetDesc().m_descriptors = dl.GetDescriptorDescs().ToSpan();

		return ResultCode::kOK;
	}

	Result Analyzer::ParseGraphicsPipeline(const char *filePath, utils::ITextParser &parser, rpc_interchange::GraphicsPipelineEntity &gp)
	{
		RKIT_CHECK(parser.ExpectToken("{"));

		const data::RenderRTTIStructType *pipelineRTTI = m_dataDriver->GetRenderDataHandler()->GetGraphicsPipelineDescRTTI();
		const data::RenderRTTIStructType *shaderDescRTTI = m_dataDriver->GetRenderDataHandler()->GetShaderDescRTTI();
		const data::RenderRTTIStructType *depthStencilDescRTTI = m_dataDriver->GetRenderDataHandler()->GetDepthStencilDescRTTI();

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

					render::TempStringIndex_t rtName;
					RKIT_CHECK(IndexString(token, rtName));

					for (const render::RenderTargetDesc *rtDesc : gp.GetRenderTargets())
					{
						if (rtDesc->m_name == rtName)
						{
							rkit::log::ErrorFmt("%s [%zu:%zu] Render target with that name already exists", filePath, line, col);
							return ResultCode::kMalformedFile;
						}
					}

					UniquePtr<render::RenderTargetDesc> rtDesc;
					RKIT_CHECK(New<render::RenderTargetDesc>(rtDesc));

					RKIT_CHECK(parser.ExpectToken("="));

					RKIT_CHECK(gp.GetRenderTargets().Append(rtDesc.Get()));

					RKIT_CHECK(ParseStruct(filePath, m_dataDriver->GetRenderDataHandler()->GetRenderTargetDescRTTI(), rtDesc.Get(), parser));

					rtDesc->m_name = rtName;
					RKIT_CHECK(m_rtDescs.Append(std::move(rtDesc)));
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
					rkit::log::ErrorFmt("%s [%zu:%zu] Unknown type identifier", filePath, line, col);
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
				bool isVS = IsToken(token, "VertexShader");
				bool isPS = IsToken(token, "PixelShader");

				RKIT_CHECK(parser.ExpectToken("="));

				UniquePtr<render::DepthStencilDesc> depthStencil;
				RKIT_CHECK(New<render::DepthStencilDesc>(depthStencil));

				RKIT_CHECK(ParseStruct(filePath, depthStencilDescRTTI, depthStencil.Get(), parser));

				gp.GetDesc().m_depthStencil = depthStencil.Get();

				RKIT_CHECK(m_depthStencilDescs.Append(std::move(depthStencil)));
			}
			else
			{
				RKIT_CHECK(ParseStructMember(filePath, token, pipelineRTTI, &gp.GetDesc(), parser));
			}
		}

		gp.GetDesc().m_descriptorLayouts = gp.GetDescriptorLayouts().ToSpan();
		gp.GetDesc().m_renderTargets = gp.GetRenderTargets().ToSpan();

		RKIT_CHECK(m_graphicsPipelines.Append(&gp.GetDesc()));

		return ResultCode::kOK;
	}

	Result Analyzer::ParseIncludeDirective(const char *blamePath, utils::ITextParser &parser)
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
		if (!utils->ValidateFilePath(path.GetChars()))
		{
			rkit::log::ErrorFmt("%s [%zu:%zu] Invalid file path", blamePath, line, col);
			return ResultCode::kMalformedFile;
		}

		BuildFileLocation loc = m_includeStack[m_includeStack.Count() - 1].m_location;

		String str;
		RKIT_CHECK(str.Set(path.GetChars()));

		RKIT_CHECK(m_includeStack.Append(AnalyzerIncludeStack(loc, str, true)));

		return ResultCode::kOK;
	}

	Result Analyzer::ResolveQuotedString(ShortTempToken &outToken, const Span<const char> &inToken)
	{
		RKIT_CHECK(outToken.Set(inToken));

		size_t newLength = 0;
		RKIT_CHECK(GetDrivers().m_utilitiesDriver->EscapeCStringInPlace(outToken.GetChars(), newLength));

		outToken.Truncate(newLength);

		return ResultCode::kOK;
	}

	Result Analyzer::ResolveInputLayoutVertexInputs(const char *blamePath, size_t line, size_t col, render::TempStringIndex_t feedName, const Span<const char> &nameBase, Vector<const render::InputLayoutVertexInputDesc *> &descsVector, uint32_t &inOutOffset, render::ValueType inputSourcesType, uint32_t inputFeedSlot, render::InputLayoutVertexInputStepping stepping)
	{
		switch (inputSourcesType.m_type)
		{
		case render::ValueTypeType::CompoundNumeric:
			rkit::log::ErrorFmt("%s [%zu:%zu] Matrix types aren't allowed in vertex inputs", blamePath, line, col);
			return ResultCode::kMalformedFile;

		case render::ValueTypeType::VectorNumeric:
			{
				const render::VectorNumericType *t = inputSourcesType.m_value.m_vectorNumericType;
				uint32_t sz = ResolveNumericTypeSize(t->m_numericType, t->m_cols);

				render::TempStringIndex_t memberName;
				if (nameBase.Count() == 0)
				{
					RKIT_CHECK(IndexString(StringView("Value").ToSpan(), memberName));
				}
				else
				{
					RKIT_CHECK(IndexString(nameBase, memberName));
				}


				RKIT_CHECK(IndexString(nameBase, memberName));

				UniquePtr<render::InputLayoutVertexInputDesc> vid;
				RKIT_CHECK(New<render::InputLayoutVertexInputDesc>(vid));
				vid->m_byteOffset = inOutOffset;
				vid->m_feedName = feedName;
				vid->m_inputSlot = inputFeedSlot;
				vid->m_memberName = memberName;
				vid->m_numericType = t;
				vid->m_stepping = stepping;

				RKIT_CHECK(descsVector.Append(vid.Get()));

				RKIT_CHECK(m_vertexInputDescs.Append(std::move(vid)));

				inOutOffset += sz;
			}
			break;

		case render::ValueTypeType::Numeric:
			{
				const render::VectorNumericType vt;

				const render::VectorNumericType *deduplicated = nullptr;
				RKIT_CHECK(Deduplicate(m_vectorTypes, vt, deduplicated));

				render::ValueType valueType(deduplicated);

				RKIT_CHECK(ResolveInputLayoutVertexInputs(blamePath, line, col, feedName, nameBase, descsVector, inOutOffset, valueType, inputFeedSlot, stepping));
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

						RKIT_CHECK(ResolveInputLayoutVertexInputs(blamePath, line, col, feedName, memberNameBaseChars.ToSpan(), descsVector, inOutOffset, memberType, inputFeedSlot, stepping));
					}
					else
					{
						RKIT_CHECK(ResolveInputLayoutVertexInputs(blamePath, line, col, feedName, memberName.ToSpan(), descsVector, inOutOffset, memberType, inputFeedSlot, stepping));
					}
				}
			}
			break;

		default:
			return ResultCode::kMalformedFile;
		}

		return ResultCode::kOK;
	}

	uint32_t Analyzer::ResolveNumericTypeSize(render::NumericType numericType, render::VectorDimension dimension)
	{
		uint8_t dimensionInt = static_cast<uint8_t>(dimension) - static_cast<uint8_t>(render::VectorDimension::Dimension1) + 1;

		switch (numericType)
		{
		case render::NumericType::SInt8:
		case render::NumericType::UInt8:
		case render::NumericType::SNorm8:
		case render::NumericType::UNorm8:
			return dimensionInt;

		case render::NumericType::Float16:
		case render::NumericType::SInt16:
		case render::NumericType::UInt16:
		case render::NumericType::SNorm16:
		case render::NumericType::UNorm16:
			return dimensionInt * 2;

		case render::NumericType::Float32:
		case render::NumericType::SInt32:
		case render::NumericType::UInt32:
		case render::NumericType::SNorm32:
		case render::NumericType::UNorm32:
			return dimensionInt * 4;

		case render::NumericType::Float64:
		case render::NumericType::SInt64:
		case render::NumericType::UInt64:
			return dimensionInt * 8;

		case render::NumericType::Bool:
			return 1;

		default:
			RKIT_ASSERT(false);
			return 0;
		}
	}


	Result Analyzer::ExpectIdentifier(const char *blamePath, Span<const char> &outToken, utils::ITextParser &parser)
	{
		RKIT_CHECK(parser.RequireToken(outToken));

		return CheckValidIdentifier(blamePath, outToken, parser);
	}

	Result Analyzer::CheckValidIdentifier(const char *blamePath, const Span<const char> &token, const utils::ITextParser &parser)
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

	Result Analyzer::ParseValue(const char *blamePath, const data::RenderRTTITypeBase *rtti, void *obj, bool isConfigurable, utils::ITextParser &parser)
	{
		switch (rtti->m_type)
		{
		case data::RenderRTTIType::Enum:
			return ParseEnum(blamePath, reinterpret_cast<const data::RenderRTTIEnumType *>(rtti), obj, isConfigurable, parser);
		case data::RenderRTTIType::Number:
			return ResultCode::kNotYetImplemented;
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

	Result Analyzer::ParseValueType(const char *blamePath, render::ValueType &valueType, const Span<const char> &token, utils::ITextParser &parser)
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
				if (c0 >= '1' && c0 <= '4')
				{
					render::VectorNumericType vectorType;
					vectorType.m_cols = static_cast<render::VectorDimension>(static_cast<int>(render::VectorDimension::Dimension1) + (c0 - '1'));
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
				if ((c0 >= '1' && c0 <= '4') && c1 == 'x' && (c2 >= '1' && c2 <= '4'))
				{
					render::CompoundNumericType compoundType;
					compoundType.m_cols = static_cast<render::VectorDimension>(static_cast<int>(render::VectorDimension::Dimension1) + (c0 - '1'));
					compoundType.m_rows = static_cast<render::VectorDimension>(static_cast<int>(render::VectorDimension::Dimension1) + (c2 - '1'));
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

	Result Analyzer::ParseConfigurable(const char *blamePath, void *obj, data::RenderRTTIMainType mainType, void (*writeNameFunc)(void *, const render::ConfigStringIndex_t &), utils::ITextParser &parser)
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

	Result Analyzer::ParseUIntConstant(const char *blamePath, size_t blameLine, size_t blameCol, const Span<const char> &token, uint64_t max, uint64_t &outValue)
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

	Result Analyzer::ParseStringIndex(const char *blamePath, const data::RenderRTTIStringIndexType *rtti, void *obj, utils::ITextParser &parser)
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

	Result Analyzer::ParseEnum(const char *blamePath, const data::RenderRTTIEnumType *rtti, void *obj, bool isConfigurable, utils::ITextParser &parser)
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

	Result Analyzer::ParseStruct(const char *blamePath, const data::RenderRTTIStructType *rtti, void *obj, utils::ITextParser &parser)
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

	Result Analyzer::ParseStructMember(const char *blamePath, const Span<const char> &memberName, const data::RenderRTTIStructType *rtti, void *obj, utils::ITextParser &parser)
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
	Result Analyzer::ParseDynamicStructMember(const char *blamePath, const Span<const char> &memberName, const data::RenderRTTIStructType *rtti, T &obj, utils::ITextParser &parser)
	{
		return ParseStructMember(blamePath, memberName, rtti, &obj, parser);
	}

	template<size_t TSize>
	bool Analyzer::IsToken(const Span<const char> &span, const char(&tokenChars)[TSize])
	{
		return IsTokenChars(span, tokenChars, TSize - 1);
	}

	bool Analyzer::IsTokenChars(const Span<const char> &span, const char *tokenStr, size_t tokenLen)
	{
		if (span.Count() != tokenLen)
			return false;

		return !memcmp(span.Ptr(), tokenStr, tokenLen);
	}

	template<class T>
	Result Analyzer::Deduplicate(Vector<UniquePtr<T>> &instVector, const T &instance, const T *&outDeduplicated)
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

	Analyzer::DescriptorTypeClassification Analyzer::ClassifyDescriptorType(render::DescriptorType descType)
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

	void Analyzer::RemoveTopStackItem()
	{
		m_includeStack.RemoveRange(m_includeStack.Count() - 1, 1);
	}

	Result Analyzer::ExportPackage(IDependencyNode *depsNode) const
	{
		data::IRenderDataHandler *dataHandler = m_dataDriver->GetRenderDataHandler();

		rpc_package::PackageObjectWriter writer;

		rpc_package::PackageBuilder pkgBuilder(dataHandler, &writer, true);

		const data::RenderRTTIStructType *pipelineType = dataHandler->GetGraphicsPipelineDescRTTI();

		RKIT_ASSERT(pipelineType->m_indexableType == data::RenderRTTIIndexableStructType::GraphicsPipelineDesc);

		for (const render::GraphicsPipelineDesc *graphicsPipeline : m_graphicsPipelines)
		{
			size_t index = 0;
			RKIT_CHECK(pkgBuilder.IndexObject(graphicsPipeline, pipelineType, true, index));
		}

		return ResultCode::kNotYetImplemented;
	}

	Result Analyzer::IndexString(const Span<const char> &span, render::GlobalStringIndex_t &outStringIndex)
	{
		String str;
		RKIT_CHECK(str.Set(span));

		size_t index = 0;
		RKIT_CHECK(m_globalStringPool.IndexString(str, index));

		outStringIndex = render::GlobalStringIndex_t(index);

		return ResultCode::kOK;
	}

	Result Analyzer::IndexString(const Span<const char> &span, render::TempStringIndex_t &outStringIndex)
	{
		render::GlobalStringIndex_t gsi;

		RKIT_CHECK(IndexString(span, gsi));

		return IndexString(gsi, outStringIndex);
	}

	Result Analyzer::IndexString(const render::GlobalStringIndex_t &globalIndex, render::TempStringIndex_t &outStringIndex)
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
}

namespace rkit::buildsystem
{
	RenderPipelineCompiler::RenderPipelineCompiler()
	{
	}

	bool RenderPipelineCompiler::HasAnalysisStage() const
	{
		return true;
	}

	Result RenderPipelineCompiler::RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::IModule *dataModule = rkit::GetDrivers().m_moduleDriver->LoadModule(IModuleDriver::kDefaultNamespace, "Data");
		if (!dataModule)
		{
			rkit::log::Error("Couldn't load utilities module");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		RKIT_CHECK(dataModule->Init(nullptr));

		data::IDataDriver *dataDriver = static_cast<data::IDataDriver *>(rkit::GetDrivers().FindDriver(IModuleDriver::kDefaultNamespace, "Data"));

		UniquePtr<rpc_analyzer::Analyzer> analyzer;
		RKIT_CHECK(New<rpc_analyzer::Analyzer>(analyzer, dataDriver, feedback));

		return analyzer->Run(depsNode);
	}

	Result RenderPipelineCompiler::RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result RenderPipelineCompiler::CreatePrivateData(UniquePtr<IDependencyNodePrivateData> &outPrivateData)
	{
		return ResultCode::kOK;
	}

	uint32_t RenderPipelineCompiler::GetVersion() const
	{
		return 1;
	}
}
