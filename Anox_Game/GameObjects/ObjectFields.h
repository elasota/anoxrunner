#pragma once

namespace anox::game::priv
{
	template<class T>
	struct ObjectRTTIResolver
	{
	};

	// This should only ever be used by codegen files!
	template<class T>
	struct ObjectFieldsImpl
	{
	};

	// This should only ever be used by codegen files!
	template<class T>
	struct ObjectRTTIImpl
	{
	};
}

namespace anox::game
{
	template<class T>
	struct ObjectFieldsBase
	{
	};

	template<class T>
	using ObjectFields = typename priv::ObjectRTTIResolver<T>::FieldType_t;

	template<class T>
	using ObjectRTTI = typename priv::ObjectRTTIResolver<T>::RTTIType_t;
}
