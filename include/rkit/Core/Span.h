#pragma once

#include <cstddef>

#include "NoCopy.h"
#include "SpanProtos.h"

namespace std
{
	struct random_access_iterator_tag;
}

namespace rkit
{
	template<class T>
	struct ISpan;

	template<class T>
	struct SpanToISpanValueWrapper;

	template<class T>
	struct SpanToISpanRefWrapper;

	template<class T>
	class SpanIterator
	{
	public:
		typedef ptrdiff_t difference_type;
		typedef T value_type;
		typedef T &reference;
		typedef T *pointer;
		typedef std::random_access_iterator_tag iterator_category;

		explicit SpanIterator(T *ptr);

		SpanIterator &operator++();
		SpanIterator &operator--();

		SpanIterator operator++(int);
		SpanIterator operator--(int);

		SpanIterator &operator+=(size_t sz);
		SpanIterator &operator+=(ptrdiff_t diff);

		SpanIterator &operator-=(size_t sz);
		SpanIterator &operator-=(ptrdiff_t diff);

		ptrdiff_t operator-(const SpanIterator &other) const;
		SpanIterator operator-(size_t sz) const;
		SpanIterator operator-(ptrdiff_t sz) const;

		SpanIterator operator+(size_t sz) const;
		SpanIterator operator+(ptrdiff_t sz) const;

		bool operator<(const SpanIterator &other) const;
		bool operator<=(const SpanIterator &other) const;
		bool operator>(const SpanIterator &other) const;
		bool operator>=(const SpanIterator &other) const;
		bool operator!=(const SpanIterator &other) const;
		bool operator==(const SpanIterator &other) const;

		SpanIterator &operator=(const SpanIterator &other);

		operator SpanIterator<const T>() const;

		T &operator*() const;

	private:
		SpanIterator() = delete;

		T *m_ptr;
	};

	template<class T>
	class Span
	{
	public:
		Span();
		Span(T *arr, size_t count);

		T *Ptr() const;
		size_t Count() const;

		Span<T> SubSpan(size_t start, size_t size) const;

		SpanIterator<T> begin() const;
		SpanIterator<T> end() const;

		T &operator[](size_t index) const;

		operator Span<const T>() const;

		SpanToISpanRefWrapper<T> ToRefISpan() const;
		SpanToISpanValueWrapper<T> ToValueISpan() const;

	private:
		T *m_arr;
		size_t m_count;
	};

	template<class T>
	class ISpanIterator
	{
	public:
		explicit ISpanIterator(const ISpan<T> &span, size_t index);

		T operator *() const;

		ISpanIterator<T> &operator++();
		ISpanIterator<T> operator++(int);

		bool operator==(const ISpanIterator<T> &other) const;
		bool operator!=(const ISpanIterator<T> &other) const;

	private:
		ISpanIterator() = delete;

		const ISpan<T> &m_span;
		size_t m_index;
	};

	template<class T>
	struct ISpan
	{
		virtual ~ISpan() {}

		virtual size_t Count() const = 0;
		virtual T operator[](size_t index) const = 0;

		ISpanIterator<T> begin() const;
		ISpanIterator<T> end() const;
	};

	template<class T>
	struct SpanToISpanRefWrapper final : public ISpan<T&>
	{
		explicit SpanToISpanRefWrapper(T *arr, size_t count);

		size_t Count() const override;
		T &operator[](size_t index) const override;

	private:
		T *m_arr;
		size_t m_count;
	};

	template<class T>
	struct SpanToISpanValueWrapper final : public ISpan<T>
	{
		SpanToISpanValueWrapper();
		explicit SpanToISpanValueWrapper(T *arr, size_t count);

		size_t Count() const override;
		T operator[](size_t index) const override;

	private:
		T *m_arr;
		size_t m_count;
	};

	template<class T, class TUserdata>
	class CallbackSpan final : public ISpan<T>
	{
	public:
		typedef T (*GetElementFunc_t)(const TUserdata &, size_t);

		CallbackSpan();
		CallbackSpan(GetElementFunc_t getElementFunc, const TUserdata &userdata, size_t count);

