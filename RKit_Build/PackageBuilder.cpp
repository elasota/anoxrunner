#include "PackageBuilder.h"

#include "rkit/Core/HashTable.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Vector.h"

#include "rkit/Render/RenderDefs.h"

namespace rkit::buildsystem
{
	class BinaryBlob final : public IBinaryBlob
	{
	public:
		BinaryBlob();
		explicit BinaryBlob(Vector<uint8_t> &&blob);

		bool operator==(const IBinaryBlob &other) const override;
		bool operator!=(const IBinaryBlob &other) const override;

		Span<const uint8_t> GetBytes() const override;

		Result Append(const void *data, size_t size) override;

	private:
		Vector<uint8_t> m_blob;
	};

	class BinaryBlobBuilder final : public IWriteStream
	{
	public:
		BinaryBlobBuilder();

		Result WritePartial(const void *data, size_t count, size_t &outCountWritten) override;
		Result Flush() override;

		BinaryBlobRef Finish();

	private:
		UniquePtr<IBinaryBlob> m_blob;
	};

	class IndexableObjectBlobCollection : public NoCopy
	{
	public:
		IndexableObjectBlobCollection();

		Result IndexObject(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIStructType *rtti, bool cached, size_t &outIndex);
		Result IndexBinaryBlob(IPackageBuilder &pkgBuilder, BinaryBlobRef &&blob, const void *obj, bool cached, size_t &outIndex);

		void ClearObjectAddressCache();

		const Vector<const IBinaryBlob *> &GetBlobs() const;

	private:
		Vector<const IBinaryBlob *> m_blobs;
		HashMap<BinaryBlobRef, size_t> m_blobToIndex;
		HashMap<const void *, size_t> m_cachedObjectToBlob;
	};

	class PackageBuilder final : public PackageBuilderBase
	{
	public:
		explicit PackageBuilder(const data::IRenderDataHandler *dataHandler, const IPackageObjectWriter *writer, bool writeTempStrings);

		Result IndexObject(const void *obj, const data::RenderRTTIStructType *rtti, bool cached, size_t &outIndex) override;
		Result IndexString(const StringSliceView &str, size_t &outIndex) override;
		Result IndexConfigKey(size_t globalStringIndex, data::RenderRTTIMainType mainType, size_t &outIndex) override;
		Result IndexObjectList(data::RenderRTTIIndexableStructType indexableType, BinaryBlobRef &&blob, size_t &outIndex) override;
		Result IndexBinaryContent(BinaryBlobRef &&blob, size_t &outIndex) override;

		const IStringResolver *GetStringResolver() const override;
		const data::IRenderDataHandler *GetDataHandler() const override;
		const IPackageObjectWriter *GetWriter() const;
		bool IsWritingTempStrings() const override;

		void BeginSource(const IStringResolver *resolver) override;

		Result WritePackage(ISeekableWriteStream &stream) const override;

	private:
		struct ConfigKey
		{
			size_t m_globalStringIndex = 0;
			data::RenderRTTIMainType m_mainType = data::RenderRTTIMainType::Invalid;
		};

		PackageBuilder() = delete;

		static Result WriteIndexableBlobCollection(const IndexableObjectBlobCollection &indexable, IWriteStream &stream);

		static const size_t kNumIndexables = static_cast<size_t>(data::RenderRTTIIndexableStructType::Count);

		IndexableObjectBlobCollection m_indexables[kNumIndexables];
		IndexableObjectBlobCollection m_objectSpans[kNumIndexables];
		IndexableObjectBlobCollection m_binaryContent;

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
		Result WriteObject(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTITypeBase *rtti, IWriteStream &stream) const override;

		static Result WriteCompactIndex(size_t index, IWriteStream &stream);
		static Result WriteUIntForSize(uint64_t ui, uint64_t max, IWriteStream &stream);

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

	private:
		static Result StaticWriteObject(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTITypeBase *rtti, bool isConfigurable, bool isNullable, IWriteStream &stream);
		static Result WriteEnum(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIEnumType *rtti, bool isConfigurable, IWriteStream &stream);
		static Result WriteStructure(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIStructType *rtti, IWriteStream &stream);
		static Result WriteNumber(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTINumberType *rtti, bool isConfigurable, IWriteStream &stream);
		static Result WriteValueType(IPackageBuilder &pkgBuilder, const void *obj, IWriteStream &stream);
		static Result WriteBinaryContentIndex(IPackageBuilder &pkgBuilder, const void *obj, IWriteStream &stream);
		static Result WriteStringIndex(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIStringIndexType *rtti, IWriteStream &stream);
		static Result WriteObjectPtr(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIObjectPtrType *rtti, bool isNullable, IWriteStream &stream);
		static Result WriteObjectPtrSpan(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIObjectPtrSpanType *rtti, bool isNullable, IWriteStream &stream);

