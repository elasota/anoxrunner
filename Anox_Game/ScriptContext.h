#pragma once

#include "rkit/Core/Opaque.h"
#include "rkit/Core/Result.h"

namespace anox::game
{
	class ScriptContextImpl;

	class ScriptContext final : public rkit::Opaque<ScriptContextImpl>
	{
	};
}
