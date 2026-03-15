#pragma once

#include "rkit/Core/RefCounted.h"
#include "rkit/Sandbox/Sandbox.h"

namespace anox
{
	class AnoxSpawnDefsResourceBase;
}

namespace anox::game
{
	struct AnoxGameSandboxEnvironment : public rkit::sandbox::Environment
	{
		rkit::ISandbox *m_sandbox = nullptr;
		rkit::RCPtr<AnoxSpawnDefsResourceBase> m_spawnDefs;
		rkit::sandbox::Address_t m_gameSessionAddr = 0;
	};
}
