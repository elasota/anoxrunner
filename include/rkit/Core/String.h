#pragma once

#include "Algorithm.h"
#include "Atomic.h"
#include "NoCopy.h"
#include "StringProto.h"

#include "FormatProtos.h"

#include <cstdint>
#include <cstddef>

namespace rkit
{
	template<class TChar>
	struct IFormatStringWriter;
}

namespace rkit { namespace priv {
	template<class TChar, CharacterEncoding TEncoding>
	struct StringFormatHelper : IFormatStringWriter<TChar>
	{
		Span<TChar> m_charsBuffer;
		size_t m_charsRequired = 0;
		bool m_overflowed = false;

		void WriteChars(const Span<const TChar> &chars) override;
	};
} }

namespace rkit
{
	struct IMallocDriver;

	template<class T>
	class Span;

	template<class TChar>
	class StringStorage final : public NoCopy
	{
	public:
		explicit StringStorage(IMallocDriver *alloc);

		void IncRef();
		void DecRef();

		TChar *GetFirstCharAddress() const;
		static Result ComputeSize(size_t sizeInChars, size_t &outSizeInBytes);

		static StringStorage<TChar> *LocateFromFirstCharAddress(const TChar *chars);

	private:
		~StringStorage();

		static size_t ComputePaddedBaseSize();

		typedef AtomicUInt32_t CounterType_t;
		typedef AtomicUInt32_t::ValueType_t CounterValueType_t;

		CounterType_t m_counter;
		IMallocDriver *m_alloc;
	};

	template<class TChar>
	class BaseStringConstructionBuffer
	{
	public:
		BaseStringConstructionBuffer();
		~BaseStringConstructionBuffer();

		Result Allocate(size_t numChars);
		Result Allocate(size_t numChars, IMallocDriver *alloc);

		Span<TChar> GetSpan() const;

		void Detach();

	private:
		StringStorage<TChar> *m_stringStorage;
		size_t m_numChars;
	};

	template<class TChar, CharacterEncoding TEncoding, size_t TStaticSize>
	class BaseString
		: public rkit::CompareWithOrderingOperatorsMixin<BaseString<TChar, TEncoding, TStaticSize>, BaseStringSliceViewComparer<TChar, TEncoding>>
	{
	public:
		typedef BaseStringView<TChar, TEncoding> View_t;
		typedef BaseStringSliceView<TChar, TEncoding> SliceView_t;

		BaseString();
		BaseString(const BaseString &other);
		BaseString(BaseString &&other) noexcept;
		BaseString(BaseStringConstructionBuffer<TChar> &&other) noexcept;
		~BaseString();

		BaseString &operator=(const BaseString &other);
		BaseString &operator=(BaseString &&other) noexcept;
		BaseString &operator=(BaseStringConstructionBuffer<TChar> &&other) noexcept;

		Result Set(const SliceView_t &strView);
		Result Set(const Span<const TChar> &strView);
		Result Append(const SliceView_t &strView);
		Result Append(const Span<const TChar> &span);
		Result Append(TChar ch);

		template<class TAdjustFunc>
		Result ChangeCase(const TAdjustFunc &adjuster);

		Result MakeLower();
		Result MakeUpper();

		bool StartsWith(const SliceView_t &strView) const;
		bool StartsWith(const Span<const TChar> &span) const;
		bool StartsWith(TChar ch) const;

		bool EndsWith(const SliceView_t &strView) const;
		bool EndsWith(const Span<const TChar> &span) const;
		bool EndsWith(TChar ch) const;

		operator View_t() const;
		operator SliceView_t() const;

		const TChar &operator[](size_t index) const;

		void Clear();

		SliceView_t SubString(size_t start, size_t length) const;
		Span<const TChar> ToSpan() const;

		const TChar *CStr() const;
		size_t Length() const;

		template<class... TArgs>
		Result Format(const BaseStringSliceView<TChar, TEncoding> &fmt, const TArgs &...args);

		Result VFormat(const BaseStringSliceView<TChar, TEncoding> &fmt, const FormatParameterList<TChar> &formatParams);

		Ordering Compare(const BaseStringSliceView<TChar, TEncoding> &other) const;

		void FormatValue(IFormatStringWriter<TChar> &writer) const;

	private:
		bool IsStaticString() const;
		void CopyStaticStringFromOther(const BaseString &other);
		void Evict();
		void UnsafeReset();

		static Result CreateAndReturnUninitializedSpan(BaseString &outStr, size_t numChars, Span<TChar> &outSpan);

		const TChar *m_chars;
		size_t m_length;

		TChar m_staticString[TStaticSize];
	};
}

