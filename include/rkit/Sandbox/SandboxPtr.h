#pragma once

#include "Sandbox.h"

namespace rkit::sandbox
{
	class SandboxPtrBase
	{
	public:
		explicit SandboxPtrBase(Address_t sandboxAddress);

		Address_t GetSandboxAddress() const;

	protected:
		SandboxPtrBase() = delete;

		Address_t m_address = 0;
	};

	template<class T>
	class SandboxPtr : public SandboxPtrBase
	{
	public:
		SandboxPtr();

#ifdef RKIT_IFDEF_IS_IN_SANDBOX
		T *Get();
#endif
	};
}
