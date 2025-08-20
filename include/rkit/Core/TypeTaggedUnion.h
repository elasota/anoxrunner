#pragma once

#include <stddef.h>
#include <new>

namespace rkit { namespace priv {
	struct TypeTaggedUnionHelper
	{
	};

	template<class... TTypes>
	union TypeTaggedUnionContainer
	{
	};

	template<class TType>
	union TypeTaggedUnionContainer<TType>
	{
		TType m_value;

		TypeTaggedUnionContainer();
		~TypeTaggedUnionContainer();
	};

	template<class TFirstType, class... TMoreTypes>
	union TypeTaggedUnionContainer<TFirstType, TMoreTypes...>
	{
		TFirstType m_value;
		TypeTaggedUnionContainer<TMoreTypes...> m_moreValues;

		TypeTaggedUnionContainer();
		~TypeTaggedUnionContainer();
	};

	template<class... TTypes>
	struct TypeTaggedUnionTypeGetter
	{
	};

	template<class TRequestedType>
	struct TypeTaggedUnionTypeGetter<TRequestedType, TRequestedType>
	{
		static TRequestedType &Get(TypeTaggedUnionContainer<TRequestedType> &container);
		static size_t GetIndex(const TypeTaggedUnionContainer<TRequestedType> &container);
	};

	template<class TRequestedType, class... TMoreTypes>
	struct TypeTaggedUnionTypeGetter<TRequestedType, TRequestedType, TMoreTypes...>
	{
		static TRequestedType &Get(TypeTaggedUnionContainer<TRequestedType, TMoreTypes...> &container);
		static size_t GetIndex(const TypeTaggedUnionContainer<TRequestedType, TMoreTypes...> &container);
	};

	template<class TRequestedType, class TWrongType, class... TMoreTypes>
	struct TypeTaggedUnionTypeGetter<TRequestedType, TWrongType, TMoreTypes...>
	{
		static TRequestedType &Get(TypeTaggedUnionContainer<TWrongType, TMoreTypes...> &container);
		static size_t GetIndex(const TypeTaggedUnionContainer<TWrongType, TMoreTypes...> &container);
	};

	template<class... TTypes>
	struct TypeTaggedUnionOperator
	{
	};

	template<class TType>
	struct TypeTaggedUnionOperator<TType>
	{
		template<class TPred>
		static bool Compare(size_t typeIndex, const TypeTaggedUnionContainer<TType> &a, const TypeTaggedUnionContainer<TType> &b, const TPred &pred);
		static void Destruct(size_t typeIndex, TypeTaggedUnionContainer<TType> &u);
		static void MoveAssign(size_t typeIndex, TypeTaggedUnionContainer<TType> &a, TypeTaggedUnionContainer<TType> &&b);
		static void MoveConstruct(size_t typeIndex, TypeTaggedUnionContainer<TType> &a, TypeTaggedUnionContainer<TType> &&b);
	};

	template<class TFirstType, class... TMoreTypes>
	struct TypeTaggedUnionOperator<TFirstType, TMoreTypes...>
	{
		template<class TPred>
		static bool Compare(size_t typeIndex, const TypeTaggedUnionContainer<TFirstType, TMoreTypes...> &a, const TypeTaggedUnionContainer<TFirstType, TMoreTypes...> &b, const TPred &pred);
		static void Destruct(size_t typeIndex, TypeTaggedUnionContainer<TFirstType, TMoreTypes...> &u);
		static void MoveAssign(size_t typeIndex, TypeTaggedUnionContainer<TFirstType, TMoreTypes...> &a, TypeTaggedUnionContainer<TFirstType, TMoreTypes...> &&b);
		static void MoveConstruct(size_t typeIndex, TypeTaggedUnionContainer<TFirstType, TMoreTypes...> &a, TypeTaggedUnionContainer<TFirstType, TMoreTypes...> &&b);
	};
} } // rkit::priv

#include "Algorithm.h"

namespace rkit { namespace priv {
	template<class TType>
	TypeTaggedUnionContainer<TType>::TypeTaggedUnionContainer()
		: m_value()
	{
	}

	template<class TType>
	TypeTaggedUnionContainer<TType>::~TypeTaggedUnionContainer()
	{
	}

	template<class TFirstType, class... TMoreTypes>
	TypeTaggedUnionContainer<TFirstType, TMoreTypes...>::TypeTaggedUnionContainer()
		: m_value()
	{
	}

	template<class TFirstType, class... TMoreTypes>
	TypeTaggedUnionContainer<TFirstType, TMoreTypes...>::~TypeTaggedUnionContainer()
	{
	}



