#pragma once

#include "rkit/Core/Result.h"
#include "rkit/Core/Opaque.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	template<class T>
	class Span;
}

namespace anox::game
{
	class ScriptManagerImpl;

	class ScriptManager final : public rkit::Opaque<ScriptManagerImpl>
	{
	public:
		enum class ScriptLayer
		{
			kGlobal,
			kMap,

			kCount,
		};

		rkit::Result LoadScriptPackage(ScriptLayer layer, const rkit::Span<const uint8_t> &contents);
		void UnloadLayer(ScriptLayer layer);

		static rkit::Result Create(rkit::UniquePtr<ScriptManager> &outScriptManager);
	};
}