		size_t Count() const override;
		T operator[](size_t index) const override;

	private:
		GetElementFunc_t m_callback;
		TUserdata m_userdata;
		size_t m_count;
	};

	template<class T>
	class ConcatenatedISpanISpan final : public ISpan<T>
	{
	public:
		ConcatenatedISpanISpan();
		ConcatenatedISpanISpan(const ISpan<const ISpan<T> *> &spans);

		size_t Count() const override;
		T operator[](size_t index) const override;

	private:
		const ISpan<const ISpan<T> *> *m_spans = nullptr;
		size_t m_numSpans = 0;
		size_t m_count = 0;
	};

	template<class T>
	class ConcatenatedSpanISpan final : public ISpan<T>
	{
	public:
		ConcatenatedSpanISpan();
		ConcatenatedSpanISpan(const Span<const ISpan<T> *> &spans);

		size_t Count() const override;
		T operator[](size_t index) const override;

	private:
		Span<const ISpan<T> *> m_spans;
		size_t m_numSpans = 0;
		size_t m_count = 0;
	};
}

#include "RKitAssert.h"

template<class T>
rkit::SpanIterator<T>::SpanIterator(T *ptr)
	: m_ptr(ptr)
{
}

template<class T>
rkit::SpanIterator<T> &rkit::SpanIterator<T>::operator++()
{
	++m_ptr;
	return *this;
}

template<class T>
rkit::SpanIterator<T> &rkit::SpanIterator<T>::operator--()
{
	--m_ptr;
	return *this;
}

template<class T>
rkit::SpanIterator<T> rkit::SpanIterator<T>::operator++(int)
{
	rkit::SpanIterator<T> cloned(*this);
	++m_ptr;
	return cloned;
}

template<class T>
rkit::SpanIterator<T> rkit::SpanIterator<T>::operator--(int)
{
	rkit::SpanIterator<T> cloned(*this);
	--m_ptr;
	return cloned;
}

template<class T>
rkit::SpanIterator<T> &rkit::SpanIterator<T>::operator+=(size_t sz)
{
	m_ptr += sz;
	return *this;
}

template<class T>
rkit::SpanIterator<T> &rkit::SpanIterator<T>::operator+=(ptrdiff_t diff)
{
	m_ptr += diff;
	return *this;
}

template<class T>
rkit::SpanIterator<T> &rkit::SpanIterator<T>::operator-=(size_t sz)
{
	m_ptr -= sz;
	return *this;
}

template<class T>
rkit::SpanIterator<T> &rkit::SpanIterator<T>::operator-=(ptrdiff_t diff)
{
	m_ptr -= diff;
	return *this;
}

template<class T>
ptrdiff_t rkit::SpanIterator<T>::operator-(const SpanIterator &other) const
{
	return m_ptr - other.m_ptr;
}

template<class T>
rkit::SpanIterator<T> rkit::SpanIterator<T>::operator-(size_t sz) const
{
	return SpanIterator(m_ptr - sz);
}

template<class T>
rkit::SpanIterator<T> rkit::SpanIterator<T>::operator-(ptrdiff_t diff) const
{
	return SpanIterator(m_ptr - diff);
}

template<class T>
rkit::SpanIterator<T> rkit::SpanIterator<T>::operator+(size_t sz) const
{
	return SpanIterator(m_ptr + sz);
}

template<class T>
rkit::SpanIterator<T> rkit::SpanIterator<T>::operator+(ptrdiff_t diff) const
{
	return SpanIterator(m_ptr + diff);
}

template<class T>
bool rkit::SpanIterator<T>::operator<(const SpanIterator &other) const
{
	return m_ptr < other.m_ptr;
}

template<class T>
bool rkit::SpanIterator<T>::operator<=(const SpanIterator &other) const
{
	return m_ptr <= other.m_ptr;
}

template<class T>
bool rkit::SpanIterator<T>::operator>(const SpanIterator &other) const
{
	return m_ptr > other.m_ptr;
}

template<class T>
bool rkit::SpanIterator<T>::operator>=(const SpanIterator &other) const
{
	return m_ptr >= other.m_ptr;
}

