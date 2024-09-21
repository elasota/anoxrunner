#include "rkit/Core/MallocDriver.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/RKitAssert.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/UniquePtr.h"

#include "rkit/Utilities/Json.h"

#include "Json.h"

#include "thirdparty/rapidjson/include/rapidjson/document.h"

namespace rkit::Utilities
{
	class JsonDocument final : public IJsonDocument
	{
	public:
		explicit JsonDocument(IMallocDriver *alloc, IReadStream *readStream);

		Result ToJsonValue(JsonValue &outValue) override;

	private:
		class PrivAllocator
		{
		public:
			static const bool kNeedFree = true;
			PrivAllocator();
			explicit PrivAllocator(IMallocDriver *alloc);

			void *Malloc(size_t size);
			void *Realloc(void *ptr, size_t oldSize, size_t newSize);
			void Free(void *ptr);

		private:
			IMallocDriver *m_alloc;
		};

		class PrivInputByteStream final : public NoCopy
		{
		public:
			typedef char Ch;

			explicit PrivInputByteStream(IReadStream *readStream);

			Ch Peek() const;
			Ch Take();
			size_t Tell() const;

			const Ch *Peek4() const;

		private:
			static const size_t kPreloadBufferSize = 1024;
			static const size_t kPreloadMinimumSize = 4;

			void Preload();

			IReadStream *m_readStream;
			char m_preloadBuffer[kPreloadBufferSize];
			size_t m_preloadedStart;
			size_t m_preloadedEnd;
			size_t m_preloadPosInFile;
			bool m_isEOF;
			Result m_errorResult;
		};

		typedef rapidjson::EncodedInputStream<rapidjson::UTF8<>, PrivInputByteStream> EncodedInputStream_t;
		typedef rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::MemoryPoolAllocator<PrivAllocator>, PrivAllocator> Document_t;
		typedef Document_t::GenericValue GenericValue_t;

		Result CheckInit();

		static void GenericValueToJsonValue(const GenericValue_t &genericValue, JsonValue &outJsonValue);

		static const JsonValueVFTable kJsonValueVFTable;

		static Result VFToBool(const void *jsonValue, bool &outBool);
		static Result VFToString(const void *jsonValue, const char *&outCharPtr, size_t &outLength);
		static Result VFToNumber(const void *jsonValue, double &outNumber);
		static Result VFGetArraySize(const void *jsonValue, size_t &outSize);
		static Result VFGetArrayElement(const void *jsonValue, size_t index, JsonValue &outJsonValue);
		static Result VFIterateObject(const void *jsonValue, void *userdata, JsonObjectIteratorCallback_t callback);
		static Result VFObjectHasElement(const void *jsonValue, const char *keyChars, size_t keyLength, bool &outHasElement);
		static Result VFGetObjectElement(const void *jsonValue, const char *keyChars, size_t keyLength, JsonValue &outJsonValue);

		PrivAllocator m_allocator;
		rapidjson::MemoryPoolAllocator<PrivAllocator> m_poolAllocator;
		Document_t m_document;
		PrivInputByteStream m_inputByteStream;
		EncodedInputStream_t m_encodedInputStream;

		bool m_haveStarted;
	};

	JsonDocument::PrivAllocator::PrivAllocator()
		: m_alloc(nullptr)
	{
		// This should never be called but rapidjson requires it to be public
		RKIT_ASSERT(false);
	}

	JsonDocument::PrivAllocator::PrivAllocator(IMallocDriver *alloc)
		: m_alloc(alloc)
	{
	}

	void *JsonDocument::PrivAllocator::Malloc(size_t size)
	{
		return m_alloc->Alloc(size);
	}

	void *JsonDocument::PrivAllocator::Realloc(void *ptr, size_t oldSize, size_t newSize)
	{
		return m_alloc->Realloc(ptr, newSize);
	}

	void JsonDocument::PrivAllocator::Free(void *ptr)
	{
		return m_alloc->Free(ptr);
	}

	JsonDocument::PrivInputByteStream::PrivInputByteStream(IReadStream *readStream)
		: m_readStream(readStream)
		, m_preloadedStart(0)
		, m_preloadedEnd(0)
		, m_preloadPosInFile(0)
		, m_isEOF(false)
	{
		Preload();
	}

	JsonDocument::PrivInputByteStream::Ch JsonDocument::PrivInputByteStream::Peek() const
	{
		if (m_preloadedEnd == m_preloadedStart)
			return Ch(0);

		return m_preloadBuffer[m_preloadedStart];
	}

