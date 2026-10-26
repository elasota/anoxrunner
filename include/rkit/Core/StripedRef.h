#pragma once

#include <cstddef>

#include "rkit/Core/Tuple.h"

namespace rkit::priv
{
	template<size_t TIndex, class TOperand, class... T>
	struct StripedRefOffsetHelper
	{
		static void Add(Tuple<T *...> &tuple, TOperand operand);
		static void Subtract(Tuple<T *...> &tuple, TOperand operand);
	};

	template<size_t TIndex, class... T>
	struct StripedRefConstHelper
	{
		static void MakeConst(Tuple<const T *...> &outTuple, const Tuple<T *...> &inTuple);
	};
}

namespace rkit
{
	template<class... T>
	class StripedRef
	{
	public:
		StripedRef();
		explicit StripedRef(const Tuple<T *...> &ptrTuple);

		StripedRef<T...> &operator++();
		StripedRef<T...> &operator--();

		StripedRef<T...> operator++(int);
		StripedRef<T...> operator--(int);

		StripedRef<T...> &operator+=(size_t sz);
		StripedRef<T...> &operator+=(ptrdiff_t diff);

		StripedRef<T...> &operator-=(size_t sz);
		StripedRef<T...> &operator-=(ptrdiff_t diff);

		ptrdiff_t operator-(const StripedRef<T...> &other) const;
		StripedRef<T...> operator-(size_t sz) const;
		StripedRef<T...> operator-(ptrdiff_t sz) const;

		StripedRef<T...> operator+(size_t sz) const;
		StripedRef<T...> operator+(ptrdiff_t sz) const;

		bool operator<(const StripedRef<T...> &other) const;
		bool operator==(const StripedRef<T...> &other) const;

		StripedRef<T...> &operator=(const StripedRef<T...> &other);

		operator StripedRef<const T...>() const;

		const Tuple<T*...> &operator*() const;

		template<size_t TIndex>
		typename priv::TupleTypeAtIndex<TIndex, T...>::Type_t &GetAt() const;

		template<class TDesiredType>
		TDesiredType &Get() const;

	private:
		Tuple<T *...> m_tuple;
	};
}

#include "RKitAssert.h"
#include "TypeList.h"

namespace rkit::priv
{
	template<size_t TIndex, class TOperand, class... T>
	void StripedRefOffsetHelper<TIndex, TOperand, T...>::Add(Tuple<T *...> &tuple, TOperand operand)
	{
		if constexpr (TIndex < TypeListSize<TypeList<T...>>::kValue)
		{
			tuple.GetAt<TIndex>() += operand;
			StripedRefOffsetHelper<TIndex + 1, TOperand, T...>::Add(tuple, operand);
		}
	}

	template<size_t TIndex, class TOperand, class... T>
	void StripedRefOffsetHelper<TIndex, TOperand, T...>::Subtract(Tuple<T *...> &tuple, TOperand operand)
	{
		if constexpr (TIndex < TypeListSize<TypeList<T...>>::kValue)
		{
			tuple.GetAt<TIndex>() -= operand;
			StripedRefOffsetHelper<TIndex + 1, TOperand, T...>::Subtract(tuple, operand);
		}
	}

	template<size_t TIndex, class... T>
	void StripedRefConstHelper<TIndex, T...>::MakeConst(Tuple<const T *...> &outTuple, const Tuple<T *...> &inTuple)
	{
		if constexpr (TIndex < TypeListSize<TypeList<T...>>::kValue)
		{
			outTuple.GetAt<TIndex>() = inTuple.GetAt<TIndex>();
			StripedRefConstHelper<TIndex + 1, T...>::MakeConst(outTuple, inTuple);
		}
	}
}

namespace rkit
{
	template<class... T>
	StripedRef<T...>::StripedRef()
		: m_tuple(static_cast<T *>(nullptr)...)
	{
	}

	template<class... T>
	StripedRef<T...>::StripedRef(const Tuple<T *...> &ptrTuple)
		: m_tuple(ptrTuple)
	{
	}

