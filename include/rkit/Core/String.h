#pragma once

#include "Atomic.h"
#include "NoCopy.h"

#include <cstddef>

namespace rkit
{
	struct IMallocDriver;
	struct Result;

	template<class T>
	class Span;

	template<class TChar>
	class BaseStringView;

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

	template<class TChar, size_t TStaticSize>
	class BaseString
	{
	public:
		typedef BaseStringView<TChar> View_t;

		BaseString();
		BaseString(const BaseString &other);
		BaseString(BaseString &&other);
		~BaseString();

		BaseString &operator=(const BaseString &other);
		BaseString &operator=(BaseString &&other) noexcept;

		Result Set(const BaseStringView<TChar> &strView);
		Result Append(const BaseStringView<TChar> &strView);
		Result Append(const Span<const TChar> &span);
		Result Append(TChar ch);

		bool EndsWith(const BaseStringView<TChar> &strView) const;
		bool EndsWith(const Span<const TChar> &span) const;
		bool EndsWith(TChar ch) const;

		operator View_t() const;

		bool operator==(const BaseStringView<TChar> &other) const;
		bool operator==(const BaseString &other) const;

		bool operator!=(const BaseStringView<TChar> &other) const;
		bool operator!=(const BaseString &other) const;

		const TChar &operator[](size_t index) const;

		void Clear();

		const TChar *CStr() const;
		size_t Length() const;

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

	template<class TChar>
	class BaseStringView
	{
	public:
		BaseStringView();
		BaseStringView(const TChar *chars, size_t length);

		template<size_t TLength>
		BaseStringView(const TChar (&charsArray)[TLength]);

		const TChar &operator[](size_t index) const;

		BaseStringView<TChar> SubString(size_t start, size_t length) const;

		Span<const TChar> ToSpan() const;

		const TChar *GetChars() const;
		size_t Length() const;

		bool operator==(const BaseStringView<TChar> &other) const;
		bool operator!=(const BaseStringView<TChar> &other) const;

	private:
		Span<const TChar> m_span;

		static const TChar kDefaultStrChar;
	};
}

#include "RKitAssert.h"
#include "Algorithm.h"
#include "MallocDriver.h"
#include "Result.h"
#include "StringProto.h"

#include <cstring>
#include <limits>

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
	return reinterpret_cast<TChar *>(const_cast<char *>(reinterpret_cast<const char *>(this)) + ComputePaddedBaseSize());
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
	return reinterpret_cast<StringStorage<TChar> *>(const_cast<char *>(reinterpret_cast<const char *>(chars) - ComputePaddedBaseSize()));
}

template<class TChar>
size_t rkit::StringStorage<TChar>::ComputePaddedBaseSize()
{
	constexpr size_t baseSize = sizeof(StringStorage<TChar>);
	constexpr size_t charAlign = alignof(TChar);
	constexpr size_t residual = baseSize % charAlign;

	return (residual == 0) ? baseSize : (baseSize + sizeof(TChar) - residual);
}


template<class TChar, size_t TStaticSize>
rkit::BaseString<TChar, TStaticSize>::BaseString()
	: m_chars(nullptr)
	, m_length(0)
{
	m_staticString[0] = static_cast<TChar>(0);
	m_chars = m_staticString;
}


template<class TChar, size_t TStaticSize>
rkit::BaseString<TChar, TStaticSize>::BaseString(const BaseString &other)
	: m_chars(nullptr)
	, m_length(other.m_length)
{
	if (other.IsStaticString())
	{
		CopyStaticStringFromOther(other);
		m_chars = m_staticString;
	}
	else
		StringStorage<TChar>::LocateFromFirstCharAddress(m_chars)->IncRef();
}

template<class TChar, size_t TStaticSize>
rkit::BaseString<TChar, TStaticSize>::BaseString(BaseString &&other)
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

template<class TChar, size_t TStaticSize>
rkit::BaseString<TChar, TStaticSize>::~BaseString()
{
	Evict();
}

template<class TChar, size_t TStaticSize>
void rkit::BaseString<TChar, TStaticSize>::CopyStaticStringFromOther(const BaseString &other)
{
	const size_t length = other.m_length;
	RKIT_ASSERT(length < TStaticSize);

	CopySpanNonOverlapping(Span<TChar>(m_staticString, length), Span<const TChar>(other.m_staticString, length));
	m_staticString[length] = static_cast<TChar>(0);
}

template<class TChar, size_t TStaticSize>
void rkit::BaseString<TChar, TStaticSize>::Evict()
{
	if (!IsStaticString())
	{
		StringStorage<TChar> *storage = StringStorage<TChar>::LocateFromFirstCharAddress(m_chars);
		storage->DecRef();
	}
}

