#pragma once

#include "rkit/Sandbox/Sandbox.h"

namespace anox::game
{
	struct AnoxGameSandboxEnvironment : public rkit::sandbox::Environment
	{
		rkit::ISandbox *m_sandbox = nullptr;
	};
}
