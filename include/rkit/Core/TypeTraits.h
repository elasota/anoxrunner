#pragma once

namespace rkit
{
	template<class TTypeA, class TTypeB>
	struct IsSameType
	{
		static const bool kValue = false;
	};

	template<class TType>
	struct IsSameType<TType, TType>
	{
		static const bool kValue = true;
	};

	template<class TType>
	struct RemoveConst
	{
		typedef TType Type_t;
	};

	template<class TType>
	struct RemoveConst<const TType>
	{
		typedef TType Type_t;
	};

	template<class TType>
	struct RemoveVolatile
	{
		typedef TType Type_t;
	};

	template<class TType>
	struct RemoveVolatile<volatile TType>
	{
		typedef TType Type_t;
	};

	template<class TType>
	struct RemoveConstVolatile : public RemoveConst<typename RemoveVolatile<TType>::Type_t>
	{
	};

	template<class TType>
	struct RemoveRef
	{
		typedef TType Type_t;
	};

	template<class TType>
	struct RemoveRef<TType&>
	{
		typedef TType Type_t;
	};
}
