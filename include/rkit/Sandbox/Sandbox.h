#pragma once

#include "rkit/Core/Result.h"
#include "rkit/Core/StringProto.h"
#include "rkit/Sandbox/SandboxAddress.h"
#include "rkit/Sandbox/SandboxSysCall.h"

#include <stddef.h>

namespace rkit
{
	template<class T>
	class Span;

	template<class T>
	class UniquePtr;
}

namespace rkit::sandbox
{
	struct IThreadContext;
	struct ThreadCreationParameters;
	struct Environment;
}

namespace rkit::sandbox
{
	struct Environment
	{
		virtual ~Environment() {}
	};

	struct IThreadContext
	{
		virtual ~IThreadContext() {}
	};
}

namespace rkit
{
	struct ISandbox
	{
		virtual ~ISandbox() {}

		virtual bool TryAccessMemoryRange(void *&outPtr, sandbox::Address_t address, sandbox::Address_t size) = 0;
		virtual Result CallFunction(sandbox::Address_t address, sandbox::io::Value_t *ioValues, size_t numReturnValues, size_t numParameters) = 0;
		virtual Result CreateThreadContext(UniquePtr<sandbox::IThreadContext> &outThreadContext, const sandbox::ThreadCreationParameters &threadParams) = 0;

		virtual Result AllocDynamicMemory(sandbox::Address_t &outAddress, uint32_t &outMMID, size_t size) = 0;
		virtual Result ReleaseDynamicMemory(uint32_t mmid) = 0;

		virtual Result RunInitializer(sandbox::IThreadContext &threadContext) = 0;

		virtual sandbox::Address_t GetEntryDescriptor() const = 0;

		Result AccessMemoryRange(void *&outPtr, sandbox::Address_t address, sandbox::Address_t size);
		Result AccessMemoryRangeWithAlignment(void *&outPtr, sandbox::Address_t address, sandbox::Address_t size, sandbox::Address_t alignment);

		template<class TType>
		Result AccessMemoryValue(TType *&outPtr, sandbox::Address_t address);

		template<class TType>
		Result AccessMemorySpan(Span<TType> &outSpan, sandbox::Address_t address, size_t count);

		template<class TType>
		Result AccessMemoryArray(TType *&outPtr, sandbox::Address_t address, size_t count);

		bool TryAccessMemoryRangeWithAlignment(void *&outPtr, sandbox::Address_t address, sandbox::Address_t size, sandbox::Address_t alignment);

		template<class TType>
		bool TryAccessMemoryValue(TType *&outPtr, sandbox::Address_t address);

		template<class TType>
		bool TryAccessMemorySpan(Span<TType> &outSpan, sandbox::Address_t address, size_t count);

		template<class TType>
		bool TryAccessMemoryArray(TType *&outPtr, sandbox::Address_t address, size_t count);
	};
}

namespace rkit
{
	inline Result ISandbox::AccessMemoryRange(void *&outPtr, sandbox::Address_t address, sandbox::Address_t size)
	{
		if (!this->TryAccessMemoryRange(outPtr, address, size))
			RKIT_THROW(rkit::ResultCode::kSandboxMemoryError);

		RKIT_RETURN_OK;
	}


	inline Result ISandbox::AccessMemoryRangeWithAlignment(void *&outPtr, sandbox::Address_t address, sandbox::Address_t size, sandbox::Address_t alignment)
	{
		if ((address & (alignment - 1)) != 0)
			RKIT_THROW(rkit::ResultCode::kSandboxMemoryError);

		if (!this->TryAccessMemoryRange(outPtr, address, size))
			RKIT_THROW(rkit::ResultCode::kSandboxMemoryError);

		RKIT_RETURN_OK;
	}

	template<class TType>
	inline Result ISandbox::AccessMemoryValue(TType *&outPtr, sandbox::Address_t address)
	{
		return this->AccessMemoryArray<TType>(outPtr, address, 1);
	}

	template<class TType>
	inline Result ISandbox::AccessMemoryArray(TType *&outPtr, sandbox::Address_t address, size_t count)
	{
		if (!TryAccessMemoryArray(outPtr, address, count))
			RKIT_THROW(rkit::ResultCode::kSandboxMemoryError);

		RKIT_RETURN_OK;
	}

	template<class TType>
	inline Result ISandbox::AccessMemorySpan(Span<TType> &outSpan, sandbox::Address_t address, size_t count)
	{
		if (!TryAccessMemorySpan(outSpan, address, count))
			RKIT_THROW(rkit::ResultCode::kSandboxMemoryError);

		RKIT_RETURN_OK;
	}

	inline bool ISandbox::TryAccessMemoryRangeWithAlignment(void *&outPtr, sandbox::Address_t address, sandbox::Address_t size, sandbox::Address_t alignment)
	{
		if ((address & (alignment - 1)) != 0)
			return false;

		return this->TryAccessMemoryRange(outPtr, address, size);
	}

	template<class TType>
	inline bool ISandbox::TryAccessMemoryValue(TType *&outPtr, sandbox::Address_t address)
	{
		return this->TryAccessMemoryArray<TType>(outPtr, address, 1);
	}

	template<class TType>
	inline bool ISandbox::TryAccessMemoryArray(TType *&outPtr, sandbox::Address_t address, size_t count)
	{
		const sandbox::Address_t maxCount = std::numeric_limits<sandbox::Address_t>::max() / sizeof(TType);
		if (count > maxCount)
			return false;

		void *ptr = nullptr;
		if (!this->TryAccessMemoryRangeWithAlignment(ptr, address, count * sizeof(TType), alignof(TType)))
			return false;

		outPtr = static_cast<TType *>(ptr);

		return true;
	}

	template<class TType>
	inline bool ISandbox::TryAccessMemorySpan(Span<TType> &outSpan, sandbox::Address_t address, size_t count)
	{
		TType *ptr = nullptr;
		if (!TryAccessMemoryArray(ptr, address, count))
			return false;

		outSpan = Span<TType>(ptr, count);

		return true;
	}
}