#include "Hasher.h"

namespace rkit
{
	template<class TChar, CharacterEncoding TEncoding, size_t TStaticSize>
	struct Hasher<BaseString<TChar, TEncoding, TStaticSize> > : public DefaultSpanHasher<BaseString<TChar, TEncoding, TStaticSize> >
	{
	public:
		static HashValue_t ComputeHash(HashValue_t baseHash, const BaseString<TChar, TEncoding, TStaticSize> &str);
	};
}

#include "RKitAssert.h"
#include "Algorithm.h"
#include "Format.h"
#include "MallocDriver.h"
#include "StaticArray.h"
#include "StringView.h"
#include "UtilitiesDriver.h"
#include "Result.h"

#include <cstring>
#include <limits>
#include <cstdarg>


template<class TChar>
rkit::StringStorage<TChar>::StringStorage(IMallocDriver *alloc)
	: m_alloc(alloc), m_counter(static_cast<CounterValueType_t>(1))
{
}

template<class TChar>
rkit::StringStorage<TChar>::~StringStorage()
{
}

template<class TChar>
void rkit::StringStorage<TChar>::IncRef()
{
	(void)m_counter.Increment();
}

template<class TChar>
void rkit::StringStorage<TChar>::DecRef()
{
	CounterValueType_t newValue = m_counter.Decrement() - 1;

	if (newValue == 0)
	{
		IMallocDriver *alloc = m_alloc;
		void *memBase = static_cast<void *>(this);
		this->~StringStorage();

		alloc->Free(memBase);
	}

}

template<class TChar>
TChar *rkit::StringStorage<TChar>::GetFirstCharAddress() const
{
	return reinterpret_cast<TChar *>(const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(this)) + ComputePaddedBaseSize());
}

template<class TChar>
rkit::Result rkit::StringStorage<TChar>::ComputeSize(size_t sizeInChars, size_t &outSizeInBytes)
{
	size_t sz = sizeInChars;
	RKIT_CHECK(rkit::SafeMul(sz, sz, sizeof(TChar)));
	RKIT_CHECK(rkit::SafeAdd(sz, sz, ComputePaddedBaseSize()));

	outSizeInBytes = sz;
	return ResultCode::kOK;
}


template<class TChar>
rkit::StringStorage<TChar> *rkit::StringStorage<TChar>::LocateFromFirstCharAddress(const TChar *chars)
{
	return reinterpret_cast<StringStorage<TChar> *>(const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(chars) - ComputePaddedBaseSize()));
}

template<class TChar>
size_t rkit::StringStorage<TChar>::ComputePaddedBaseSize()
{
	constexpr size_t baseSize = sizeof(StringStorage<TChar>);
	constexpr size_t charAlign = alignof(TChar);
	constexpr size_t residual = baseSize % charAlign;

	return (residual == 0) ? baseSize : (baseSize + sizeof(TChar) - residual);
}

template<class TChar>
rkit::BaseStringConstructionBuffer<TChar>::BaseStringConstructionBuffer()
	: m_stringStorage(nullptr)
	, m_numChars(0)
{
}

template<class TChar>
rkit::BaseStringConstructionBuffer<TChar>::~BaseStringConstructionBuffer()
{
	if (m_stringStorage)
		m_stringStorage->DecRef();
}

template<class TChar>
rkit::Result rkit::BaseStringConstructionBuffer<TChar>::Allocate(size_t numChars)
{
	return Allocate(numChars, GetDrivers().m_mallocDriver);
}