	JsonDocument::PrivInputByteStream::Ch JsonDocument::PrivInputByteStream::Take()
	{
		if (m_preloadedEnd == m_preloadedStart)
			return 0;

		Ch result = m_preloadBuffer[m_preloadedStart++];

		if (m_preloadedEnd - m_preloadedStart < kPreloadMinimumSize)
			Preload();

		return result;
	}

	size_t JsonDocument::PrivInputByteStream::Tell() const
	{
		return m_preloadPosInFile + m_preloadedStart;
	}

	const JsonDocument::PrivInputByteStream::Ch *JsonDocument::PrivInputByteStream::Peek4() const
	{
		if (m_preloadedEnd - m_preloadedStart < 4)
			return nullptr;

		return m_preloadBuffer + m_preloadedStart;
	}

	void JsonDocument::PrivInputByteStream::Preload()
	{
		if (m_isEOF)
			return;

		if (m_preloadedStart > 0)
		{
			size_t preloadedSize = m_preloadedEnd - m_preloadedStart;

			if (preloadedSize > 0)
			{
				memmove(m_preloadBuffer, m_preloadBuffer + m_preloadedStart, preloadedSize);
				m_preloadPosInFile += m_preloadedStart;
			}

			m_preloadedEnd -= m_preloadedStart;
			m_preloadedStart = 0;
		}

		size_t preloadCapacity = kPreloadBufferSize - m_preloadedEnd;
		size_t amountPreloaded = 0;

		Result result = m_readStream->Read(m_preloadBuffer + m_preloadedStart, preloadCapacity, amountPreloaded);
		if (!result.IsOK())
		{
			m_isEOF = true;
			if (result.GetResultCode() == ResultCode::kEndOfStream)
				m_errorResult = result;
		}

		m_preloadedEnd += amountPreloaded;
	}

	JsonDocument::JsonDocument(IMallocDriver *alloc, IReadStream *readStream)
		: m_allocator(alloc)
		, m_poolAllocator(65536, &m_allocator)
		, m_document(&m_poolAllocator, 1024, &m_allocator)
		, m_inputByteStream(readStream)
		, m_encodedInputStream(m_inputByteStream)
		, m_haveStarted(false)
	{
	}

	Result JsonDocument::ToJsonValue(JsonValue &outValue)
	{
		RKIT_CHECK(CheckInit());

		GenericValue_t &docValue = m_document;
		GenericValueToJsonValue(docValue, outValue);

		return ResultCode::kOK;
	}

	Result JsonDocument::CheckInit()
	{
		if (!m_haveStarted)
		{
			m_haveStarted = true;
			m_document.ParseStream<rapidjson::kParseValidateEncodingFlag, rapidjson::UTF8<>, EncodedInputStream_t>(m_encodedInputStream);
			if (m_document.HasParseError())
				return ResultCode::kInvalidJson;
		}

		return ResultCode::kOK;
	}

	void JsonDocument::GenericValueToJsonValue(const GenericValue_t &genericValue, JsonValue &outJsonValue)
	{
		JsonElementType type = JsonElementType::kNull;
		const void *jsonValuePtr = &genericValue;
		const JsonValueVFTable *vptr = &kJsonValueVFTable;

		switch (genericValue.GetType())
		{
		case rapidjson::kTrueType:
		case rapidjson::kFalseType:
			type = JsonElementType::kBool;
			break;
		case rapidjson::kObjectType:
			type = JsonElementType::kObject;
			break;
		case rapidjson::kArrayType:
			type = JsonElementType::kArray;
			break;
		case rapidjson::kStringType:
			type = JsonElementType::kString;
			break;
		case rapidjson::kNumberType:
			type = JsonElementType::kNumber;
			break;
		default:
			type = JsonElementType::kNull;
			break;
		}

		outJsonValue = JsonValue(type, vptr, jsonValuePtr);
	}

	Result JsonDocument::VFToBool(const void *jsonValue, bool &outBool)
	{
		const GenericValue_t *gValue = static_cast<const GenericValue_t *>(jsonValue);

		if (gValue->GetType() == rapidjson::kTrueType)
			outBool = true;
		else if (gValue->GetType() == rapidjson::kFalseType)
			outBool = false;
		else
		{
			outBool = false;
			return ResultCode::kInvalidParameter;
		}

		return ResultCode::kOK;
	}

