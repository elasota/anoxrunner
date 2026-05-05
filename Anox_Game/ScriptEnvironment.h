#pragma once

#include "rkit/Core/CoroutineProtos.h"
#include "rkit/Core/Opaque.h"
#include "rkit/Core/Result.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace anox
{
	class Label;
}

namespace anox::game
{
	class ScriptEnvironmentImpl;
	class ScriptManagerImpl;
	class ScriptContext;
	class ScriptPackage;
	class ScriptExprValue;
	class World;

	class ScriptEnvironment final : public rkit::Opaque<ScriptEnvironmentImpl>
	{
	public:
		explicit ScriptEnvironment(ScriptManagerImpl &scriptManager);

		rkit::ResultCoroutine StartSequence(rkit::ICoroThread &thread, ScriptContext &scriptContext, const Label &label, World &world);

		rkit::Result CreateScriptContext(rkit::UniquePtr<ScriptContext> &outScriptCtx);

		bool TryEvaluateFloatScriptExpr(float &outValue, const ScriptPackage &pkg, const ScriptExprValue &expr) const;

	private:
		ScriptEnvironment() = delete;
	};
}
