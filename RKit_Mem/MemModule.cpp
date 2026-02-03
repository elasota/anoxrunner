#include "rkit/Core/ModuleGlue.h"

#include "rkit/Core/Drivers.h"

#include "rkit/Mem/MemMapDriver.h"
#include "rkit/Mem/MemModule.h"

#include <atomic>

namespace rkit { namespace mem {
	class MemMapMallocDriver final : public IMallocDriver
	{
	public:
		MemMapMallocDriver(IMemMapDriver &mmapDriver);
		~MemMapMallocDriver();

	protected:
		void *InternalAlloc(size_t size) override;
		void *InternalRealloc(void *ptr, size_t size) override;
		void InternalFree(void *ptr) override;

	private:
		static void CheckMemAllocCounter(uint32_t counter);

		IMemMapDriver &m_mmapDriver;
		std::atomic<uint32_t> m_memAllocCounter;
		std::atomic<uint32_t> m_freeCounter;
	};

	MemMapMallocDriver::MemMapMallocDriver(IMemMapDriver &mmapDriver)
		: m_mmapDriver(mmapDriver)
		, m_memAllocCounter(0)
	{
	}

	MemMapMallocDriver::~MemMapMallocDriver()
	{
	}

	void *MemMapMallocDriver::InternalAlloc(size_t size)
	{
		void *mem = malloc(size + 16);
		if (!mem)
			return nullptr;

		uint32_t memAllocCounter = m_memAllocCounter.fetch_add(1);
		CheckMemAllocCounter(memAllocCounter);

		memcpy(mem, &memAllocCounter, sizeof(memAllocCounter));

		return static_cast<uint8_t *>(mem) + 16;
	}

	void *MemMapMallocDriver::InternalRealloc(void *ptr, size_t size)
	{
		void *mem = realloc(static_cast<uint8_t *>(ptr) - 16, size + 16);
		if (!mem)
			return nullptr;

		uint32_t memAllocCounter = m_memAllocCounter.fetch_add(1);
		CheckMemAllocCounter(memAllocCounter);

		memcpy(mem, &memAllocCounter, sizeof(memAllocCounter));

		return static_cast<uint8_t *>(mem) + 16;
	}

	void MemMapMallocDriver::InternalFree(void *ptr)
	{
		if (ptr)
		{
			memset(static_cast<uint8_t *>(ptr) - 16, 0, 16);
			free(static_cast<uint8_t *>(ptr) - 16);

			m_freeCounter.fetch_add(1);
		}
	}

	void MemMapMallocDriver::CheckMemAllocCounter(uint32_t counter)
	{
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

		RKIT_RETURN_OK;
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