	template<class TRequestedType>
	TRequestedType &TypeTaggedUnionTypeGetter<TRequestedType, TRequestedType>::Get(TypeTaggedUnionContainer<TRequestedType> &container)
	{
		return container.m_value;
	}

	template<class TRequestedType>
	size_t TypeTaggedUnionTypeGetter<TRequestedType, TRequestedType>::GetIndex(const TypeTaggedUnionContainer<TRequestedType> &container)
	{
		return 0;
	}

	template<class TRequestedType, class... TMoreTypes>
	TRequestedType &TypeTaggedUnionTypeGetter<TRequestedType, TRequestedType, TMoreTypes...>::Get(TypeTaggedUnionContainer<TRequestedType, TMoreTypes...> &container)
	{
		return container.m_value;
	}

	template<class TRequestedType, class... TMoreTypes>
	size_t TypeTaggedUnionTypeGetter<TRequestedType, TRequestedType, TMoreTypes...>::GetIndex(const TypeTaggedUnionContainer<TRequestedType, TMoreTypes...> &container)
	{
		return 0;
	}

	template<class TRequestedType, class TWrongType, class... TMoreTypes>
	TRequestedType &TypeTaggedUnionTypeGetter<TRequestedType, TWrongType, TMoreTypes...>::Get(TypeTaggedUnionContainer<TWrongType, TMoreTypes...> &container)
	{
		return TypeTaggedUnionTypeGetter<TRequestedType, TMoreTypes...>::Get(container.m_moreValues);
	}

	template<class TRequestedType, class TWrongType, class... TMoreTypes>
	size_t TypeTaggedUnionTypeGetter<TRequestedType, TWrongType, TMoreTypes...>::GetIndex(const TypeTaggedUnionContainer<TWrongType, TMoreTypes...> &container)
	{
		return TypeTaggedUnionTypeGetter<TRequestedType, TMoreTypes...>::GetIndex(container.m_moreValues) + 1;
	}

	template<class TType>
	template<class TPred>
	bool TypeTaggedUnionOperator<TType>::Compare(size_t typeIndex, const TypeTaggedUnionContainer<TType> &a, const TypeTaggedUnionContainer<TType> &b, const TPred &pred)
	{
		if (typeIndex == 0)
			return pred(a.m_value, b.m_value);

		return false;
	}

	template<class TType>
	void TypeTaggedUnionOperator<TType>::Destruct(size_t typeIndex, TypeTaggedUnionContainer<TType> &u)
	{
		if (typeIndex == 0)
			u.m_value.~TType();
	}

	template<class TType>
	void TypeTaggedUnionOperator<TType>::MoveAssign(size_t typeIndex, TypeTaggedUnionContainer<TType> &a, TypeTaggedUnionContainer<TType> &&b)
	{
		if (typeIndex == 0)
			a.m_value = std::move(b.m_value);
	}

	template<class TType>
	void TypeTaggedUnionOperator<TType>::MoveConstruct(size_t typeIndex, TypeTaggedUnionContainer<TType> &a, TypeTaggedUnionContainer<TType> &&b)
	{
		if (typeIndex == 0)
			new (&a.m_value) TType(std::move(b.m_value));
	}

	template<class TFirstType, class... TMoreTypes>
	template<class TPred>
	bool TypeTaggedUnionOperator<TFirstType, TMoreTypes...>::Compare(size_t typeIndex, const TypeTaggedUnionContainer<TFirstType, TMoreTypes...> &a, const TypeTaggedUnionContainer<TFirstType, TMoreTypes...> &b, const TPred &pred)
	{
		if (typeIndex == 0)
			return pred(a.m_value, b.m_value);
		else
			return TypeTaggedUnionOperator<TMoreTypes...>::Compare(typeIndex - 1, a.m_moreValues, b.m_moreValues, pred);
	}

	template<class TFirstType, class... TMoreTypes>
	void TypeTaggedUnionOperator<TFirstType, TMoreTypes...>::Destruct(size_t typeIndex, TypeTaggedUnionContainer<TFirstType, TMoreTypes...> &u)
	{
		if (typeIndex == 0)
			u.m_value.~TFirstType();
		else
			TypeTaggedUnionOperator<TMoreTypes...>::Destruct(typeIndex - 1, u.m_moreValues);
	}

	template<class TFirstType, class... TMoreTypes>
	void TypeTaggedUnionOperator<TFirstType, TMoreTypes...>::MoveAssign(size_t typeIndex, TypeTaggedUnionContainer<TFirstType, TMoreTypes...> &a, TypeTaggedUnionContainer<TFirstType, TMoreTypes...> &&b)
	{
		if (typeIndex == 0)
			a.m_value = std::move(b.m_value);
		else
			TypeTaggedUnionOperator<TMoreTypes...>::MoveAssign(typeIndex - 1, a.m_moreValues, std::move(b.m_moreValues));
	}

