#include "anox/Sandbox/AnoxGame.host.generated.inl"

#include "AnoxGameSandboxEnv.h"

#include "rkit/Sandbox/Sandbox.h"

namespace anox::game::sandbox
{
	::rkit::Result HostExports::MemAlloc(::rkit::sandbox::Environment &env, ::rkit::sandbox::IThreadContext *thread, ::rkit::sandbox::Address_t &ptr, uint32_t &mmid, ::rkit::sandbox::Address_t size)
	{
		if (size == 0 || size > std::numeric_limits<size_t>::max())
		{
			ptr = 0;
			RKIT_RETURN_OK;
		}

		return static_cast<AnoxGameSandboxEnvironment &>(env).m_sandbox->AllocDynamicMemory(ptr, mmid, size);
	}

	::rkit::Result HostExports::MemFree(::rkit::sandbox::Environment &env, ::rkit::sandbox::IThreadContext *thread, uint32_t mmid)
	{
		return static_cast<AnoxGameSandboxEnvironment &>(env).m_sandbox->ReleaseDynamicMemory(mmid);
	}
}