template<class T>
bool rkit::SpanIterator<T>::operator!=(const SpanIterator &other) const
{
	return m_ptr != other.m_ptr;
}

template<class T>
bool rkit::SpanIterator<T>::operator==(const SpanIterator &other) const
{
	return m_ptr == other.m_ptr;
}

template<class T>
rkit::SpanIterator<T> &rkit::SpanIterator<T>::operator=(const SpanIterator &other)
{
	m_ptr = other.m_ptr;
	return *this;
}

template<class T>
rkit::SpanIterator<T>::operator SpanIterator<const T>() const
{
	return SpanIterator<const T>(m_ptr);
}

template<class T>
T &rkit::SpanIterator<T>::operator*() const
{
	return *m_ptr;
}

template<class T>
rkit::Span<T>::Span()
	: m_arr(nullptr)
	, m_count(0)
{
}

template<class T>
rkit::Span<T>::Span(T *arr, size_t count)
	: m_arr(arr)
	, m_count(count)
{
}

template<class T>
T *rkit::Span<T>::Ptr() const
{
	return m_arr;
}

template<class T>
size_t rkit::Span<T>::Count() const
{
	return m_count;
}

template<class T>
rkit::Span<T> rkit::Span<T>::SubSpan(size_t start, size_t size) const
{
	RKIT_ASSERT(start <= m_count);
	RKIT_ASSERT(m_count - start >= size);
	return Span<T>(m_arr + start, size);
}

template<class T>
rkit::SpanIterator<T> rkit::Span<T>::begin() const
{
	return SpanIterator<T>(m_arr);
}

template<class T>
rkit::SpanIterator<T> rkit::Span<T>::end() const
{
	return SpanIterator<T>(m_arr + m_count);
}

template<class T>
T &rkit::Span<T>::operator[](size_t index) const
{
	RKIT_ASSERT(index < m_count);
	return m_arr[index];
}

template<class T>
rkit::Span<T>::operator Span<const T>() const
{
	return Span<const T>(m_arr, m_count);
}

template<class T>
rkit::SpanToISpanRefWrapper<T> rkit::Span<T>::ToRefISpan() const
{
	return rkit::SpanToISpanRefWrapper<T>(m_arr, m_count);
}

template<class T>
rkit::SpanToISpanValueWrapper<T> rkit::Span<T>::ToValueISpan() const
{
	return rkit::SpanToISpanValueWrapper<T>(m_arr, m_count);
}

template<class T>
rkit::ISpanIterator<T>::ISpanIterator(const ISpan<T> &span, size_t index)
	: m_span(span)
	, m_index(index)
{
}


template<class T>
T rkit::ISpanIterator<T>::operator *() const
{
	return m_span[m_index];
}

template<class T>
rkit::ISpanIterator<T> &rkit::ISpanIterator<T>::operator++()
{
	++m_index;
	return *this;
}

template<class T>
rkit::ISpanIterator<T> rkit::ISpanIterator<T>::operator++(int)
{
	ISpanIterator<T> copied(*this);
	++m_index;
	return copied;
}

template<class T>
bool rkit::ISpanIterator<T>::operator==(const ISpanIterator<T> &other) const
{
	return (&m_span) == (&other.m_span) && m_index == other.m_index;
}

template<class T>
bool rkit::ISpanIterator<T>::operator!=(const ISpanIterator<T> &other) const
{
	return !((*this) == other);
}

template<class T>
rkit::ISpanIterator<T> rkit::ISpan<T>::begin() const
{
	return ISpanIterator<T>(*this, 0);
}

template<class T>
rkit::ISpanIterator<T> rkit::ISpan<T>::end() const
{
	return ISpanIterator<T>(*this, this->Count());
}


template<class T>
rkit::SpanToISpanRefWrapper<T>::SpanToISpanRefWrapper(T *arr, size_t count)
	: m_arr(arr)
	, m_count(count)
{
}

template<class T>
size_t rkit::SpanToISpanRefWrapper<T>::Count() const
{
	return m_count;
}