template<class TChar>
rkit::Result rkit::BaseStringConstructionBuffer<TChar>::Allocate(size_t numChars, IMallocDriver *alloc)
{
	if (numChars == std::numeric_limits<size_t>::max())
		return ResultCode::kOutOfMemory;

	size_t stringStorageObjSize = 0;
	RKIT_CHECK(StringStorage<TChar>::ComputeSize(numChars + 1, stringStorageObjSize));

	void *stringStorageMem = alloc->Alloc(stringStorageObjSize);
	if (!stringStorageMem)
		return ResultCode::kOutOfMemory;

	StringStorage<TChar> *stringStorage = new (stringStorageMem) StringStorage<TChar>(alloc);

	TChar *chars = stringStorage->GetFirstCharAddress();
	chars[numChars] = static_cast<TChar>(0);

	m_stringStorage = stringStorage;
	m_numChars = numChars;

	return ResultCode::kOK;
}

template<class TChar>
rkit::Span<TChar> rkit::BaseStringConstructionBuffer<TChar>::GetSpan() const
{
	return Span<TChar>(m_stringStorage->GetFirstCharAddress(), m_numChars);
}

template<class TChar>
void rkit::BaseStringConstructionBuffer<TChar>::Detach()
{
	m_stringStorage = nullptr;
	m_numChars = 0;
}