		static Result WriteConfigurationKey(IPackageBuilder &pkgBuilder, const render::ConfigStringIndex_t &str, data::RenderRTTIMainType mainType, IWriteStream &stream);

		static Result WritePartialLEUInt64(uint64_t ui, uint8_t numBytes, IWriteStream &stream);
	};


	BinaryBlob::BinaryBlob()
	{
	}

	BinaryBlob::BinaryBlob(Vector<uint8_t> &&blob)
		: m_blob(std::move(blob))
	{
	}

	bool BinaryBlob::operator==(const IBinaryBlob &other) const
	{
		Span<const uint8_t> otherBlob = other.GetBytes();

		if (m_blob.Count() != otherBlob.Count())
			return false;

		if (m_blob.Count() == 0)
			return true;

		return !memcmp(&m_blob[0], otherBlob.Ptr(), m_blob.Count());
	}

	bool BinaryBlob::operator!=(const IBinaryBlob &other) const
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

	IndexableObjectBlobCollection::IndexableObjectBlobCollection()
	{
	}

	Result IndexableObjectBlobCollection::IndexObject(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIStructType *rtti, bool cached, size_t &outIndex)
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

		return IndexBinaryBlob(pkgBuilder, blobBuilder.Finish(), obj, cached, outIndex);
	}

	Result IndexableObjectBlobCollection::IndexBinaryBlob(IPackageBuilder &pkgBuilder, BinaryBlobRef &&blob, const void *obj, bool cached, size_t &outIndex)
	{
		HashMap<BinaryBlobRef, size_t>::ConstIterator_t blobCheckIt = m_blobToIndex.Find(blob);
		if (blobCheckIt != m_blobToIndex.end())
		{
			size_t index = blobCheckIt.Value();
			if (cached)
			{
				RKIT_CHECK(m_cachedObjectToBlob.Set(obj, index));
			}

			outIndex = index;
			return ResultCode::kOK;
		}

		size_t newIndex = m_blobs.Count();

		RKIT_CHECK(m_blobs.Append(blob.GetBlob()));

		RKIT_CHECK(m_blobToIndex.Set(std::move(blob), newIndex));

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

	const Vector<const IBinaryBlob *> &IndexableObjectBlobCollection::GetBlobs() const
	{
		return m_blobs;
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

		outIndex = newIndex;

		return ResultCode::kOK;
	}

	Result PackageBuilder::IndexConfigKey(size_t globalStringIndex, data::RenderRTTIMainType mainType, size_t &outIndex)
	{
		HashMap<size_t, size_t>::ConstIterator_t it = m_stringIndexToConfigKeyIndex.Find(globalStringIndex);
		if (it != m_stringIndexToConfigKeyIndex.end())
		{
			if (m_configKeys[it.Value()].m_mainType != mainType)
			{
				for (HashMapKeyValueView<String, const size_t> errIt : m_stringToIndex)
				{
					if (errIt.Value() == it.Value())
					{
						rkit::log::ErrorFmt("Config key '%s' was defined as multiple conflicting types", errIt.Key().CStr());
						break;
					}
				}

				return ResultCode::kMalformedFile;
			}

			outIndex = it.Value();
			return ResultCode::kOK;
		}

		size_t newIndex = m_configKeys.Count();

		ConfigKey newKey;
		newKey.m_globalStringIndex = globalStringIndex;
		newKey.m_mainType = mainType;

		RKIT_CHECK(m_configKeys.Append(newKey));

		RKIT_CHECK(m_stringIndexToConfigKeyIndex.Set(globalStringIndex, newIndex));

		outIndex = newIndex;

		return ResultCode::kOK;
	}

	Result PackageBuilder::IndexBinaryContent(BinaryBlobRef &&blob, size_t &outIndex)
	{
		return m_binaryContent.IndexBinaryBlob(*this, std::move(blob), nullptr, false, outIndex);
	}

	Result PackageBuilder::IndexObjectList(data::RenderRTTIIndexableStructType indexableType, BinaryBlobRef &&blob, size_t &outIndex)
	{
		RKIT_ASSERT(static_cast<size_t>(indexableType) < kNumIndexables);

		return m_objectSpans[static_cast<size_t>(indexableType)].IndexBinaryBlob(*this, std::move(blob), nullptr, false, outIndex);
	}

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

	Result PackageBuilder::WriteIndexableBlobCollection(const IndexableObjectBlobCollection &indexable, IWriteStream &stream)
	{
		for (const IBinaryBlob *blob : indexable.GetBlobs())
		{
			if (blob)
			{
				Span<const uint8_t> blobBytes = blob->GetBytes();
				RKIT_CHECK(stream.WriteAll(blobBytes.Ptr(), blobBytes.Count()));
			}
		}

		return ResultCode::kOK;
	}

	Result PackageBuilder::WritePackage(ISeekableWriteStream &stream) const
	{
		uint32_t packageVersion = m_dataHandler->GetPackageVersion();

		RKIT_CHECK(PackageObjectWriter::WriteUInt32(0, stream));
		RKIT_CHECK(PackageObjectWriter::WriteUInt32(packageVersion, stream));

		size_t numStrings = m_stringToIndex.Count();
		size_t numConfigKeys = m_configKeys.Count();
		size_t numBinaryContent = m_binaryContent.GetBlobs().Count();

		Vector<const String *> strings;
		RKIT_CHECK(strings.Resize(numStrings));

		for (HashMapKeyValueView<String, const size_t> kv : m_stringToIndex)
			strings[kv.Value()] = &kv.Key();

		RKIT_CHECK(PackageObjectWriter::WriteCompactIndex(numStrings, stream));
		RKIT_CHECK(PackageObjectWriter::WriteCompactIndex(numConfigKeys, stream));
		RKIT_CHECK(PackageObjectWriter::WriteCompactIndex(numBinaryContent, stream));

		for (const String *str : strings)
		{
			RKIT_CHECK(PackageObjectWriter::WriteCompactIndex(str->Length(), stream));
		}

		for (const String *str : strings)
		{
			RKIT_CHECK(stream.WriteAll(str->CStr(), str->Length() + 1));
		}

		for (const ConfigKey &configKey : m_configKeys)
		{
			RKIT_CHECK(PackageObjectWriter::WriteCompactIndex(configKey.m_globalStringIndex, stream));
			RKIT_CHECK(PackageObjectWriter::WriteUIntForSize(static_cast<uint64_t>(configKey.m_mainType), static_cast<uint64_t>(data::RenderRTTIMainType::Count) - 1, stream));
		}

		for (const IBinaryBlob *blob : m_binaryContent.GetBlobs())
		{
			size_t contentSize = 0;
			if (blob)
				contentSize = blob->GetBytes().Count();

			RKIT_CHECK(PackageObjectWriter::WriteCompactIndex(contentSize, stream));
		}

		for (size_t i = 0; i < kNumIndexables; i++)
		{
			RKIT_CHECK(PackageObjectWriter::WriteCompactIndex(m_objectSpans[i].GetBlobs().Count(), stream));
			RKIT_CHECK(PackageObjectWriter::WriteCompactIndex(m_indexables[i].GetBlobs().Count(), stream));
		}

		for (size_t i = 0; i < kNumIndexables; i++)
		{
			RKIT_CHECK(WriteIndexableBlobCollection(m_objectSpans[i], stream));
		}

		for (size_t i = 0; i < kNumIndexables; i++)
		{
			RKIT_CHECK(WriteIndexableBlobCollection(m_indexables[i], stream));
		}

		RKIT_CHECK(WriteIndexableBlobCollection(m_binaryContent, stream));

		RKIT_CHECK(stream.SeekStart(0));
		RKIT_CHECK(PackageObjectWriter::WriteUInt32(m_dataHandler->GetPackageIdentifier(), stream));

		return ResultCode::kOK;
	}

	Result PackageObjectWriter::WriteObject(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTITypeBase *rtti, IWriteStream &stream) const
	{
		return StaticWriteObject(pkgBuilder, obj, rtti, false, false, stream);
	}

	Result PackageObjectWriter::StaticWriteObject(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTITypeBase *rtti, bool isConfigurable, bool isNullable, IWriteStream &stream)
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
		case data::RenderRTTIType::BinaryContent:
			RKIT_ASSERT(!isConfigurable);
			return WriteBinaryContentIndex(pkgBuilder, obj, stream);
		default:
			return ResultCode::kInternalError;
		}
	}

	Result PackageObjectWriter::WriteEnum(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIEnumType *rtti, bool isConfigurable, IWriteStream &stream)
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

	Result PackageObjectWriter::WriteStructure(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIStructType *rtti, IWriteStream &stream)
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

	Result PackageObjectWriter::WriteNumber(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTINumberType *rtti, bool isConfigurable, IWriteStream &stream)
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
			if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize8)
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

	Result PackageObjectWriter::WriteValueType(IPackageBuilder &pkgBuilder, const void *obj, IWriteStream &stream)
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

	Result PackageObjectWriter::WriteBinaryContentIndex(IPackageBuilder &pkgBuilder, const void *obj, IWriteStream &stream)
	{
		const render::BinaryContent *binaryContent = static_cast<const render::BinaryContent *>(obj);

		Span<const uint8_t> binaryContentData = pkgBuilder.GetStringResolver()->ResolveBinaryContent(binaryContent->m_contentIndex);

		Vector<uint8_t> bytes;
		RKIT_CHECK(bytes.Resize(binaryContentData.Count()));

		CopySpanNonOverlapping(bytes.ToSpan(), binaryContentData);

		UniquePtr<IBinaryBlob> blob;
		RKIT_CHECK(New<BinaryBlob>(blob, std::move(bytes)));

		size_t index = 0;
		RKIT_CHECK(pkgBuilder.IndexBinaryContent(BinaryBlobRef(std::move(blob)), index));
		RKIT_CHECK(WriteCompactIndex(index, stream));

		return ResultCode::kOK;
	}

	Result PackageObjectWriter::WriteStringIndex(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIStringIndexType *rtti, IWriteStream &stream)
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

	Result PackageObjectWriter::WriteObjectPtr(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIObjectPtrType *rtti, bool isNullable, IWriteStream &stream)
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

	Result PackageObjectWriter::WriteObjectPtrSpan(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTIObjectPtrSpanType *rtti, bool isNullable, IWriteStream &stream)
	{
		const void *currentElement = nullptr;
		size_t count = 0;
		size_t ptrSize = rtti->m_ptrSize;
		const data::RenderRTTIObjectPtrType *ptrType = rtti->m_getPtrTypeFunc();

		rtti->m_getFunc(obj, currentElement, count);

		BinaryBlobBuilder spanBlobBuilder;
		RKIT_CHECK(WriteCompactIndex(count, spanBlobBuilder));

		for (size_t i = 0; i < count; i++)
		{
			RKIT_CHECK(WriteObjectPtr(pkgBuilder, currentElement, ptrType, true, spanBlobBuilder));
			currentElement = static_cast<const uint8_t *>(currentElement) + ptrSize;
		}

		size_t objectListIndex = 0;
		RKIT_CHECK(pkgBuilder.IndexObjectList(ptrType->m_getTypeFunc()->m_indexableType, spanBlobBuilder.Finish(), objectListIndex));

		RKIT_CHECK(WriteCompactIndex(objectListIndex, stream));

		return ResultCode::kOK;
	}

	Result PackageObjectWriter::WriteConfigurationKey(IPackageBuilder &pkgBuilder, const render::ConfigStringIndex_t &str, data::RenderRTTIMainType mainType, IWriteStream &stream)
	{
		StringSliceView strSlice = pkgBuilder.GetStringResolver()->ResolveConfigKey(str.GetIndex());

		size_t globalStringIndex = 0;
		RKIT_CHECK(pkgBuilder.IndexString(strSlice, globalStringIndex));

		size_t configKeyIndex = 0;
		RKIT_CHECK(pkgBuilder.IndexConfigKey(globalStringIndex, mainType, configKeyIndex));

		RKIT_CHECK(WriteCompactIndex(configKeyIndex, stream));

		return ResultCode::kOK;
	}

	Result PackageObjectWriter::WriteUIntForSize(uint64_t ui, uint64_t max, IWriteStream &stream)
	{
		if (max <= 0xffu)
			return WriteUInt8(static_cast<uint8_t>(ui), stream);
		if (max <= 0xffffu)
			return WriteUInt16(static_cast<uint16_t>(ui), stream);
		if (max <= 0xffffffffu)
			return WriteUInt32(static_cast<uint32_t>(ui), stream);
		return WriteUInt64(ui, stream);
	}

	Result PackageObjectWriter::WriteCompactIndex(size_t index, IWriteStream &stream)
	{
		if (index <= 0x3fu)
			return WriteUInt8(static_cast<uint8_t>((index << 2) | 0), stream);
		if (index <= 0x3fffu)
			return WriteUInt16(static_cast<uint16_t>((index << 2) | 1), stream);
		if (index <= 0x3fffffffu)
			return WriteUInt32(static_cast<uint32_t>((index << 2) | 2), stream);
		if (index <= 0x3fffffffffffffffu)
			return WriteUInt64((static_cast<uint64_t>(index) << 2) | 3, stream);

		return ResultCode::kIntegerOverflow;
	}

	Result PackageObjectWriter::WriteUInt8(uint8_t ui, IWriteStream &stream)
	{
		return WritePartialLEUInt64(ui, 1, stream);
	}

	Result PackageObjectWriter::WriteUInt16(uint16_t ui, IWriteStream &stream)
	{
		return WritePartialLEUInt64(ui, 2, stream);
	}

	Result PackageObjectWriter::WriteUInt32(uint32_t ui, IWriteStream &stream)
	{
		return WritePartialLEUInt64(ui, 4, stream);
	}

	Result PackageObjectWriter::WriteUInt64(uint64_t ui, IWriteStream &stream)
	{
		return WritePartialLEUInt64(ui, 8, stream);
	}

	Result PackageObjectWriter::WriteSInt8(int8_t i, IWriteStream &stream)
	{
		uint8_t ui = 0;
		memcpy(&ui, &i, sizeof(i));
		return WriteUInt8(ui, stream);
	}

	Result PackageObjectWriter::WriteSInt16(int16_t i, IWriteStream &stream)
	{
		uint16_t ui = 0;
		memcpy(&ui, &i, sizeof(i));
		return WriteUInt16(ui, stream);
	}

	Result PackageObjectWriter::WriteSInt32(int32_t i, IWriteStream &stream)
	{
		uint32_t ui = 0;
		memcpy(&ui, &i, sizeof(i));
		return WriteUInt32(ui, stream);
	}

	Result PackageObjectWriter::WriteSInt64(int64_t i, IWriteStream &stream)
	{
		uint64_t ui = 0;
		memcpy(&ui, &i, sizeof(i));
		return WriteUInt64(ui, stream);
	}

	Result PackageObjectWriter::WritePartialLEUInt64(uint64_t ui, uint8_t numBytes, IWriteStream &stream)
	{
		uint8_t uiBytes[8];
		for (size_t i = 0; i < 8; i++)
			uiBytes[i] = static_cast<uint8_t>((ui >> (i * 8)) & 0xffu);

		return stream.WriteAll(uiBytes, numBytes);
	}

	Result PackageObjectWriter::WriteFloat32(float f, IWriteStream &stream)
	{
		uint32_t ui = 0;
		memcpy(&ui, &f, sizeof(f));
		return WriteUInt32(ui, stream);
	}

	Result PackageObjectWriter::WriteFloat64(double f, IWriteStream &stream)
	{
		uint64_t ui = 0;
		memcpy(&ui, &f, sizeof(f));
		return WriteUInt64(ui, stream);
	}
}

namespace rkit::buildsystem
{
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

		outCountWritten = count;

		return ResultCode::kOK;
	}

	Result BinaryBlobBuilder::Flush()
	{
		return ResultCode::kOK;
	}

	BinaryBlobRef BinaryBlobBuilder::Finish()
	{
		return BinaryBlobRef(std::move(m_blob));
	}

	Result PackageBuilderBase::Create(data::IRenderDataHandler *dataHandler, IPackageObjectWriter *objWriter, bool allowTempStrings, UniquePtr<IPackageBuilder> &outPackageBuilder)
	{
		return New<PackageBuilder>(outPackageBuilder, dataHandler, objWriter, allowTempStrings);
	}

	Result PackageObjectWriterBase::Create(UniquePtr<IPackageObjectWriter> &outPackageObjectWriter)
	{
		return New<PackageObjectWriter>(outPackageObjectWriter);
	}
}
