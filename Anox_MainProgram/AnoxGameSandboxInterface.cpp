#include "AnoxGameSandboxInterface.h"

#include "rkit/Sandbox/Sandbox.h"
#include "rkit/Core/NewDelete.h"

namespace anox
{
	class AnoxGameSandboxInterfaceImpl final : public rkit::OpaqueImplementation<AnoxGameSandboxInterface>
	{
	public:
		friend class AnoxGameSandboxInterface;

		AnoxGameSandboxInterfaceImpl(rkit::UniquePtr<rkit::ISandbox> &&sandbox);

	private:
		rkit::UniquePtr<rkit::ISandbox> m_sandbox;
	};

	AnoxGameSandboxInterfaceImpl::AnoxGameSandboxInterfaceImpl(rkit::UniquePtr<rkit::ISandbox> &&sandbox)
		: m_sandbox(std::move(sandbox))
	{
	}

	AnoxGameSandboxInterface::AnoxGameSandboxInterface(rkit::UniquePtr<rkit::ISandbox> &&sandbox)
		: Opaque<AnoxGameSandboxInterfaceImpl>(std::move(sandbox))
	{
	}

	rkit::Result AnoxGameSandboxInterface::LinkToSandbox()
	{
		const rkit::sandbox::Address_t entryDescriptorAddr = this->Impl().m_sandbox->GetEntryDescriptor();

		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxGameSandboxInterface::Create(rkit::UniquePtr<AnoxGameSandboxInterface> &outSandboxInterface, rkit::UniquePtr<rkit::ISandbox> &&sandbox)
	{
		return rkit::New<AnoxGameSandboxInterface>(outSandboxInterface, std::move(sandbox));
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::AnoxGameSandboxInterfaceImpl)