	Result JsonDocument::VFToString(const void *jsonValue, const char *&outCharPtr, size_t &outLength)
	{
		const GenericValue_t *gValue = static_cast<const GenericValue_t *>(jsonValue);

		if (gValue->GetType() != rapidjson::kStringType)
			return ResultCode::kInvalidParameter;

		outCharPtr = gValue->GetString();
		outLength = gValue->GetStringLength();

		return ResultCode::kOK;
	}

	Result JsonDocument::VFToNumber(const void *jsonValue, double &outNumber)
	{
		const GenericValue_t *gValue = static_cast<const GenericValue_t *>(jsonValue);

		if (gValue->GetType() != rapidjson::kNumberType)
			return ResultCode::kInvalidParameter;

		outNumber = gValue->GetDouble();

		return ResultCode::kOK;
	}

	Result JsonDocument::VFGetArraySize(const void *jsonValue, size_t &outSize)
	{
		const GenericValue_t *gValue = static_cast<const GenericValue_t *>(jsonValue);

		if (gValue->GetType() != rapidjson::kArrayType)
			return ResultCode::kInvalidParameter;

		outSize = gValue->Size();

		return ResultCode::kOK;
	}

	Result JsonDocument::VFGetArrayElement(const void *jsonValue, size_t index, JsonValue &outJsonValue)
	{
		const GenericValue_t *gValue = static_cast<const GenericValue_t *>(jsonValue);

		if (gValue->GetType() != rapidjson::kArrayType)
			return ResultCode::kInvalidParameter;

		RKIT_ASSERT(index < gValue->Size());
		const GenericValue_t &arrayElement = (*gValue)[static_cast<rapidjson::SizeType>(index)];

		GenericValueToJsonValue(arrayElement, outJsonValue);

		return ResultCode::kOK;
	}

	Result JsonDocument::VFIterateObject(const void *jsonValue, void *userdata, JsonObjectIteratorCallback_t callback)
	{
		typedef Document_t::ConstMemberIterator ConstMemberIterator_t;
		const GenericValue_t *gValue = static_cast<const GenericValue_t *>(jsonValue);

		if (gValue->GetType() != rapidjson::kObjectType)
			return ResultCode::kInvalidParameter;

		for (ConstMemberIterator_t it = gValue->MemberBegin(), itEnd = gValue->MemberEnd(); it != itEnd; ++it)
		{
			bool shouldContinue = true;
			JsonValue jsonValue;
			GenericValueToJsonValue(it->value, jsonValue);
			RKIT_CHECK(callback(userdata, it->name.GetString(), it->name.GetStringLength(), jsonValue, shouldContinue));

			if (!shouldContinue)
				break;
		}

		return ResultCode::kOK;
	}

	Result JsonDocument::VFObjectHasElement(const void *jsonValue, const char *keyChars, size_t keyLength, bool &outHasElement)
	{
		const GenericValue_t *gValue = static_cast<const GenericValue_t *>(jsonValue);

		if (gValue->GetType() != rapidjson::kObjectType)
			return ResultCode::kInvalidParameter;

		outHasElement = gValue->HasMember(GenericValue_t(keyChars, static_cast<rapidjson::SizeType>(keyLength)));

		return ResultCode::kOK;
	}

	Result JsonDocument::VFGetObjectElement(const void *jsonValue, const char *keyChars, size_t keyLength, JsonValue &outJsonValue)
	{
		typedef Document_t::ConstMemberIterator ConstMemberIterator_t;
		const GenericValue_t *gValue = static_cast<const GenericValue_t *>(jsonValue);

		if (gValue->GetType() != rapidjson::kObjectType)
			return ResultCode::kInvalidParameter;

		Document_t::ConstMemberIterator it = gValue->FindMember(GenericValue_t(keyChars, static_cast<rapidjson::SizeType>(keyLength)));
		if (it == gValue->MemberEnd())
			return ResultCode::kKeyNotFound;

		GenericValueToJsonValue(it->value, outJsonValue);

		return ResultCode::kOK;
	}

	const JsonValueVFTable JsonDocument::kJsonValueVFTable =
	{
		JsonDocument::VFToBool,
		JsonDocument::VFToString,
		JsonDocument::VFToNumber,
		JsonDocument::VFGetArraySize,
		JsonDocument::VFGetArrayElement,
		JsonDocument::VFIterateObject,
		JsonDocument::VFObjectHasElement,
		JsonDocument::VFGetObjectElement,
	};
}


rkit::Result rkit::Utilities::CreateJsonDocument(UniquePtr<IJsonDocument> &outDocument, IMallocDriver *alloc, IReadStream *readStream)
{
	return NewWithAlloc<JsonDocument>(outDocument, alloc, alloc, readStream);
}
