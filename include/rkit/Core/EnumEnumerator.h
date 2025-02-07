#include <cstddef>

namespace rkit
{
	template<class T>
	class EnumEnumerator;

	template<class T>
	class EnumEnumeratorIterator
	{
	public:
		friend class EnumEnumerator<T>;

		EnumEnumeratorIterator();

		EnumEnumeratorIterator operator++(int);
		EnumEnumeratorIterator &operator++();

		EnumEnumeratorIterator operator--(int);
		EnumEnumeratorIterator &operator--();

		bool operator==(const EnumEnumeratorIterator<T> &other) const;
		bool operator!=(const EnumEnumeratorIterator<T> &other) const;

		T operator*() const;

	private:
		explicit EnumEnumeratorIterator(size_t index);

		size_t m_index;
	};

	template<class T>
	class EnumEnumerator
	{
	public:
		friend class EnumEnumeratorIterator<T>;

		EnumEnumeratorIterator<T> begin() const;
		EnumEnumeratorIterator<T> end() const;
	};
}

namespace rkit
{
	template<class T>
	EnumEnumeratorIterator<T>::EnumEnumeratorIterator()
		: m_index(0)
	{
	}

	template<class T>
	EnumEnumeratorIterator<T> EnumEnumeratorIterator<T>::operator++(int)
	{
		EnumEnumeratorIterator result = (*this);
		++m_index;
		return result;
	}

	template<class T>
	EnumEnumeratorIterator<T> &EnumEnumeratorIterator<T>::operator++()
	{
		++m_index;
		return *this;
	}

	template<class T>
	EnumEnumeratorIterator<T> EnumEnumeratorIterator<T>::operator--(int)
	{
		EnumEnumeratorIterator<T> result = (*this);
		--m_index;
		return result;
	}

	template<class T>
	EnumEnumeratorIterator<T> &EnumEnumeratorIterator<T>::operator--()
	{
		--m_index;
		return *this;
	}

	template<class T>
	bool EnumEnumeratorIterator<T>::operator==(const EnumEnumeratorIterator<T> &other) const
	{
		return m_index == other.m_index;
	}

	template<class T>
	bool EnumEnumeratorIterator<T>::operator!=(const EnumEnumeratorIterator<T> &other) const
	{
		return !((*this) == other);
	}

	template<class T>
	T EnumEnumeratorIterator<T>::operator*() const
	{
		return static_cast<T>(m_index);
	}

	template<class T>
	EnumEnumeratorIterator<T>::EnumEnumeratorIterator(size_t index)
		: m_index(index)
	{
	}


	template<class T>
	EnumEnumeratorIterator<T> EnumEnumerator<T>::begin() const
	{
		return EnumEnumeratorIterator<T>(0);
	}

	template<class T>
	EnumEnumeratorIterator<T> EnumEnumerator<T>::end() const
	{
		return EnumEnumeratorIterator<T>(static_cast<size_t>(T::kCount));
	}
}
