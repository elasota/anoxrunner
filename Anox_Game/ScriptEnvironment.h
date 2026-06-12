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
	struct ScriptExprValue;
	class World;
	class ScriptWindowInstance;
	struct ScriptWindow;

	class ScriptEnvironment final : public rkit::Opaque<ScriptEnvironmentImpl>
	{
	public:
		explicit ScriptEnvironment(ScriptManagerImpl &scriptManager);

		rkit::ResultCoroutine StartSequence(rkit::ICoroThread &thread, ScriptContext &scriptContext, const Label &label, World &world);
		rkit::ResultCoroutine RunSwitch(rkit::ICoroThread &thread, ScriptContext &scriptContext, const Label &label, World &world);
		rkit::ResultCoroutine RunWindowCommands(rkit::ICoroThread &thread, ScriptWindowInstance &windowInstance, const Label &label);

		rkit::Result CreateScriptContext(rkit::UniquePtr<ScriptContext> &outScriptCtx);

		bool TryEvaluateFloatScriptExpr(float &outValue, const ScriptPackage &pkg, const ScriptExprValue &expr) const;

	private:
		ScriptEnvironment() = delete;
	};
}