	template<class TFirstType, class... TMoreTypes>
	void TypeTaggedUnionOperator<TFirstType, TMoreTypes...>::MoveConstruct(size_t typeIndex, TypeTaggedUnionContainer<TFirstType, TMoreTypes...> &a, TypeTaggedUnionContainer<TFirstType, TMoreTypes...> &&b)
	{
		if (typeIndex == 0)
			new (&a.m_value) TFirstType(std::move(b.m_value));
		else
			TypeTaggedUnionOperator<TMoreTypes...>::MoveConstruct(typeIndex - 1, a.m_moreValues, std::move(b.m_moreValues));
	}
} }

namespace rkit
{
	template<class TEnum, class... TTypes>
	class TypeTaggedUnion
	{
	public:
		TypeTaggedUnion();
		TypeTaggedUnion(const TypeTaggedUnion &other);
		TypeTaggedUnion(TypeTaggedUnion &&other);
		~TypeTaggedUnion();

		template<class TOther>
		TypeTaggedUnion(const TOther &other);

		template<class TOther>
		TypeTaggedUnion(TOther &&other);

		TEnum GetType() const;

		template<class T>
		T &GetAs();

		template<class T>
		const T &GetAs() const;

		TypeTaggedUnion &operator=(const TypeTaggedUnion &other);
		TypeTaggedUnion &operator=(TypeTaggedUnion &&other);

		template<class TOther>
		TypeTaggedUnion &operator=(const TOther &other);

		template<class TOther>
		TypeTaggedUnion &operator=(TOther &&other);

		bool operator==(const TypeTaggedUnion &other) const;
		bool operator!=(const TypeTaggedUnion &other) const;
		bool operator<(const TypeTaggedUnion &other) const;
		bool operator<=(const TypeTaggedUnion &other) const;
		bool operator>(const TypeTaggedUnion &other) const;
		bool operator>=(const TypeTaggedUnion &other) const;

	private:
		void DestructUnion();

		template<class TPred>
		bool Compare(const TypeTaggedUnion &other) const;

		priv::TypeTaggedUnionContainer<TTypes...> m_value;
		TEnum m_type;
	};
}

namespace rkit
{
	template<class TEnum, class... TTypes>
	inline TypeTaggedUnion<TEnum, TTypes...>::TypeTaggedUnion()
		: m_type(static_cast<TEnum>(0))
		, m_value()
	{
	}

	template<class TEnum, class... TTypes>
	inline TypeTaggedUnion<TEnum, TTypes...>::TypeTaggedUnion(const TypeTaggedUnion &other)
		: m_type(static_cast<TEnum>(0))
		, m_value()
	{
		(*this) = other;
	}

	template<class TEnum, class... TTypes>
	inline TypeTaggedUnion<TEnum, TTypes...>::TypeTaggedUnion(TypeTaggedUnion &&other)
		: m_type(static_cast<TEnum>(0))
		, m_value()
	{
		(*this) = std::move(other);
	}

	template<class TEnum, class... TTypes>
	inline TypeTaggedUnion<TEnum, TTypes...>::~TypeTaggedUnion()
	{
		DestructUnion();
		m_value.~TypeTaggedUnionContainer();
	}

	template<class TEnum, class... TTypes>
	template<class TOther>
	inline TypeTaggedUnion<TEnum, TTypes...>::TypeTaggedUnion(const TOther &other)
		: m_type(static_cast<TEnum>(0))
		, m_value()
	{
		(*this) = other;
	}


	template<class TEnum, class... TTypes>
	template<class TOther>
	TypeTaggedUnion<TEnum, TTypes...>::TypeTaggedUnion(TOther &&other)
		: m_type(static_cast<TEnum>(0))
		, m_value()
	{
		(*this) = std::move(other);
	}

	template<class TEnum, class... TTypes>
	template<class TOther>
	TypeTaggedUnion<TEnum, TTypes...>::TypeTaggedUnion(TOther &&other);

	template<class TEnum, class... TTypes>
	TEnum TypeTaggedUnion<TEnum, TTypes...>::GetType() const
	{
		return m_type;
	}

	template<class TEnum, class... TTypes>
	template<class T>
	T &TypeTaggedUnion<TEnum, TTypes...>::GetAs()
	{
		return priv::TypeTaggedUnionTypeGetter<T, TTypes...>::Get(m_value);
	}

