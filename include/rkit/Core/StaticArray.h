#pragma once

#include <cstddef>

namespace rkit
{
	template<class T>
	class Span;

	template<class T, size_t TSize>
	class StaticArray
	{
	public:
		StaticArray();
		StaticArray(const StaticArray<T, TSize> &other);
		StaticArray(StaticArray<T, TSize> &&other);
		~StaticArray();

		StaticArray<T, TSize> &operator=(const StaticArray<T, TSize> &other);
		StaticArray<T, TSize> &operator=(StaticArray<T, TSize> &&other);

		T &operator[](size_t index);
		const T &operator[](size_t index) const;

		Span<T> ToSpan();
		Span<const T> ToSpan() const;

		T *begin();
		T *end();

		const T *begin() const;
		const T *end() const;


	private:
		T m_elements[TSize];
	};
}

#include "RKitAssert.h"
#include "Span.h"

#include <utility>

namespace rkit
{
	template<class T, size_t TSize>
	StaticArray<T, TSize>::StaticArray()
		: m_elements{}
	{
	}

	template<class T, size_t TSize>
	StaticArray<T, TSize>::StaticArray(const StaticArray<T, TSize> &other)
		: m_elements(other.m_elements)
	{
	}

	template<class T, size_t TSize>
	StaticArray<T, TSize>::StaticArray(StaticArray<T, TSize> &&other)
		: m_elements(std::move(other.m_elements))
	{
	}

	template<class T, size_t TSize>
	StaticArray<T, TSize>::~StaticArray()
	{
	}

	template<class T, size_t TSize>
	StaticArray<T, TSize> &StaticArray<T, TSize>::operator=(const StaticArray<T, TSize> &other)
	{
		for (size_t i = 0; i < TSize; i++)
			m_elements[i] = other.m_elements[i];

		return *this;
	}

	template<class T, size_t TSize>
	StaticArray<T, TSize> &StaticArray<T, TSize>::operator=(StaticArray<T, TSize> &&other)
	{
		for (size_t i = 0; i < TSize; i++)
			m_elements[i] = std::move(other.m_elements[i]);

		return *this;
	}

	template<class T, size_t TSize>
	T &StaticArray<T, TSize>::operator[](size_t index)
	{
		RKIT_ASSERT(index < TSize);
		return m_elements[index];
	}

	template<class T, size_t TSize>
	const T &StaticArray<T, TSize>::operator[](size_t index) const
	{
		RKIT_ASSERT(index < TSize);
		return m_elements[index];
	}


	template<class T, size_t TSize>
	Span<T> StaticArray<T, TSize>::ToSpan()
	{
		return Span<T>(m_elements, TSize);
	}

	template<class T, size_t TSize>
	Span<const T> StaticArray<T, TSize>::ToSpan() const
	{
		return Span<const T>(m_elements, TSize);
	}

	template<class T, size_t TSize>
	T *StaticArray<T, TSize>::begin()
	{
		return m_elements;
	}

	template<class T, size_t TSize>
	T *StaticArray<T, TSize>::end()
	{
		return m_elements + TSize;
	}

	template<class T, size_t TSize>
	const T *StaticArray<T, TSize>::begin() const
	{
		return m_elements;
	}

	template<class T, size_t TSize>
	const T *StaticArray<T, TSize>::end() const
	{
		return m_elements + TSize;
	}
}
