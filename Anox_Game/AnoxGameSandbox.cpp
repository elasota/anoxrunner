#include "anox/Sandbox/AnoxGame.sb.generated.inl"

#include "anox/AnoxModule.h"

#include "rkit/Sandbox/SandboxModuleImpl.h"

namespace anox::game
{
	RKIT_SANDBOX_DEFINE_MODULE_CLASS(anox::game::sandbox::SandboxAPI, AnoxGameSandboxModule)
}

RKIT_IMPLEMENT_MODULE(Anox, Game, anox::game::AnoxGameSandboxModule)

RKIT_SANDBOX_IMPLEMENT_MODULE_GLOBALS


namespace anox::game::sandbox
{
	::rkit::Result SandboxExports::Whatever(int32_t &c, int32_t &d, int32_t a, int32_t b)
	{
		c = a + 4;
		d = b + 5;

		RKIT_RETURN_OK;
	}
}
