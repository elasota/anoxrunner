#pragma once

#include "rkit/Core/CoroutineProtos.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/Opaque.h"
#include "rkit/Core/Span.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	template<class T>
	class Span;
}

namespace anox::game
{
	class ScriptPackage;
	class ScriptEnvironment;
	struct ScriptExprValue;
}

namespace anox::game::ape
{
	struct ExternDispatchContext
	{
		const ScriptPackage *m_pkg = nullptr;
		ScriptEnvironment *m_env = nullptr;
		rkit::Span<const ScriptExprValue> m_operands;
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
		typedef rkit::ResultCoroutine (*ExternDispatchFunc_t)(rkit::ICoroThread &thread, ape::ExternDispatchContext context);

		enum class ScriptLayer
		{
			kGlobal,
			kMap,

			kCount,
		};

		rkit::Result LoadScriptPackage(ScriptLayer layer, const rkit::Span<const uint8_t> &contents);
		void UnloadLayer(ScriptLayer layer);

		rkit::Result CreateScriptEnvironment(rkit::UniquePtr<ScriptEnvironment> &outScriptEnvironment);

		void RegisterAllExterns();
		ExternDispatchFunc_t GetExtern(size_t opcode) const;

		static rkit::Result Create(rkit::UniquePtr<ScriptManager> &outScriptManager);

	private:
		template<class TOp>
		void RegisterExtern();

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