template<class TChar, size_t TStaticSize>
void rkit::BaseString<TChar, TStaticSize>::UnsafeReset()
{
	m_chars = m_staticString;
	m_length = 0;
	m_staticString[0] = static_cast<TChar>(0);
}


template<class TChar, size_t TStaticSize>
rkit::BaseString<TChar, TStaticSize> &rkit::BaseString<TChar, TStaticSize>::operator=(const BaseString &other)
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
	}

	return *this;
}

template<class TChar, size_t TStaticSize>
rkit::BaseString<TChar, TStaticSize> &rkit::BaseString<TChar, TStaticSize>::operator=(BaseString &&other) noexcept
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

template<class TChar, size_t TStaticSize>
size_t rkit::BaseString<TChar, TStaticSize>::Length() const
{
	return m_length;
}

template<class TChar, size_t TStaticSize>
const TChar *rkit::BaseString<TChar, TStaticSize>::CStr() const
{
	return m_chars;
}

template<class TChar, size_t TStaticSize>
void rkit::BaseString<TChar, TStaticSize>::Clear()
{
	Evict();
	UnsafeReset();
}

template<class TChar, size_t TStaticSize>
bool rkit::BaseString<TChar, TStaticSize>::IsStaticString() const
{
	return m_chars == m_staticString;
}

template<class TChar, size_t TStaticSize>
rkit::Result rkit::BaseString<TChar, TStaticSize>::Set(const BaseStringView<TChar> &strView)
{
	if (strView.Length() == 0)
	{
		Clear();
		return ResultCode::kOK;
	}

	if (!IsStaticString())
	{
		// Try to deallocate the existing string first, if the strView is not contained
		// within the current string
		const TChar *chars = strView.GetChars();
		size_t length = strView.Length();

		if ((chars >= m_chars + m_length) || (m_chars >= chars + m_length))
		{
			// Separate memory range, or only the null terminator (which can be safely handled)
			Clear();
		}
	}

	BaseString<TChar, TStaticSize> newString;

	Span<TChar> uninitSpan;
	RKIT_CHECK(CreateAndReturnUninitializedSpan(newString, strView.Length(), uninitSpan));

	CopySpanNonOverlapping(uninitSpan, strView.ToSpan());

	(*this) = std::move(newString);

	return ResultCode::kOK;
}

template<class TChar, size_t TStaticSize>
rkit::Result rkit::BaseString<TChar, TStaticSize>::Append(const Span<const TChar> &span)
{
	if (span.Count() == 0)
		return ResultCode::kOK;

	if (std::numeric_limits<size_t>::max() - m_length < span.Count())
		return ResultCode::kOutOfMemory;

	const size_t combinedLength = m_length + span.Count();

	if (combinedLength < TStaticSize)
	{
		// Existing length will also be under static size
		memcpy(m_staticString + m_length, span.Ptr(), span.Count() * sizeof(TChar));
		m_staticString[combinedLength] = static_cast<TChar>(0);

		m_length = combinedLength;
	}
	else
	{
		// New string won't fit
		BaseString<TChar, TStaticSize> newString;

		Span<TChar> uninitSpan;
		RKIT_CHECK(CreateAndReturnUninitializedSpan(newString, combinedLength, uninitSpan));

		CopySpanNonOverlapping(Span<TChar>(uninitSpan.Ptr(), m_length), Span<const TChar>(m_chars, m_length));
		CopySpanNonOverlapping(Span<TChar>(uninitSpan.Ptr() + m_length, span.Count()), span);

		(*this) = std::move(newString);
	}

	return ResultCode::kOK;
}

template<class TChar, size_t TStaticSize>
rkit::Result rkit::BaseString<TChar, TStaticSize>::Append(const BaseStringView<TChar> &strView)
{
	return this->Append(strView.ToSpan());
}

template<class TChar, size_t TStaticSize>
rkit::Result rkit::BaseString<TChar, TStaticSize>::Append(TChar ch)
{
	return this->Append(Span<const TChar>(&ch, 1));
}


template<class TChar, size_t TStaticSize>
bool rkit::BaseString<TChar, TStaticSize>::EndsWith(const BaseStringView<TChar> &strView) const
{
	return this->EndsWith(strView.ToSpan());
}

template<class TChar, size_t TStaticSize>
bool rkit::BaseString<TChar, TStaticSize>::EndsWith(const Span<const TChar> &span) const
{
	if (m_length < span.Count())
		return false;

	return !memcmp(this->CStr() + m_length - span.Count(), span.Ptr(), sizeof(TChar) * span.Count());
}

template<class TChar, size_t TStaticSize>
bool rkit::BaseString<TChar, TStaticSize>::EndsWith(TChar ch) const
{
	return this->EndsWith(Span<const TChar>(&ch, 1));
}