template<class TChar, rkit::CharacterEncoding TEncoding>
void rkit::priv::StringFormatHelper<TChar, TEncoding>::WriteChars(const Span<const TChar> &chars)
{
	if (chars.Count() == 0)
		return;

	size_t available = 0;
	if (m_charsBuffer.Count() >= m_charsRequired)
		available = m_charsBuffer.Count() - m_charsRequired;

	if (chars.Count() <= available)
		CopySpanNonOverlapping(m_charsBuffer.SubSpan(m_charsRequired, chars.Count()), chars);

	size_t numericallyAvailable = std::numeric_limits<size_t>::max() - m_charsRequired;

	if (chars.Count() > numericallyAvailable)
	{
		m_overflowed = true;
		m_charsRequired = std::numeric_limits<size_t>::max();
	}
	else
		m_charsRequired += chars.Count();
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::BaseString<TChar, TEncoding, TStaticSize>::BaseString()
	: m_chars(nullptr)
	, m_length(0)
{
	m_staticString[0] = static_cast<TChar>(0);
	m_chars = m_staticString;
}


template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::BaseString<TChar, TEncoding, TStaticSize>::BaseString(const BaseString &other)
	: m_chars(nullptr)
	, m_length(other.m_length)
{
	if (other.IsStaticString())
	{
		CopyStaticStringFromOther(other);
		m_chars = m_staticString;
	}
	else
	{
		m_chars = other.m_chars;
		StringStorage<TChar>::LocateFromFirstCharAddress(m_chars)->IncRef();
	}
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::BaseString<TChar, TEncoding, TStaticSize>::BaseString(BaseString &&other) noexcept
	: m_length(other.m_length)
{
	if (other.IsStaticString())
	{
		CopyStaticStringFromOther(other);
		m_chars = m_staticString;
	}
	else
		m_chars = other.m_chars;

	other.UnsafeReset();
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::BaseString<TChar, TEncoding, TStaticSize>::BaseString(BaseStringConstructionBuffer<TChar> &&other) noexcept
	: m_chars(nullptr)
	, m_length(0)
{
	m_staticString[0] = static_cast<TChar>(0);
	m_chars = m_staticString;

	(*this) = std::move(other);
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::BaseString<TChar, TEncoding, TStaticSize>::~BaseString()
{
	Evict();
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
void rkit::BaseString<TChar, TEncoding, TStaticSize>::CopyStaticStringFromOther(const BaseString &other)
{
	const size_t length = other.m_length;
	RKIT_ASSERT(length < TStaticSize);

	CopySpanNonOverlapping(Span<TChar>(m_staticString, length), Span<const TChar>(other.m_staticString, length));
	m_staticString[length] = static_cast<TChar>(0);
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
void rkit::BaseString<TChar, TEncoding, TStaticSize>::Evict()
{
	if (!IsStaticString())
	{
		StringStorage<TChar> *storage = StringStorage<TChar>::LocateFromFirstCharAddress(m_chars);
		storage->DecRef();
	}
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
void rkit::BaseString<TChar, TEncoding, TStaticSize>::UnsafeReset()
{
	m_chars = m_staticString;
	m_length = 0;
	m_staticString[0] = static_cast<TChar>(0);
}


template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::BaseString<TChar, TEncoding, TStaticSize> &rkit::BaseString<TChar, TEncoding, TStaticSize>::operator=(const BaseString &other)
{
	if (this != &other)
	{
		Evict();

		if (other.IsStaticString())
		{
			CopyStaticStringFromOther(other);
			m_chars = m_staticString;
		}
		else
		{
			m_chars = other.m_chars;
			StringStorage<TChar>::LocateFromFirstCharAddress(m_chars)->IncRef();
		}

		m_length = other.m_length;
	}

	return *this;
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::BaseString<TChar, TEncoding, TStaticSize> &rkit::BaseString<TChar, TEncoding, TStaticSize>::operator=(BaseString &&other) noexcept
{
	Evict();

	if (other.IsStaticString())
	{
		CopyStaticStringFromOther(other);
		m_chars = m_staticString;
	}
	else
		m_chars = other.m_chars;

	m_length = other.m_length;

	other.UnsafeReset();

	return *this;
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::BaseString<TChar, TEncoding, TStaticSize> &rkit::BaseString<TChar, TEncoding, TStaticSize>::operator=(BaseStringConstructionBuffer<TChar> &&other) noexcept
{
	Evict();

	Span<TChar> span = other.GetSpan();
	if (span.Count() < TStaticSize)
	{
		CopySpanNonOverlapping(Span<TChar>(m_staticString, span.Count()), Span<const TChar>(span));
		m_staticString[span.Count()] = static_cast<TChar>(0);
		m_chars = m_staticString;
	}
	else
	{
		m_chars = span.Ptr();
		other.Detach();
	}

	m_length = span.Count();

	return *this;
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
size_t rkit::BaseString<TChar, TEncoding, TStaticSize>::Length() const
{
	return m_length;
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
template<class... TParams>
rkit::Result rkit::BaseString<TChar, TEncoding, TStaticSize>::Format(const BaseStringSliceView<TChar, TEncoding> &fmt, const TParams &...params)
{
	return VFormat(fmt, CreateFormatParameterList<TChar, TParams...>(params...));
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::Result rkit::BaseString<TChar, TEncoding, TStaticSize>::VFormat(const BaseStringSliceView<TChar, TEncoding> &fmt, const FormatParameterList<TChar> &args)
{
	size_t charsRequired = 0;
	{
		// Formatting into the static buffer is permitted so we need to use a temp
		StaticArray<TChar, TStaticSize - 1> tempStaticBuffer;

		priv::StringFormatHelper<TChar, TEncoding> formatHelper;

		formatHelper.m_charsBuffer = tempStaticBuffer.ToSpan();

		GetDrivers().m_utilitiesDriver->FormatString(formatHelper, fmt, args);
		if (formatHelper.m_overflowed)
			return rkit::ResultCode::kOutOfMemory;

		if (formatHelper.m_charsRequired <= formatHelper.m_charsBuffer.Count())
		{
			Evict();
			CopySpanNonOverlapping<TChar>(Span<TChar>(m_staticString, formatHelper.m_charsRequired), tempStaticBuffer.ToSpan().SubSpan(0, formatHelper.m_charsRequired));
			m_staticString[formatHelper.m_charsRequired] = static_cast<TChar>(0);
			m_chars = m_staticString;
			m_length = formatHelper.m_charsRequired;
			return rkit::ResultCode::kOK;
		}

		charsRequired = formatHelper.m_charsRequired;
	}

	BaseStringConstructionBuffer<TChar> constructionBuffer;
	RKIT_CHECK(constructionBuffer.Allocate(charsRequired));

	{
		priv::StringFormatHelper<TChar, TEncoding> formatHelper;

		formatHelper.m_charsBuffer = constructionBuffer.GetSpan();

		GetDrivers().m_utilitiesDriver->FormatString(formatHelper, fmt, args);
		RKIT_ASSERT(!formatHelper.m_overflowed);
		RKIT_ASSERT(formatHelper.m_charsRequired == charsRequired);
	}

	(*this) = std::move(constructionBuffer);

	return ResultCode::kOK;
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::Ordering rkit::BaseString<TChar, TEncoding, TStaticSize>::Compare(const BaseStringSliceView<TChar, TEncoding> &other) const
{
	return static_cast<BaseStringSliceView<TChar, TEncoding>>(*this).Compare(other);
}


template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
void rkit::BaseString<TChar, TEncoding, TStaticSize>::FormatValue(IFormatStringWriter<TChar> &writer) const
{
	return static_cast<BaseStringSliceView<TChar, TEncoding>>(*this).FormatValue(writer);
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
const TChar *rkit::BaseString<TChar, TEncoding, TStaticSize>::CStr() const
{
	return m_chars;
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
void rkit::BaseString<TChar, TEncoding, TStaticSize>::Clear()
{
	Evict();
	UnsafeReset();
}


template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::BaseStringSliceView<TChar, TEncoding> rkit::BaseString<TChar, TEncoding, TStaticSize>::SubString(size_t start, size_t length) const
{
	return rkit::BaseStringSliceView<TChar, TEncoding>(*this).SubString(start, length);
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::Span<const TChar> rkit::BaseString<TChar, TEncoding, TStaticSize>::ToSpan() const
{
	return static_cast<SliceView_t>(*this).ToSpan();
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
bool rkit::BaseString<TChar, TEncoding, TStaticSize>::IsStaticString() const
{
	return m_chars == m_staticString;
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::Result rkit::BaseString<TChar, TEncoding, TStaticSize>::Set(const BaseStringSliceView<TChar, TEncoding> &strView)
{
	return Set(strView.ToSpan());
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::Result rkit::BaseString<TChar, TEncoding, TStaticSize>::Set(const Span<const TChar> &strSpan)
{
	if (strSpan.Count() == 0)
	{
		Clear();
		return ResultCode::kOK;
	}

	if (!IsStaticString())
	{
		// Try to deallocate the existing string first, if the strSpan is not contained
		// within the current string
		const TChar *chars = strSpan.Ptr();
		size_t length = strSpan.Count();

		if ((chars >= m_chars + m_length) || (m_chars >= chars + m_length))
		{
			// Separate memory range, or only the null terminator (which can be safely handled)
			Clear();
		}
	}

	BaseString<TChar, TEncoding, TStaticSize> newString;

	Span<TChar> uninitSpan;
	RKIT_CHECK(CreateAndReturnUninitializedSpan(newString, strSpan.Count(), uninitSpan));

	CopySpanNonOverlapping(uninitSpan, strSpan);

	(*this) = std::move(newString);

	return ResultCode::kOK;
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::Result rkit::BaseString<TChar, TEncoding, TStaticSize>::Append(const Span<const TChar> &span)
{
	if (span.Count() == 0)
		return ResultCode::kOK;

	if (std::numeric_limits<size_t>::max() - m_length < span.Count())
		return ResultCode::kOutOfMemory;

	const size_t combinedLength = m_length + span.Count();

	if (combinedLength < TStaticSize)
	{
		// Existing length will also be under static size
		::memcpy(m_staticString + m_length, span.Ptr(), span.Count() * sizeof(TChar));
		m_staticString[combinedLength] = static_cast<TChar>(0);

		m_length = combinedLength;
	}
	else
	{
		// New string won't fit
		BaseString<TChar, TEncoding, TStaticSize> newString;

		Span<TChar> uninitSpan;
		RKIT_CHECK(CreateAndReturnUninitializedSpan(newString, combinedLength, uninitSpan));

		CopySpanNonOverlapping(Span<TChar>(uninitSpan.Ptr(), m_length), Span<const TChar>(m_chars, m_length));
		CopySpanNonOverlapping(Span<TChar>(uninitSpan.Ptr() + m_length, span.Count()), span);

		(*this) = std::move(newString);
	}

	return ResultCode::kOK;
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::Result rkit::BaseString<TChar, TEncoding, TStaticSize>::Append(const BaseStringSliceView<TChar, TEncoding> &strView)
{
	return this->Append(strView.ToSpan());
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::Result rkit::BaseString<TChar, TEncoding, TStaticSize>::Append(TChar ch)
{
	return this->Append(Span<const TChar>(&ch, 1));
}


template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
template<class TAdjustFunc>
rkit::Result rkit::BaseString<TChar, TEncoding, TStaticSize>::ChangeCase(const TAdjustFunc &adjuster)
{
	size_t length = m_length;

	if (IsStaticString())
	{
		TChar *chars = m_staticString;
		for (size_t i = 0; i < length; i++)
			chars[i] = adjuster(chars[i]);

		return ResultCode::kOK;
	}

	BaseString<TChar, TEncoding, TStaticSize> newString;

	Span<TChar> uninitSpan;
	RKIT_CHECK(CreateAndReturnUninitializedSpan(newString, length, uninitSpan));

	TChar *chars = uninitSpan.Ptr();
	const TChar *inChars = m_chars;

	for (size_t i = 0; i < length; i++)
		chars[i] = adjuster(inChars[i]);

	(*this) = std::move(newString);

	return ResultCode::kOK;
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::Result rkit::BaseString<TChar, TEncoding, TStaticSize>::MakeLower()
{
	return ChangeCase(InvariantCharCaseAdjuster<TChar>::ToLower);
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::Result rkit::BaseString<TChar, TEncoding, TStaticSize>::MakeUpper()
{
	return ChangeCase(InvariantCharCaseAdjuster<TChar>::ToUpper);
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
bool rkit::BaseString<TChar, TEncoding, TStaticSize>::StartsWith(const BaseStringSliceView<TChar, TEncoding> &strView) const
{
	return this->StartsWith(strView.ToSpan());
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
bool rkit::BaseString<TChar, TEncoding, TStaticSize>::StartsWith(const Span<const TChar> &span) const
{
	if (m_length < span.Count())
		return false;

	return !::memcmp(this->CStr(), span.Ptr(), sizeof(TChar) * span.Count());
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
bool rkit::BaseString<TChar, TEncoding, TStaticSize>::StartsWith(TChar ch) const
{
	return this->StartsWith(Span<const TChar>(&ch, 1));
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
bool rkit::BaseString<TChar, TEncoding, TStaticSize>::EndsWith(const BaseStringSliceView<TChar, TEncoding> &strView) const
{
	return this->EndsWith(strView.ToSpan());
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
bool rkit::BaseString<TChar, TEncoding, TStaticSize>::EndsWith(const Span<const TChar> &span) const
{
	if (m_length < span.Count())
		return false;

	return !::memcmp(this->CStr() + m_length - span.Count(), span.Ptr(), sizeof(TChar) * span.Count());
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
bool rkit::BaseString<TChar, TEncoding, TStaticSize>::EndsWith(TChar ch) const
{
	return this->EndsWith(Span<const TChar>(&ch, 1));
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::BaseString<TChar, TEncoding, TStaticSize>::operator View_t() const
{
	return View_t(m_chars, m_length);
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::BaseString<TChar, TEncoding, TStaticSize>::operator SliceView_t() const
{
	return SliceView_t(m_chars, m_length);
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
const TChar &rkit::BaseString<TChar, TEncoding, TStaticSize>::operator[](size_t index) const
{
	RKIT_ASSERT(index < m_length);
	return CStr()[index];
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::Result rkit::BaseString<TChar, TEncoding, TStaticSize>::CreateAndReturnUninitializedSpan(BaseString &outStr, size_t numChars, Span<TChar> &outSpan)
{
	outStr = BaseString();

	if (numChars < TStaticSize)
	{
		outStr.m_length = numChars;
		outStr.m_chars = outStr.m_staticString;
		outStr.m_staticString[numChars] = static_cast<TChar>(0);

		outSpan = Span<TChar>(outStr.m_staticString, numChars);
		return ResultCode::kOK;
	}

	BaseStringConstructionBuffer<TChar> constructionBuffer;
	RKIT_CHECK(constructionBuffer.Allocate(numChars));

	Span<TChar> constructedSpan = constructionBuffer.GetSpan();
	constructionBuffer.Detach();

	outStr.m_length = constructedSpan.Count();
	outStr.m_chars = constructedSpan.Ptr();

	outSpan = constructedSpan;

	return ResultCode::kOK;
}

template<class TChar, rkit::CharacterEncoding TEncoding, size_t TStaticSize>
rkit::HashValue_t rkit::Hasher<rkit::BaseString<TChar, TEncoding, TStaticSize> >::ComputeHash(HashValue_t baseHash, const BaseString<TChar, TEncoding, TStaticSize> &str)
{
	return Hasher<BaseStringView<TChar, TEncoding> >::ComputeHash(baseHash, str);
}
