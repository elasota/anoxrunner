#pragma once

#include <cstddef>

namespace rkit
{
	template<class T>
	struct ISpan;

	template<class T>
	class SpanIterator
	{
	public:
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

		SpanIterator &operator=(const SpanIterator &other) const;

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

		SpanIterator<T> begin() const;
		SpanIterator<T> end() const;

		T &operator[](size_t index) const;

	private:
		T *m_arr;
		size_t m_count;
	};

	template<class T>
	class ISpanIterator
	{
	public:
		explicit ISpanIterator(const ISpan<T> &span, size_t index);

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
		virtual T &operator[](size_t index) const = 0;

		ISpanIterator<T> begin() const;
		ISpanIterator<T> end() const;
	};

	template<class T, class TUserdata>
	class CallbackSpan final : public ISpan<T>
	{
	public:
		typedef T &(*GetElementFunc_t)(const TUserdata &, size_t);

		CallbackSpan(GetElementFunc_t getElementFunc, const TUserdata &userdata, size_t count);

		size_t Count() const override;
		T &operator[](size_t index) const override;

	private:
		GetElementFunc_t m_callback;
		TUserdata m_userdata;
		size_t m_count;
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
rkit::SpanIterator<T> &rkit::SpanIterator<T>::operator=(const SpanIterator &other) const
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
rkit::ISpanIterator<T>::ISpanIterator(const ISpan<T> &span, size_t index)
	: m_span(span)
{
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
T &rkit::CallbackSpan<T, TUserdata>::operator[](size_t index) const
{
	return m_callback(m_userdata, index);
}
