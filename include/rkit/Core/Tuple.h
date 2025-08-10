#pragma once

#include <cstddef>

#include "TupleProtos.h"

namespace rkit
{
	namespace priv
	{
		template<size_t TIndex, class... TTypes>
		struct TupleTypeAtIndex
		{
		};

		template<class TFirstType, class... TMoreTypes>
		struct TupleTypeAtIndex<0, TFirstType, TMoreTypes...>
		{
			typedef TFirstType Type_t;
		};

		template<class TType>
		struct TupleTypeAtIndex<0, TType>
		{
			typedef TType Type_t;
		};

		template<size_t TIndex, class TFirstType, class... TMoreTypes>
		struct TupleTypeAtIndex<TIndex, TFirstType, TMoreTypes...>
		{
			typedef typename TupleTypeAtIndex<TIndex - 1, TMoreTypes...>::Type_t Type_t;
		};

		template<size_t TIndex, class TFirstType, class... TMoreTypes>
		struct TupleGetAtHelper
		{
			static typename TupleTypeAtIndex<TIndex - 1, TMoreTypes...>::Type_t &GetMutable(TFirstType &firstValue, Tuple<TMoreTypes...> &moreValues);
			static const typename TupleTypeAtIndex<TIndex - 1, TMoreTypes...>::Type_t &GetConst(const TFirstType &firstValue, const Tuple<TMoreTypes...> &moreValues);
		};

		template<class TFirstType, class... TMoreTypes>
		struct TupleGetAtHelper<0, TFirstType, TMoreTypes...>
		{
			static TFirstType &GetMutable(TFirstType &firstValue, Tuple<TMoreTypes...> &moreValues);
			static const TFirstType &GetConst(const TFirstType &firstValue, const Tuple<TMoreTypes...> &moreValues);
		};
	}

	template<class... TTypes>
	class Tuple
	{
	};

	template<class T>
	class Tuple<T>
	{
	public:
		Tuple();
		Tuple(Tuple<T> &&other);
		Tuple(const Tuple<T> &other);
		explicit Tuple(const T &value);
		explicit Tuple(T &&value);

		Tuple &operator=(Tuple<T> &&other);
		Tuple &operator=(const Tuple<T> &other);

		template<size_t TIndex>
		T &GetAt();

		template<size_t TIndex>
		const T &GetAt() const;

	private:
		T m_value;
	};

	template<class TFirst, class... TMore>
	class Tuple<TFirst, TMore...>
	{
	public:
		Tuple();
		Tuple(Tuple<TFirst, TMore...> &&other);
		Tuple(const Tuple<TFirst, TMore...> &other);

		template<class TFirstArg, class... TMoreArgs>
		explicit Tuple(TFirstArg firstArg, TMoreArgs... moreArgs);

		Tuple<TFirst, TMore...> &operator=(Tuple<TFirst, TMore...> &&other);
		Tuple<TFirst, TMore...> &operator=(const Tuple<TFirst, TMore...> &other);

		template<size_t TIndex>
		typename priv::TupleTypeAtIndex<TIndex, TFirst, TMore...>::Type_t &GetAt();

		template<size_t TIndex>
		const typename priv::TupleTypeAtIndex<TIndex, TFirst, TMore...>::Type_t &GetAt() const;


	private:
		TFirst m_first;
		Tuple<TMore...> m_more;
	};
}

namespace rkit { namespace priv
{
	template<size_t TIndex, class TFirstType, class... TMoreTypes>
	typename TupleTypeAtIndex<TIndex - 1, TMoreTypes...>::Type_t &TupleGetAtHelper<TIndex, TFirstType, TMoreTypes...>::GetMutable(TFirstType &firstValue, Tuple<TMoreTypes...> &moreValues)
	{
		return moreValues.GetAt<TIndex - 1>();
	}

	template<size_t TIndex, class TFirstType, class... TMoreTypes>
	const typename TupleTypeAtIndex<TIndex - 1, TMoreTypes...>::Type_t &TupleGetAtHelper<TIndex, TFirstType, TMoreTypes...>::GetConst(const TFirstType &firstValue, const Tuple<TMoreTypes...> &moreValues)
	{
		return moreValues.GetAt<TIndex - 1>();
	}

