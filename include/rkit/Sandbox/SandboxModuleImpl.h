#include "rkit/Core/ModuleGlue.h"
#include "rkit/Sandbox/SandboxModule.h"

#define RKIT_SANDBOX_DEFINE_MODULE_CLASS(sandboxClass, moduleClass)\
class moduleClass\
{\
public:\
	static rkit::Result Init(const rkit::ModuleInitParameters *params)\
	{\
		const rkit::sandbox::SandboxModuleInitParams *sandboxParams = static_cast<const rkit::sandbox::SandboxModuleInitParams *>(params);\
		(*sandboxParams->m_outEntryDescriptor) = reinterpret_cast<uintptr_t>(&sandboxClass::ms_entryDescriptor);\
		(*sandboxParams->m_outSandboxPtr) = &sandboxClass::ms_sandboxPtr;\
		sandboxClass::ms_criticalError = sandboxParams->m_criticalError;\
		sandboxClass::ms_sandboxEnvPtr = sandboxParams->m_envPtr;\
		sandboxClass::ms_sysCalls = sandboxParams->m_sysCalls;\
		RKIT_RETURN_OK;\
	}\
	static void Shutdown()\
	{\
	}\
};

#define RKIT_SANDBOX_DEFINE_SYSCALL(stubName)	\
	{ u8 ## #stubName, sizeof(u8 ## #stubName), static_cast<size_t>(SysCall::k ## stubName) }

#define RKIT_SANDBOX_DEFINE_EXPORT(exportName)	\
	{ u8 ## #exportName, sizeof(u8 ## #exportName), Exports::exportName }

#define RKIT_SANDBOX_IMPLEMENT_MODULE_GLOBALS	\
namespace rkit::sandbox::io \
{ \
	inline void CriticalError(ISandbox *sandbox, Environment *env, CriticalErrorFunc_t criticalError) noexcept\
	{\
		criticalError(sandbox, env, nullptr);\
	}\
	inline void SysCall(ISandbox *sandbox, Environment *env, const SysCallDispatchFunc_t *sysCalls, uint32_t sysCallID, PackedResultAndExtCode &outResult, Value_t *values) noexcept\
	{\
		outResult = sysCalls[sysCallID](sandbox, env, nullptr, reinterpret_cast<uintptr_t>(values));\
	}\
	inline Value_t LoadParam(void *ioContext, uint32_t paramIndex) noexcept\
	{\
		return static_cast<const Value_t *>(ioContext)[paramIndex];\
	}\
	inline void StoreResult(void *ioContext, PackedResultAndExtCode result) noexcept\
	{\
		*(static_cast<Value_t *>(ioContext) - 1) = static_cast<Value_t>(result);\
	}\
	inline void StoreReturnValue(void *ioContext, uint32_t index, Value_t value) noexcept\
	{\
		*(static_cast<Value_t *>(ioContext) - 2 - index) = value;\
	}\
}

#if RKIT_MODULE_LINKER_TYPE == RKIT_MODULE_LINKER_TYPE_DLL

#include "rkit/Core/Drivers.h"

#define RKIT_SANDBOX_IMPLEMENT_MODULE(moduleNamespace, moduleName, moduleClass)	\
namespace rkit::sandbox::priv\
{\
	Drivers g_drivers;\
}\
namespace rkit\
{\
	Drivers &GetMutableDrivers() { return ::rkit::sandbox::priv::g_drivers; }\
	const Drivers &GetDrivers() { return ::rkit::sandbox::priv::g_drivers; }\
}\
extern "C" __declspec(dllexport) void InitializeRKitModule(void *moduleAPIPtr)\
{\
	::rkit::ModuleAPI_Win32 *moduleAPI = static_cast<::rkit::ModuleAPI_Win32 *>(moduleAPIPtr);\
	moduleAPI->m_initFunction = moduleClass::Init;\
	moduleAPI->m_shutdownFunction = moduleClass::Shutdown;\
}\
RKIT_IMPLEMENT_PER_MODULE_FUNCTIONS



#define RKIT_SANDBOX_USE_PRIVATE_DRIVERS 1

#else

#define RKIT_SANDBOX_IMPLEMENT_MODULE RKIT_IMPLEMENT_MODULE
#define RKIT_SANDBOX_USE_PRIVATE_DRIVERS 0

#endif