	template<class TEnum, class... TTypes>
	template<class T>
	const T &TypeTaggedUnion<TEnum, TTypes...>::GetAs() const
	{
		return const_cast<TypeTaggedUnion<TEnum, TTypes...> *>(this)->GetAs<T>();
	}

	template<class TEnum, class... TTypes>
	TypeTaggedUnion<TEnum, TTypes...> &TypeTaggedUnion<TEnum, TTypes...>::operator=(const TypeTaggedUnion &other)
	{
		(*this) = TypeTaggedUnion(other);
		return *this;
	}

	template<class TEnum, class... TTypes>
	TypeTaggedUnion<TEnum, TTypes...> &TypeTaggedUnion<TEnum, TTypes...>::operator=(TypeTaggedUnion &&other)
	{
		if (m_type == other.m_type)
			priv::TypeTaggedUnionOperator<TTypes...>::MoveAssign(static_cast<size_t>(m_type), m_value, std::move(other.m_value));
		else
		{
			// Have to move-construct in case the source is currently contained by this value
			TypeTaggedUnion temp;
			temp.DestructUnion();

			temp.m_type = other.m_type;
			priv::TypeTaggedUnionOperator<TTypes...>::MoveConstruct(static_cast<size_t>(temp.m_type), temp.m_value, std::move(other.m_value));

			DestructUnion();
			m_type = other.m_type;
			priv::TypeTaggedUnionOperator<TTypes...>::MoveConstruct(static_cast<size_t>(m_type), m_value, std::move(temp.m_value));
		}

		return *this;
	}

	template<class TEnum, class... TTypes>
	template<class TOther>
	TypeTaggedUnion<TEnum, TTypes...> &TypeTaggedUnion<TEnum, TTypes...>::operator=(const TOther &other)
	{
		(*this) = TOther(other);
	}

	template<class TEnum, class... TTypes>
	template<class TOther>
	TypeTaggedUnion<TEnum, TTypes...> &TypeTaggedUnion<TEnum, TTypes...>::operator=(TOther &&other)
	{
		const size_t expectedType = priv::TypeTaggedUnionTypeGetter<TOther, TTypes...>::GetIndex(m_value);
		if (static_cast<size_t>(m_type) == expectedType)
		{
			TOther &target = GetAs<TOther>();
			target = std::move(other);
		}
		else
		{
			// Have to move-construct in case the source is currently contained by this value
			TOther temp(std::move(other));

			DestructUnion();
			m_type = static_cast<TEnum>(expectedType);

			TOther &constructionSpace = GetAs<TOther>();
			new (&constructionSpace) TOther(std::move(temp));
		}

		return *this;
	}

	template<class TEnum, class... TTypes>
	bool TypeTaggedUnion<TEnum, TTypes...>::operator==(const TypeTaggedUnion &other) const
	{
		return Compare<DefaultCompareEqualPred>(other);
	}

	template<class TEnum, class... TTypes>
	bool TypeTaggedUnion<TEnum, TTypes...>::operator!=(const TypeTaggedUnion &other) const
	{
		return Compare<DefaultCompareNotEqualPred>(other);
	}

	template<class TEnum, class... TTypes>
	bool TypeTaggedUnion<TEnum, TTypes...>::operator<(const TypeTaggedUnion &other) const
	{
		return Compare<DefaultCompareLessPred>(other);
	}

	template<class TEnum, class... TTypes>
	bool TypeTaggedUnion<TEnum, TTypes...>::operator<=(const TypeTaggedUnion &other) const
	{
		return Compare<DefaultCompareLessEqualPred>(other);
	}

	template<class TEnum, class... TTypes>
	bool TypeTaggedUnion<TEnum, TTypes...>::operator>(const TypeTaggedUnion &other) const
	{
		return Compare<DefaultCompareGreaterPred>(other);
	}

	template<class TEnum, class... TTypes>
	bool TypeTaggedUnion<TEnum, TTypes...>::operator>=(const TypeTaggedUnion &other) const
	{
		return Compare<DefaultCompareGreaterEqualPred>(other);
	}

	template<class TEnum, class... TTypes>
	void TypeTaggedUnion<TEnum, TTypes...>::DestructUnion()
	{
		priv::TypeTaggedUnionOperator<TTypes...>::Destruct(static_cast<size_t>(m_type), m_value);
	}

	template<class TEnum, class... TTypes>
	template<class TPred>
	bool TypeTaggedUnion<TEnum, TTypes...>::Compare(const TypeTaggedUnion &other) const
	{
		if (m_type != other.m_type)
			return TPred()(m_type, other.m_type);
		
		priv::TypeTaggedUnionOperator<TTypes...>::Compare(static_cast<size_t>(m_type), m_value, other.m_value, TPred());

	}
}
