#pragma once

#include "rkit/Core/Opaque.h"
#include "rkit/Core/Result.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct ISandbox;
}

namespace anox
{
	class AnoxGameSandboxInterfaceImpl;

	class AnoxGameSandboxInterface final : public rkit::Opaque<AnoxGameSandboxInterfaceImpl>
	{
	public:
		explicit AnoxGameSandboxInterface(rkit::UniquePtr<rkit::ISandbox> &&sandbox);

		rkit::Result LinkToSandbox();

		static rkit::Result Create(rkit::UniquePtr<AnoxGameSandboxInterface> &outSandboxInterface, rkit::UniquePtr<rkit::ISandbox> &&sandbox);
	};
}
