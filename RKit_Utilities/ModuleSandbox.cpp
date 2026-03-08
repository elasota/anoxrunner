#include "ModuleSandbox.h"

#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/Opaque.h"

#include "rkit/Sandbox/SandboxEntryDescriptor.h"
#include "rkit/Sandbox/SandboxSysCall.h"
#include "rkit/Sandbox/SandboxModule.h"

namespace rkit::utils
{
	class ModuleThreadContext final : public sandbox::IThreadContext
	{
		// Module thread contexts just run in OS threads
	};

	class ModuleSandboxImpl final : public OpaqueImplementation<ModuleSandbox>
	{
	public:
		friend class ModuleSandbox;

		ModuleSandboxImpl(sandbox::Address_t entryDescriptor, IModule *module, IMallocDriver *alloc);
		~ModuleSandboxImpl();

		sandbox::Address_t GetEntryDescriptor() const;

	private:
		ModuleSandboxImpl() = delete;

		sandbox::Address_t m_entryDescriptor = 0;
		IModule *m_module = nullptr;
		IMallocDriver *m_alloc = nullptr;
	};

	ModuleSandboxImpl::ModuleSandboxImpl(sandbox::Address_t entryDescriptor, IModule *module, IMallocDriver *alloc)
		: m_entryDescriptor(entryDescriptor)
		, m_module(module)
		, m_alloc(alloc)
	{
	}

	ModuleSandboxImpl::~ModuleSandboxImpl()
	{
		m_module->Unload();
	}

	sandbox::Address_t ModuleSandboxImpl::GetEntryDescriptor() const
	{
		return m_entryDescriptor;
	}

	ModuleSandbox::ModuleSandbox(sandbox::Address_t entryDescriptor, IModule *module, IMallocDriver *alloc)
		: Opaque<ModuleSandboxImpl>(entryDescriptor, module, alloc)
	{
	}

	ModuleSandbox::~ModuleSandbox()
	{
	}

	bool ModuleSandbox::TryAccessMemoryRange(void *&outPtr, sandbox::Address_t address, sandbox::Address_t size)
	{
		if (address > std::numeric_limits<uintptr_t>::max())
			return false;

		outPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(address));
		return true;
	}

	Result ModuleSandbox::CallFunction(sandbox::Address_t address, sandbox::io::Value_t *ioValues, size_t numReturnValues, size_t numParameters)
	{
		sandbox::ExportDescriptor::ExportFunctionPtr_t exportFunc = reinterpret_cast<sandbox::ExportDescriptor::ExportFunctionPtr_t>(address);

		exportFunc(ioValues + numReturnValues + 1);

		PackedResultAndExtCode packedResult = static_cast<PackedResultAndExtCode>(ioValues[numReturnValues]);
		RKIT_CHECK(rkit::ThrowIfError(packedResult));

		RKIT_RETURN_OK;
	}

	Result ModuleSandbox::CreateThreadConext(UniquePtr<sandbox::IThreadContext> &outThreadContext, const sandbox::ThreadCreationParameters &threadParams)
	{
		return rkit::New<ModuleThreadContext>(outThreadContext);
	}

	Result ModuleSandbox::AllocDynamicMemory(sandbox::Address_t &outAddress, size_t size)
	{
		outAddress = reinterpret_cast<uintptr_t>(Impl().m_alloc->Alloc(size));
		RKIT_RETURN_OK;
	}

	Result ModuleSandbox::ReleaseDynamicMemory(sandbox::Address_t address)
	{
		Impl().m_alloc->Free(reinterpret_cast<void *>(static_cast<uintptr_t>(address & std::numeric_limits<uintptr_t>::max())));
		RKIT_RETURN_OK;
	}

	Result ModuleSandbox::RunInitializer()
	{
		// Module initializers are pre-run
		RKIT_RETURN_OK;
	}

	uint64_t ModuleSandbox::GetEntryDescriptor() const
	{
		return Impl().GetEntryDescriptor();
	}

	Result ModuleSandbox::Create(UniquePtr<ModuleSandbox> &outSandbox, IModuleDriver *moduleDriver, uint32_t moduleNamespace, const Utf8Char_t *moduleName, const sandbox::SysCallCatalog &sysCalls, sandbox::Environment &sandboxEnv)
	{
		uint64_t entryDescriptor = 0;
		sandbox::SandboxModuleInitParams params;
		IMallocDriver *alloc = GetDrivers().m_mallocDriver;

		ISandbox **internalSandboxPtr = nullptr;

		params.m_outEntryDescriptor = &entryDescriptor;
		params.m_outSandboxPtr = &internalSandboxPtr;
		params.m_envPtr = &sandboxEnv;
		params.m_sysCalls = sysCalls.m_sysCalls;

		IModule *module = moduleDriver->LoadModule(moduleNamespace, moduleName, &params);
		if (!module)
			RKIT_THROW(rkit::ResultCode::kModuleLoadFailed);

		UniquePtr<ModuleSandbox> sandbox;


		RKIT_TRY_CATCH_RETHROW(
			NewWithAlloc<ModuleSandbox>(sandbox, alloc, entryDescriptor, module, alloc),
			rkit::CatchContext(
				[module]
				{
					module->Unload();
				}
			)
		);

		*internalSandboxPtr = outSandbox.Get();

		outSandbox = std::move(sandbox);

		RKIT_RETURN_OK;
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(rkit::utils::ModuleSandboxImpl)