	template<class... T>
	StripedRef<T...> &StripedRef<T...>::operator++()
	{
		return ((*this) += static_cast<size_t>(1));
	}

	template<class... T>
	StripedRef<T...> &StripedRef<T...>::operator--()
	{
		return ((*this) -= static_cast<size_t>(1));
	}

	template<class... T>
	StripedRef<T...> StripedRef<T...>::operator++(int)
	{
		StripedRef<T...> clone = (*this);
		(*this) += 1;
		return clone;
	}

	template<class... T>
	StripedRef<T...> StripedRef<T...>::operator--(int)
	{
		StripedRef<T...> clone = (*this);
		(*this) -= 1;
		return clone;
	}

	template<class... T>
	StripedRef<T...> &StripedRef<T...>::operator+=(size_t sz)
	{
		priv::StripedRefOffsetHelper<0, size_t, T...>::Add(m_tuple, sz);
		return *this;
	}

	template<class... T>
	StripedRef<T...> &StripedRef<T...>::operator+=(ptrdiff_t diff)
	{
		priv::StripedRefOffsetHelper<0, ptrdiff_t, T...>::Add(m_tuple, diff);
		return *this;
	}

	template<class... T>
	StripedRef<T...> &StripedRef<T...>::operator-=(size_t sz)
	{
		priv::StripedRefOffsetHelper<0, size_t, T...>::Subtract(m_tuple, sz);
		return *this;
	}

	template<class... T>
	StripedRef<T...> &StripedRef<T...>::operator-=(ptrdiff_t diff)
	{
		priv::StripedRefOffsetHelper<0, ptrdiff_t, T...>::Subtract(m_tuple, diff);
		return *this;
	}

	template<class... T>
	ptrdiff_t StripedRef<T...>::operator-(const StripedRef<T...> &other) const
	{
		return this->m_tuple.GetAt<0>() - other.m_tuple.GetAt<0>();
	}

	template<class... T>
	StripedRef<T...> StripedRef<T...>::operator-(size_t sz) const
	{
		StripedRef<T...> result = (*this);
		result -= sz;
		return result;
	}

	template<class... T>
	StripedRef<T...> StripedRef<T...>::operator-(ptrdiff_t sz) const
	{
		StripedRef<T...> result = (*this);
		result -= sz;
		return result;
	}

	template<class... T>
	StripedRef<T...> StripedRef<T...>::operator+(size_t sz) const
	{
		StripedRef<T...> result = (*this);
		result += sz;
		return result;
	}

	template<class... T>
	StripedRef<T...> StripedRef<T...>::operator+(ptrdiff_t sz) const
	{
		StripedRef<T...> result = (*this);
		result += sz;
		return result;
	}

	template<class... T>
	bool StripedRef<T...>::operator<(const StripedRef<T...> &other) const
	{
		return this->m_tuple.GetAt<0>() < other.m_tuple.GetAt<0>();
	}

	template<class... T>
	bool StripedRef<T...>::operator==(const StripedRef<T...> &other) const
	{
		return this->m_tuple.GetAt<0>() < other.m_tuple.GetAt<0>();
	}

	template<class... T>
	StripedRef<T...> &StripedRef<T...>::operator=(const StripedRef<T...> &other)
	{
		m_tuple = other.m_tuple;
		return *this;
	}

	template<class... T>
	StripedRef<T...>::operator StripedRef<const T...>() const
	{
		Tuple<const T*...> result;
		priv::StripedRefConstHelper<0, T...>::MakeConst(result, m_tuple);
		return StripedRef<const T...>(result);
	}

	template<class... T>
	const Tuple<T*...> &StripedRef<T...>::operator*() const
	{
		return m_tuple;
	}

	template<class... T>
	template<size_t TIndex>
	typename priv::TupleTypeAtIndex<TIndex, T...>::Type_t &StripedRef<T...>::GetAt() const
	{
		return *m_tuple.template GetAt<TIndex>();
	}

	template<class... T>
	template<class TDesiredType>
	TDesiredType &StripedRef<T...>::Get() const
	{
		return *m_tuple.template GetUnique<TDesiredType *>();
	}
}
