#include "StaticBoolVector.h"

#include <cstddef>

namespace rkit
{
	template<class T>
	class EnumMask;

	template<class T>
	class EnumMaskIterator
	{
	public:
		friend class EnumMask<T>;

		EnumMaskIterator();

		EnumMaskIterator operator++(int);
		EnumMaskIterator &operator++();

		EnumMaskIterator operator--(int);
		EnumMaskIterator &operator--();

		bool operator==(const EnumMaskIterator<T> &other) const;
		bool operator!=(const EnumMaskIterator<T> &other) const;

		T operator*() const;

	private:
		explicit EnumMaskIterator(const EnumMask<T> &enumMask, size_t index);

		const EnumMask<T> *m_enumMask;
		size_t m_index;
	};

	template<class T>
	class EnumMask
	{
	public:
		friend class EnumMaskIterator<T>;

		EnumMask();

		template<size_t TCount>
		explicit EnumMask(const T(&items)[TCount]);

		EnumMask(const EnumMask &other) = default;

		bool Get(T item) const;
		EnumMask<T> &Set(T item, bool value) const;

		template<size_t TCount>
		EnumMask &Add(const T(&items)[TCount]);

		template<size_t TCount>
		EnumMask &Remove(const T(&items)[TCount]);

		bool operator==(const EnumMask &other) const;
		bool operator!=(const EnumMask &other) const;

		EnumMask operator|(const EnumMask &other) const;
		EnumMask operator&(const EnumMask &other) const;
		EnumMask operator^(const EnumMask &other) const;

		EnumMask operator~() const;

		EnumMaskIterator<T> begin() const;
		EnumMaskIterator<T> end() const;

	private:
		static const size_t kMax = static_cast<size_t>(T::kCount);

		explicit EnumMask(const StaticBoolVector<kMax> &boolVector);

		StaticBoolVector<kMax> m_boolVector;
	};
}

namespace rkit
{
	template<class T>
	EnumMaskIterator<T>::EnumMaskIterator()
		: m_enumMask(nullptr)
		, m_index(0)
	{
	}

	template<class T>
	EnumMaskIterator<T> EnumMaskIterator<T>::operator++(int)
	{
		EnumMaskIterator result = (*this);
		++m_index;
		return result;
	}

	template<class T>
	EnumMaskIterator<T> &EnumMaskIterator<T>::operator++()
	{
		size_t nudgedIndex = ++m_index;

		while (nudgedIndex < EnumMask<T>::kMax)
		{
			if (m_enumMask->m_boolVector.Get(nudgedIndex))
			{
				m_index = nudgedIndex;
				break;
			}

			++nudgedIndex;
		}

		return *this;
	}

	template<class T>
	EnumMaskIterator<T> EnumMaskIterator<T>::operator--(int)
	{
		EnumMaskIterator result = (*this);
		--m_index;
		return result;
	}

	template<class T>
	EnumMaskIterator<T> &EnumMaskIterator<T>::operator--()
	{
		for (;;)
		{
			RKIT_ASSERT(m_index > 0);
			--m_index;

			if (m_enumMask->m_boolVector.Get(m_index))
				break;
		}

		return *this;
	}

	template<class T>
	bool EnumMaskIterator<T>::operator==(const EnumMaskIterator<T> &other) const
	{
		return m_enumMask == other.m_enumMask && m_index == other.m_index;
	}

	template<class T>
	bool EnumMaskIterator<T>::operator!=(const EnumMaskIterator<T> &other) const
	{
		return !((*this) == other);
	}

	template<class T>
	T EnumMaskIterator<T>::operator*() const
	{
		RKIT_ASSERT(m_index < EnumMask<T>::kMax);
		return static_cast<T>(m_index);
	}

	template<class T>
	EnumMaskIterator<T>::EnumMaskIterator(const EnumMask<T> &enumMask, size_t index)
		: m_enumMask(&enumMask)
		, m_index(index)
	{
	}


	template<class T>
	EnumMask<T>::EnumMask()
	{
	}

	template<class T>
	template<size_t TCount>
	EnumMask<T>::EnumMask(const T(&items)[TCount])
	{
		Add(items);
	}

	template<class T>
	bool EnumMask<T>::Get(T item) const
	{
		return m_boolVector.Get(static_cast<size_t>(item));
	}

	template<class T>
	EnumMask<T> &EnumMask<T>::Set(T item, bool value) const
	{
		m_boolVector.Set(static_cast<size_t>(item), value);
		return *this;
	}

	template<class T>
	template<size_t TCount>
	EnumMask<T> &EnumMask<T>::Add(const T(&items)[TCount])
	{
		for (T item : items)
			m_boolVector.Set(static_cast<size_t>(item), true);

		return *this;
	}

	template<class T>
	template<size_t TCount>
	EnumMask<T> &EnumMask<T>::Remove(const T(&items)[TCount])
	{
		for (T item : items)
			m_boolVector.Set(static_cast<size_t>(item), false);

		return *this;
	}

	template<class T>
	bool EnumMask<T>::operator==(const EnumMask &other) const
	{
		return m_boolVector == other.m_boolVector;
	}

	template<class T>
	bool EnumMask<T>::operator!=(const EnumMask &other) const
	{
		return !((*this) == other);
	}

	template<class T>
	EnumMask<T> EnumMask<T>::operator|(const EnumMask &other) const
	{
		return EnumMask<T>(m_boolVector | other.m_boolVector);
	}

	template<class T>
	EnumMask<T> EnumMask<T>::operator&(const EnumMask &other) const
	{
		return EnumMask<T>(m_boolVector & other.m_boolVector);
	}

	template<class T>
	EnumMask<T> EnumMask<T>::operator^(const EnumMask &other) const
	{
		return EnumMask<T>(m_boolVector ^ other.m_boolVector);
	}

	template<class T>
	EnumMask<T> EnumMask<T>::operator~() const
	{
		return EnumMask<T>(~m_boolVector);
	}

	template<class T>
	EnumMaskIterator<T> EnumMask<T>::begin() const
	{
		size_t firstSet = 0;
		if (m_boolVector.FindFirstSet(firstSet))
			return EnumMaskIterator<T>(*this, firstSet);
		else
			return EnumMaskIterator<T>();
	}

	template<class T>
	EnumMaskIterator<T> EnumMask<T>::end() const
	{
		size_t lastSet = 0;
		if (m_boolVector.FindFirstSet(lastSet))
			return EnumMaskIterator<T>(*this, lastSet + 1u);
		else
			return EnumMaskIterator<T>();
	}

	template<class T>
	EnumMask<T>::EnumMask(const StaticBoolVector<kMax> &boolVector)
		: m_boolVector(boolVector)
	{
	}
}
