#pragma once

#include "rkit/Core/CoroutineProtos.h"
#include "rkit/Core/Opaque.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/StringProto.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace rkit::data
{
	struct ContentID;
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

	template<uint32_t TNamespace, uint32_t TType>
	struct ScriptNamedResourceRef;
	struct ScriptNamedResourceRefBase;

	class ScriptEnvironment final : public rkit::Opaque<ScriptEnvironmentImpl>
	{
	public:
		explicit ScriptEnvironment(ScriptManagerImpl &scriptManager);

		rkit::ResultCoroutine StartSequence(rkit::ICoroThread &thread, ScriptContext &scriptContext, const Label &label, World &world);
		rkit::ResultCoroutine RunSwitch(rkit::ICoroThread &thread, ScriptContext &scriptContext, const Label &label, World &world);
		rkit::ResultCoroutine RunWindowCommands(rkit::ICoroThread &thread, ScriptWindowInstance &windowInstance, const Label &label);

		rkit::Result CreateScriptContext(rkit::UniquePtr<ScriptContext> &outScriptCtx);

		bool TryEvaluateFloatScriptExpr(float &outValue, const ScriptPackage &pkg, const ScriptExprValue &expr) const;

		template<uint32_t TExpectedNamespace, uint32_t TExpectedType>
		bool TryEvaluateNamedResourceRefScriptExpr(ScriptNamedResourceRef<TExpectedNamespace, TExpectedType> &outValue, const ScriptPackage &pkg, const ScriptExprValue &expr) const;

		rkit::Result TryEvaluateStringScriptExpr(bool &outSucceeded, rkit::ByteString &outValue, const ScriptPackage &pkg, const ScriptExprValue &expr) const;

		// SAVEGAME TODO

	private:
		bool PrivTryEvaluateNamedResourceRefScriptExpr(ScriptNamedResourceRefBase &outValue, const ScriptPackage &pkg, const ScriptExprValue &expr, uint32_t expectedNamespace, uint32_t expectedType) const;

		ScriptEnvironment() = delete;
	};
}

#include "ScriptNamedResourceRef.h"

namespace anox::game
{
	template<uint32_t TExpectedNamespace, uint32_t TExpectedType>
	bool ScriptEnvironment::TryEvaluateNamedResourceRefScriptExpr(ScriptNamedResourceRef<TExpectedNamespace, TExpectedType> &outValue, const ScriptPackage &pkg, const ScriptExprValue &expr) const
	{
		return PrivTryEvaluateNamedResourceRefScriptExpr(outValue, pkg, expr, TExpectedNamespace, TExpectedType);
	}
}