	template<class TFirstType, class... TMoreTypes>
	TFirstType &TupleGetAtHelper<0, TFirstType, TMoreTypes...>::GetMutable(TFirstType &firstValue, Tuple<TMoreTypes...> &moreValues)
	{
		return firstValue;
	}

	template<class TFirstType, class... TMoreTypes>
	const TFirstType &TupleGetAtHelper<0, TFirstType, TMoreTypes...>::GetConst(const TFirstType &firstValue, const Tuple<TMoreTypes...> &moreValues)
	{
		return firstValue;
	}
} } // rkit::priv

namespace rkit
{
	template<class T>
	inline Tuple<T>::Tuple()
	{
	}

	template<class T>
	Tuple<T>::Tuple(Tuple<T> &&other)
		: m_value(std::move(other.m_value))
	{
	}

	template<class T>
	Tuple<T>::Tuple(const Tuple<T> &other)
		: m_value(other.m_value)
	{
	}

	template<class T>
	Tuple<T>::Tuple(const T &value)
		: m_value(value)
	{
	}

	template<class T>
	Tuple<T>::Tuple(T &&value)
		: m_value(std::move(value))
	{
	}

	template<class T>
	Tuple<T> &Tuple<T>::operator=(Tuple<T> &&other)
	{
		m_value = std::move(other.m_value);
		return *this;
	}

	template<class T>
	Tuple<T> &Tuple<T>::operator=(const Tuple<T> &other)
	{
		m_value = other;
		return *this;
	}

	template<class T>
	template<size_t TIndex>
	T &Tuple<T>::GetAt()
	{
		static_assert(TIndex == 0);
		return m_value;
	}

	template<class T>
	template<size_t TIndex>
	const T &Tuple<T>::GetAt() const
	{
		static_assert(TIndex == 0);
		return m_value;
	}

	// 2+ tuple
	template<class TFirst, class... TMore>
	Tuple<TFirst, TMore...>::Tuple()
	{
	}

	template<class TFirst, class... TMore>
	Tuple<TFirst, TMore...>::Tuple(Tuple<TFirst, TMore...> &&other)
		: m_first(std::move(other.m_first))
		, m_more(std::move(other.m_more))
	{
	}

	template<class TFirst, class... TMore>
	Tuple<TFirst, TMore...>::Tuple(const Tuple<TFirst, TMore...> &other)
		: m_first(other.m_first)
		, m_more(other.m_more)
	{
	}

	template<class TFirst, class... TMore>
	template<class TFirstArg, class... TMoreArgs>
	Tuple<TFirst, TMore...>::Tuple(TFirstArg firstArg, TMoreArgs... moreArgs)
		: m_first(std::forward<TFirstArg>(firstArg))
		, m_more(std::forward<TMoreArgs>(moreArgs)...)
	{
	}

	template<class TFirst, class... TMore>
	Tuple<TFirst, TMore...> &Tuple<TFirst, TMore...>::operator=(Tuple<TFirst, TMore...> &&other)
	{
		m_first = std::move(other.m_first);
		m_more = std::move(other.m_more);
		return *this;
	}

	template<class TFirst, class... TMore>
	Tuple<TFirst, TMore...> &Tuple<TFirst, TMore...>::operator=(const Tuple<TFirst, TMore...> &other)
	{
		m_first = other.m_first;
		m_more = other.m_more;
		return *this;
	}

	template<class TFirst, class... TMore>
	template<size_t TIndex>
	typename priv::TupleTypeAtIndex<TIndex, TFirst, TMore...>::Type_t &Tuple<TFirst, TMore...>::GetAt()
	{
		return priv::TupleGetAtHelper<TIndex, TFirst, TMore...>::GetMutable(m_first, m_more);
	}

	template<class TFirst, class... TMore>
	template<size_t TIndex>
	const typename priv::TupleTypeAtIndex<TIndex, TFirst, TMore...>::Type_t &Tuple<TFirst, TMore...>::GetAt() const
	{
		return priv::TupleGetAtHelper<TIndex, TFirst, TMore...>::GetConst(m_first, m_more);
	}
}
