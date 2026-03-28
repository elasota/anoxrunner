#pragma once

#include "DynamicObject.h"

#include "rkit/Core/Result.h"

#define ANOX_SERIALIZABLE_PROTOTYPE	\
public:\
rkit::Result Serialize(::anox::game::IWorldObjectSerializer &serializer);

namespace anox::game
{
	struct IWorldObjectSerializer;

	class SerializableBase : public DynamicObject
	{
		ANOX_RTTI_CLASS_NO_BASE(SerializableBase)

	protected:
		struct VTable
		{
			rkit::Result(*m_serializeFunc)(SerializableBase *object, IWorldObjectSerializer &serializer);
		};

		SerializableBase() = delete;
		explicit SerializableBase(const VTable &vtable);

	private:
		const VTable &m_vtable;
	};

	template<class TType>
	class Serializable : public SerializableBase
	{
	public:
		Serializable();

	private:
		static rkit::Result StaticSerialize(SerializableBase *object, IWorldObjectSerializer &serializer);

		static const typename SerializableBase::VTable ms_vtable;
	};
}

namespace anox::game
{
	template<class TType>
	Serializable<TType>::Serializable()
		: SerializableBase(ms_vtable)
	{
		static_cast<void>(static_cast<TType *>(this));
	}

	template<class TType>
	rkit::Result Serializable<TType>::StaticSerialize(SerializableBase *object, IWorldObjectSerializer &serializer)
	{
		//return static_cast<TType *>(static_cast<Serializable<TType> *>(object))->Serialize(serializer);
	}

	template<class TType>
	const typename SerializableBase::VTable Serializable<TType>::ms_vtable =
	{
		Serializable<TType>::StaticSerialize,
	};
}
