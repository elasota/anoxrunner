#pragma once

#include "rkit/Sandbox/SandboxSysCall.h"

#include <stdint.h>
#include <type_traits>

namespace rkit::sandbox::io::priv
{

	template<class T, bool TIsIntegral, bool TIsUnsigned, bool TIsFloat>
	struct SandboxNumberIOHelper
	{
	};

	template<class T>
	struct SandboxNumberIOHelper<T, true, true, false>
	{
		static void StoreValue(T &outValue, Value_t inValue);
		static Value_t LoadValue(T inValue);
	};

	template<class T>
	struct SandboxNumberIOHelper<T, true, false, false>
	{
		static void StoreValue(T &outValue, Value_t inValue);
		static Value_t LoadValue(T inValue);
	};

	template<class T>
	struct SandboxIOHelper : public SandboxNumberIOHelper<T, std::is_integral<T>::value, std::is_unsigned<T>::value, std::is_floating_point<T>::value>
	{
	};

	template<class T>
	struct SandboxIOHelper<T *>
	{
		static void StoreValue(T *&outValue, Value_t inValue);
		static Value_t LoadValue(T *inValue);
	};

	template<>
	struct SandboxIOHelper<double>
	{
		static void StoreValue(double &outValue, Value_t inValue);
		static Value_t LoadValue(double inValue);
	};

	template<>
	struct SandboxIOHelper<float>
	{
		static void StoreValue(float &outValue, Value_t inValue);
		static Value_t LoadValue(float inValue);
	};
}

namespace rkit::sandbox::io
{
	typedef uint64_t Value_t;

	template<class T>
	inline void StoreValue(T &outValue, Value_t inValue)
	{
		priv::SandboxIOHelper<T>::StoreValue(outValue, inValue);
	}

	template<class T>
	Value_t LoadValue(T inValue)
	{
		return priv::SandboxIOHelper<T>::LoadValue(inValue);
	}

	void CriticalError(ISandbox *sandbox, Environment *env, CriticalErrorFunc_t criticalError) noexcept;
	void SysCall(ISandbox *sandbox, Environment *env, const SysCallDispatchFunc_t *sysCalls, uint32_t sysCallID, PackedResultAndExtCode &outResult, Value_t *values) noexcept;
	Value_t LoadParam(void *ioContext, uint32_t paramIndex) noexcept;
	void StoreResult(void *ioContext, PackedResultAndExtCode result) noexcept;
	void StoreReturnValue(void *ioContext, uint32_t index, Value_t value) noexcept;
}

#include <string.h>
#include <limits>

namespace rkit::sandbox::io::priv
{
	// Unsigned ints
	template<class T>
	void SandboxNumberIOHelper<T, true, true, false>::StoreValue(T &outValue, Value_t inValue)
	{
		outValue = static_cast<T>(inValue & std::numeric_limits<T>::max());
	}

	template<class T>
	Value_t SandboxNumberIOHelper<T, true, true, false>::LoadValue(T inValue)
	{
		return inValue;
	}

	// Signed ints
	template<class T>
	void SandboxNumberIOHelper<T, true, false, false>::StoreValue(T &outValue, Value_t inValue)
	{
		typedef typename std::make_unsigned<T>::type UnsignedType_t;

		UnsignedType_t temp = 0;
		SandboxNumberIOHelper<UnsignedType_t, true, true, false>::StoreValue(temp, inValue);

		outValue = static_cast<T>(temp);
	}

	template<class T>
	Value_t SandboxNumberIOHelper<T, true, false, false>::LoadValue(T inValue)
	{
		typedef typename std::make_unsigned<T>::type UnsignedType_t;

		return static_cast<UnsignedType_t>(inValue);
	}

	template<class T>
	void SandboxIOHelper<T *>::StoreValue(T *&outValue, Value_t inValue)
	{
		uintptr_t temp = 0;
		SandboxIOHelper<uintptr_t>::StoreValue(temp, inValue);
		outValue = reinterpret_cast<T *>(temp);
	}

	template<class T>
	Value_t SandboxIOHelper<T *>::LoadValue(T *inValue)
	{
		return reinterpret_cast<uintptr_t>(inValue);
	}

	inline void SandboxIOHelper<double>::StoreValue(double &outValue, Value_t inValue)
	{
		uint64_t temp = 0;
		SandboxIOHelper<uint64_t>::StoreValue(temp, inValue);
		memcpy(&outValue, &temp, 8);
	}

	inline Value_t SandboxIOHelper<double>::LoadValue(double inValue)
	{
		uint64_t temp = 0;
		memcpy(&temp, &inValue, 8);
		return temp;
	}

	inline void SandboxIOHelper<float>::StoreValue(float &outValue, Value_t inValue)
	{
		uint32_t temp = 0;
		SandboxIOHelper<uint32_t>::StoreValue(temp, inValue);
		memcpy(&outValue, &temp, 4);
	}

	inline Value_t SandboxIOHelper<float>::LoadValue(float inValue)
	{
		uint32_t temp = 0;
		memcpy(&temp, &inValue, 4);
		return temp;
	}
}