template<class T>
T &rkit::SpanToISpanRefWrapper<T>::operator[](size_t index) const
{
	RKIT_ASSERT(index < m_count);
	return m_arr[index];
}


template<class T>
rkit::SpanToISpanValueWrapper<T>::SpanToISpanValueWrapper()
	: m_arr(nullptr)
	, m_count(0)
{
}

template<class T>
rkit::SpanToISpanValueWrapper<T>::SpanToISpanValueWrapper(T *arr, size_t count)
	: m_arr(arr)
	, m_count(count)
{
}

template<class T>
size_t rkit::SpanToISpanValueWrapper<T>::Count() const
{
	return m_count;
}

template<class T>
T rkit::SpanToISpanValueWrapper<T>::operator[](size_t index) const
{
	RKIT_ASSERT(index < m_count);
	return m_arr[index];
}


template<class T, class TUserdata>
rkit::CallbackSpan<T, TUserdata>::CallbackSpan()
	: m_callback(nullptr)
	, m_userdata()
	, m_count(0)
{
}

template<class T, class TUserdata>
rkit::CallbackSpan<T, TUserdata>::CallbackSpan(GetElementFunc_t getElementFunc, const TUserdata &userdata, size_t count)
	: m_callback(getElementFunc)
	, m_userdata(userdata)
	, m_count(count)
{
}

template<class T, class TUserdata>
size_t rkit::CallbackSpan<T, TUserdata>::Count() const
{
	return m_count;
}

template<class T, class TUserdata>
T rkit::CallbackSpan<T, TUserdata>::operator[](size_t index) const
{
	return m_callback(m_userdata, index);
}


template<class T>
rkit::ConcatenatedISpanISpan<T>::ConcatenatedISpanISpan()
	: m_spans(nullptr)
	, m_count(0)
	, m_numSpans(0)
{
}

template<class T>
rkit::ConcatenatedISpanISpan<T>::ConcatenatedISpanISpan(const ISpan<const ISpan<T> *> &spans)
	: m_spans(&spans)
	, m_count(0)
	, m_numSpans(0)
{
	size_t count = 0;
	for (const ISpan<T> *subSpan : spans)
		count += subSpan->Count();

	m_count = count;
	m_numSpans = spans.Count();
}

template<class T>
size_t rkit::ConcatenatedISpanISpan<T>::Count() const
{
	return m_count;
}

template<class T>
T rkit::ConcatenatedISpanISpan<T>::operator[](size_t index) const
{
	RKIT_ASSERT(index < m_count);

	for (size_t i = 1; i < m_numSpans; i++)
	{
		const ISpan<T> &span = *(*m_spans)[i - 1];
		const size_t count = span.Count();
		if (index < count)
			return span[index];

		index -= count;
	}

	const ISpan<T> &lastSpan = *(*m_spans)[m_numSpans - 1];
	return lastSpan[index];
}

template<class T>
rkit::ConcatenatedSpanISpan<T>::ConcatenatedSpanISpan()
	: m_count(0)
	, m_numSpans(0)
{
}

template<class T>
rkit::ConcatenatedSpanISpan<T>::ConcatenatedSpanISpan(const Span<const ISpan<T> *> &spans)
	: m_spans(spans)
	, m_count(0)
	, m_numSpans(0)
{
	size_t count = 0;
	for (const ISpan<T> *subSpan : spans)
		count += subSpan->Count();

	m_count = count;
	m_numSpans = spans.Count();
}

template<class T>
size_t rkit::ConcatenatedSpanISpan<T>::Count() const
{
	return m_count;
}

template<class T>
T rkit::ConcatenatedSpanISpan<T>::operator[](size_t index) const
{
	RKIT_ASSERT(index < m_count);

	for (size_t i = 1; i < m_numSpans; i++)
	{
		const ISpan<T> &span = *m_spans[i - 1];
		const size_t count = span.Count();
		if (index < count)
			return span[index];

		index -= count;
	}


	const ISpan<T> &lastSpan = *m_spans[m_numSpans - 1];
	return lastSpan[index];
}
