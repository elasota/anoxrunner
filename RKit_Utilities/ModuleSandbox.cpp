#include "ModuleSandbox.h"

#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/MutexLock.h"
#include "rkit/Core/Opaque.h"
#include "rkit/Core/Vector.h"

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

		ModuleSandboxImpl(sandbox::Address_t entryDescriptor, IModule *module, IMallocDriver *alloc, UniquePtr<IMutex> &&memMutex);
		~ModuleSandboxImpl();

		Result AllocDynamicMemory(sandbox::Address_t &outAddress, uint32_t &outMMID, size_t size);
		Result ReleaseDynamicMemory(uint32_t mmid);

		sandbox::Address_t GetEntryDescriptor() const;

		static void CriticalError(ISandbox *sandbox, sandbox::Environment *env, sandbox::IThreadContext *thread) noexcept;

	private:
		ModuleSandboxImpl() = delete;

		sandbox::Address_t m_entryDescriptor = 0;
		IModule *m_module = nullptr;
		IMallocDriver *m_alloc = nullptr;

		rkit::Vector<void *> m_memAllocations;
		rkit::Vector<uint32_t> m_mmidFreeList;
		UniquePtr<IMutex> m_memMutex;
	};

	ModuleSandboxImpl::ModuleSandboxImpl(sandbox::Address_t entryDescriptor, IModule *module, IMallocDriver *alloc, UniquePtr<IMutex> &&memMutex)
		: m_entryDescriptor(entryDescriptor)
		, m_module(module)
		, m_alloc(alloc)
		, m_memAllocations(alloc)
		, m_mmidFreeList(alloc)
		, m_memMutex(std::move(memMutex))
	{
	}

	ModuleSandboxImpl::~ModuleSandboxImpl()
	{
		m_module->Unload();

		for (void *mem : m_memAllocations)
		{
			if (mem)
				m_alloc->Free(mem);
		}
	}

	Result ModuleSandboxImpl::AllocDynamicMemory(sandbox::Address_t &outAddress, uint32_t &outMMID, size_t size)
	{
		if (size == 0)
		{
			outAddress = 0;
			outMMID = 0;
			RKIT_RETURN_OK;
		}

		void *mem = m_alloc->Alloc(size);
		if (!mem)
		{
			outAddress = 0;
			outMMID = 0;

			RKIT_THROW(ResultCode::kOutOfMemory);
		}

		rkit::MutexLock lock(*m_memMutex);

		if (m_mmidFreeList.Count() > 0)
		{
			size_t newSize = m_mmidFreeList.Count() - 1;
			uint32_t mmid = m_mmidFreeList[newSize];
			m_mmidFreeList.ShrinkToSize(newSize);
			m_memAllocations[mmid - 1] = mem;
			outMMID = mmid;
		}
		else
		{
			if (m_memAllocations.Count() == (std::numeric_limits<uint32_t>::max() - 1u))
				RKIT_THROW(ResultCode::kOutOfMemory);

			const uint32_t newMMID = m_memAllocations.Count() + 1;

			RKIT_TRY_CATCH_RETHROW(m_memAllocations.Append(mem),
				rkit::CatchContext(
					[this, mem, &lock]
					{
						lock.Unlock();
						this->m_alloc->Free(mem);
					}
				)
			);

			outMMID = newMMID;
		}

		outAddress = reinterpret_cast<uintptr_t>(mem);
		RKIT_RETURN_OK;
	}

	Result ModuleSandboxImpl::ReleaseDynamicMemory(uint32_t mmid)
	{
		if (mmid == 0)
			RKIT_RETURN_OK;

		void *mem = nullptr;

		{
			rkit::MutexLock lock(*m_memMutex);

			if (mmid > m_memAllocations.Count())
				RKIT_THROW(ResultCode::kOperationFailed);

			mem = m_memAllocations[mmid - 1];

			if (!mem)
				RKIT_THROW(ResultCode::kOperationFailed);

			RKIT_CHECK(m_mmidFreeList.Append(mmid));

			m_memAllocations[mmid - 1] = nullptr;
		}

		m_alloc->Free(mem);

		RKIT_RETURN_OK;
	}

	sandbox::Address_t ModuleSandboxImpl::GetEntryDescriptor() const
	{
		return m_entryDescriptor;
	}

	void ModuleSandboxImpl::CriticalError(ISandbox *sandbox, sandbox::Environment *env, sandbox::IThreadContext *thread) noexcept
	{
		RKIT_ASSERT(false);
		std::abort();
	}

	ModuleSandbox::ModuleSandbox(sandbox::Address_t entryDescriptor, IModule *module, IMallocDriver *alloc, UniquePtr<IMutex> &&memMutex)
		: Opaque<ModuleSandboxImpl>(entryDescriptor, module, alloc, std::move(memMutex))
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

	Result ModuleSandbox::CreateThreadContext(UniquePtr<sandbox::IThreadContext> &outThreadContext, const sandbox::ThreadCreationParameters &threadParams)
	{
		return rkit::New<ModuleThreadContext>(outThreadContext);
	}

	Result ModuleSandbox::AllocDynamicMemory(sandbox::Address_t &outAddress, uint32_t &outMMID, size_t size)
	{
		return Impl().AllocDynamicMemory(outAddress, outMMID, size);
	}

	Result ModuleSandbox::ReleaseDynamicMemory(uint32_t mmid)
	{
		return Impl().ReleaseDynamicMemory(mmid);
	}

	Result ModuleSandbox::RunInitializer(sandbox::IThreadContext &threadContext)
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
		params.m_criticalError = ModuleSandboxImpl::CriticalError;

		IModule *module = moduleDriver->LoadModule(moduleNamespace, moduleName, &params);
		if (!module)
			RKIT_THROW(rkit::ResultCode::kModuleLoadFailed);

		UniquePtr<IMutex> memMutex;
		RKIT_CHECK(GetDrivers().m_systemDriver->CreateMutex(memMutex));

		UniquePtr<ModuleSandbox> sandbox;

		RKIT_TRY_CATCH_RETHROW(
			NewWithAlloc<ModuleSandbox>(sandbox, alloc, entryDescriptor, module, alloc, std::move(memMutex)),
			rkit::CatchContext(
				[module]
				{
					module->Unload();
				}
			)
		);

		*internalSandboxPtr = sandbox.Get();

		outSandbox = std::move(sandbox);

		RKIT_RETURN_OK;
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(rkit::utils::ModuleSandboxImpl)
