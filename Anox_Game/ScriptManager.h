#pragma once

#include "rkit/Core/CoroutineProtos.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/Opaque.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	template<class T>
	class Span;
}

namespace anox::game::ape
{
	struct ExternDispatchContext
	{
	};
}

namespace anox::game
{
	class ScriptEnvironment;
	class ScriptManagerImpl;
	class ScriptHandle;

	class ScriptManager final : public rkit::Opaque<ScriptManagerImpl>
	{
	public:
		typedef rkit::ResultCoroutine (*ExternDispatchFunc_t)(rkit::ICoroThread &thread, const ape::ExternDispatchContext &context);

		enum class ScriptLayer
		{
			kGlobal,
			kMap,

			kCount,
		};

		rkit::Result LoadScriptPackage(ScriptLayer layer, const rkit::Span<const uint8_t> &contents);
		void UnloadLayer(ScriptLayer layer);

		rkit::Result CreateScriptEnvironment(rkit::UniquePtr<ScriptEnvironment> &outScriptEnvironment);

		template<class TOp>
		void RegisterExtern();

		static rkit::Result Create(rkit::UniquePtr<ScriptManager> &outScriptManager);

	private:
		void InternalRegisterExtern(size_t slot, ExternDispatchFunc_t dispatchFunc);
	};
}

namespace anox::game
{
	template<class TOp>
	void ScriptManager::RegisterExtern()
	{
		this->InternalRegisterExtern(static_cast<size_t>(TOp::kOpcode), TOp::Dispatch);
	}
}
