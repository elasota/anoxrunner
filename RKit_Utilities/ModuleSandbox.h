#pragma once

#include "rkit/Sandbox/Sandbox.h"
#include "rkit/Sandbox/SandboxAddress.h"
#include "rkit/Sandbox/SandboxSysCall.h"

#include "rkit/Core/Opaque.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	template<class T>
	class Span;

	struct IMallocDriver;
	struct IModuleDriver;
	struct IModule;
}

namespace rkit::utils
{
	class ModuleSandboxImpl;

	class ModuleSandbox final : public Opaque<ModuleSandboxImpl>, public ISandbox
	{
	public:
		ModuleSandbox(sandbox::Address_t entryDescriptor, IModule *module, IMallocDriver *alloc);
		~ModuleSandbox();

		bool TryAccessMemoryRange(void *&outPtr, uint64_t address, uint64_t size) override;
		Result CallFunction(sandbox::Address_t address, sandbox::io::Value_t *ioValues, size_t numReturnValues, size_t numParameters) override;
		Result CreateThreadConext(UniquePtr<sandbox::IThreadContext> &outThreadContext, const sandbox::ThreadCreationParameters &threadParams) override;
		Result RunInitializer() override;
		Result AllocDynamicMemory(sandbox::Address_t &outAddress, size_t size) override;
		Result ReleaseDynamicMemory(sandbox::Address_t address) override;
		uint64_t GetEntryDescriptor() const override;

		static Result Create(UniquePtr<ModuleSandbox> &outSandbox, IModuleDriver *moduleDriver, uint32_t moduleNamespace, const Utf8Char_t *moduleName, const sandbox::SysCallCatalog &sysCalls, sandbox::Environment &sandboxEnv);
	};
}
