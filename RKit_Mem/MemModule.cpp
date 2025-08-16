#include "rkit/Core/ModuleGlue.h"

#include "rkit/Core/Drivers.h"

#include "rkit/Mem/MemMapDriver.h"
#include "rkit/Mem/MemModule.h"

namespace rkit { namespace mem {
	class MemMapMallocDriver final : public IMallocDriver
	{
	public:
		MemMapMallocDriver(IMemMapDriver &mmapDriver);

	protected:
		void *InternalAlloc(size_t size) override;
		void *InternalRealloc(void *ptr, size_t size) override;
		void InternalFree(void *ptr) override;

	private:
		IMemMapDriver &m_mmapDriver;
	};

	MemMapMallocDriver::MemMapMallocDriver(IMemMapDriver &mmapDriver)
		: m_mmapDriver(mmapDriver)
	{
	}

	void *MemMapMallocDriver::InternalAlloc(size_t size)
	{
		return malloc(size);
	}

	void *MemMapMallocDriver::InternalRealloc(void *ptr, size_t size)
	{
		return realloc(ptr, size);
	}

	void MemMapMallocDriver::InternalFree(void *ptr)
	{
		if (ptr)
			free(ptr);
	}

	class MemModule final
	{
	public:
		static Result Init(const ModuleInitParameters *);
		static void Shutdown();

	private:
		static SimpleObjectAllocation<IMallocDriver> ms_mmapPrevDriver;
	};

	Result MemModule::Init(const ModuleInitParameters *baseParams)
	{
		const MemModuleInitParameters *initParams = static_cast<const MemModuleInitParameters *>(baseParams);

		if (initParams->m_mmapDriver)
		{
			UniquePtr<IMallocDriver> mmapMallocDriver;
			RKIT_CHECK(New<MemMapMallocDriver>(mmapMallocDriver, *initParams->m_mmapDriver));

			ms_mmapPrevDriver = GetDrivers().m_mallocDriver;
			GetMutableDrivers().m_mallocDriver = mmapMallocDriver.Detach();
		}

		return rkit::ResultCode::kOK;
	}

	void MemModule::Shutdown()
	{
		if (ms_mmapPrevDriver.m_obj)
		{
			Delete(GetDrivers().m_mallocDriver);
			GetMutableDrivers().m_mallocDriver = ms_mmapPrevDriver;

			ms_mmapPrevDriver.Clear();
		}
	}

	SimpleObjectAllocation<IMallocDriver> MemModule::ms_mmapPrevDriver;
} } // rkit::mem


RKIT_IMPLEMENT_MODULE("RKit", "Mem", ::rkit::mem::MemModule)