template<class TChar, size_t TStaticSize>
rkit::BaseString<TChar, TStaticSize>::operator View_t() const
{
	return BaseStringView<TChar>(m_chars, m_length);
}

template<class TChar, size_t TStaticSize>
bool rkit::BaseString<TChar, TStaticSize>::operator==(const BaseStringView<TChar> &other) const
{
	return static_cast<BaseStringView<TChar>>(*this) == other;
}

template<class TChar, size_t TStaticSize>
bool rkit::BaseString<TChar, TStaticSize>::operator==(const BaseString &other) const
{
	return (*this) == static_cast<rkit::BaseStringView<TChar> >(other);
}

template<class TChar, size_t TStaticSize>
bool rkit::BaseString<TChar, TStaticSize>::operator!=(const BaseStringView<TChar> &other) const
{
	return !((*this) == other);
}

template<class TChar, size_t TStaticSize>
bool rkit::BaseString<TChar, TStaticSize>::operator!=(const BaseString &other) const
{
	return !((*this) == other);
}


template<class TChar, size_t TStaticSize>
const TChar &rkit::BaseString<TChar, TStaticSize>::operator[](size_t index) const
{
	RKIT_ASSERT(index < m_length);
	return CStr()[index];
}

template<class TChar, size_t TStaticSize>
rkit::Result rkit::BaseString<TChar, TStaticSize>::CreateAndReturnUninitializedSpan(BaseString &outStr, size_t numChars, Span<TChar> &outSpan)
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

	if (numChars == std::numeric_limits<size_t>::max())
		return ResultCode::kOutOfMemory;

	size_t stringStorageObjSize = 0;
	RKIT_CHECK(StringStorage<TChar>::ComputeSize(numChars + 1, stringStorageObjSize));

	IMallocDriver *alloc = GetDrivers().m_mallocDriver;

	void *stringStorageMem = alloc->Alloc(stringStorageObjSize);
	if (!stringStorageMem)
		return ResultCode::kOutOfMemory;

	StringStorage<TChar> *stringStorage = new (stringStorageMem) StringStorage<TChar>(alloc);

	TChar *firstCharAddr = stringStorage->GetFirstCharAddress();
	firstCharAddr[numChars] = static_cast<TChar>(0);

	outStr.m_length = numChars;
	outStr.m_chars = firstCharAddr;

	outSpan = Span<TChar>(firstCharAddr, numChars);

	return ResultCode::kOK;
}

template<class TChar>
rkit::BaseStringView<TChar>::BaseStringView()
	: m_span(&kDefaultStrChar, 0)
{
}

template<class TChar>
rkit::BaseStringView<TChar>::BaseStringView(const TChar *chars, size_t length)
	: m_span(chars, length)
{
}

template<class TChar>
template<size_t TLength>
rkit::BaseStringView<TChar>::BaseStringView(const TChar(&charsArray)[TLength])
	: m_span(charsArray, TLength - 1)
{
	RKIT_ASSERT(charsArray[TLength - 1] == static_cast<TChar>(0));
}

template<class TChar>
const TChar &rkit::BaseStringView<TChar>::operator[](size_t index) const
{
	return m_span[index];
}


template<class TChar>
rkit::BaseStringView<TChar> rkit::BaseStringView<TChar>::SubString(size_t start, size_t length) const
{
	if (start > m_span.Count())
		start = m_span.Count();

	const size_t maxLength = m_span.Count() - start;
	if (length > maxLength)
		length = maxLength;

	return BaseStringView<TChar>(m_span.Ptr() + start, length);
}

template<class TChar>
rkit::Span<const TChar> rkit::BaseStringView<TChar>::ToSpan() const
{
	return m_span;
}

template<class TChar>
const TChar *rkit::BaseStringView<TChar>::GetChars() const
{
	return m_span.Ptr();
}

template<class TChar>
size_t rkit::BaseStringView<TChar>::Length() const
{
	return m_span.Count();
}

template<class TChar>
bool rkit::BaseStringView<TChar>::operator==(const BaseStringView<TChar> &other) const
{
	if (m_span.Count() != other.m_span.Count())
		return false;

	const size_t length = m_span.Count();
	const TChar *charsA = m_span.Ptr();
	const TChar *charsB = other.m_span.Ptr();

	for (size_t i = 0; i < length; i++)
	{
		if (charsA[i] != charsB[i])
			return false;
	}

	return true;
}

template<class TChar>
bool rkit::BaseStringView<TChar>::operator!=(const BaseStringView<TChar> &other) const
{
	return !((*this) == other);
}

template<class TChar>
const TChar rkit::BaseStringView<TChar>::kDefaultStrChar = static_cast<TChar>(0);
