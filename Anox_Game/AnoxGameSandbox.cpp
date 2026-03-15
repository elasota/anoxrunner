#include "anox/Sandbox/AnoxGame.sb.generated.inl"

#include "anox/AnoxModule.h"

#include "rkit/Sandbox/SandboxModuleImpl.h"

namespace anox::game
{
	RKIT_SANDBOX_DEFINE_MODULE_CLASS(anox::game::sandbox::SandboxAPI, AnoxGameSandboxModule)
}

RKIT_SANDBOX_IMPLEMENT_MODULE(Anox, Game, anox::game::AnoxGameSandboxModule)

RKIT_SANDBOX_IMPLEMENT_MODULE_GLOBALS

#if !!RKIT_SANDBOX_USE_PRIVATE_DRIVERS
namespace anox::game::sandbox
{
	class InternalMallocDriver final : public rkit::IMallocDriver
	{
	public:
		struct alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) MemBlockHeader
		{
			size_t m_size = 0;
			uint32_t m_mmid = 0;
		};

		static void *StaticAlloc(size_t size);
		static void *StaticRealloc(void *ptr, size_t size);
		static void StaticFree(void *ptr);

	protected:
		void *InternalAlloc(size_t size) override;
		void *InternalRealloc(void *ptr, size_t size) override;
		void InternalFree(void *ptr) override;
	};

	void *InternalMallocDriver::StaticAlloc(size_t size)
	{
		if (size == 0)
			return nullptr;

		if ((std::numeric_limits<size_t>::max() - size) < sizeof(MemBlockHeader))
			return nullptr;

		void *ptr = nullptr;
		uint32_t mmid = 0;
		anox::game::sandbox::SandboxImports::MemAlloc(ptr, mmid, size + sizeof(MemBlockHeader));

		MemBlockHeader *header = static_cast<MemBlockHeader *>(ptr);
		header->m_mmid = mmid;
		header->m_size = size;
		return header + 1;
	}

	void *InternalMallocDriver::StaticRealloc(void *ptr, size_t size)
	{
		if (ptr == nullptr)
			return StaticAlloc(size);

		if (size == 0)
		{
			StaticFree(ptr);
			return nullptr;
		}

		const MemBlockHeader *header = static_cast<const MemBlockHeader *>(ptr) - 1;

		if (header->m_size == size)
			return ptr;

		void *newPtr = StaticAlloc(size);
		if (!newPtr)
			return nullptr;

		memcpy(newPtr, ptr, std::min(header->m_size, size));
		anox::game::sandbox::SandboxImports::MemFree(header->m_mmid);

		return newPtr;
	}

	void InternalMallocDriver::StaticFree(void *ptr)
	{
		if (ptr == nullptr)
			return;

		const MemBlockHeader *header = static_cast<const MemBlockHeader *>(ptr) - 1;
		anox::game::sandbox::SandboxImports::MemFree(header->m_mmid);
	}

	void *InternalMallocDriver::InternalAlloc(size_t size)
	{
		return StaticAlloc(size);
	}

	void *InternalMallocDriver::InternalRealloc(void *ptr, size_t size)
	{
		return StaticRealloc(ptr, size);
	}

	void InternalMallocDriver::InternalFree(void *ptr)
	{
		StaticFree(ptr);
	}
}
#endif

namespace anox::game::sandbox
{
	::rkit::Result SandboxExports::Initialize(void *&outGameSession)
	{
#if !!RKIT_SANDBOX_USE_PRIVATE_DRIVERS
		{
			void *memAllocDriverPtr = InternalMallocDriver::StaticAlloc(sizeof(InternalMallocDriver));
			if (!memAllocDriverPtr)
				RKIT_THROW(rkit::ResultCode::kOutOfMemory);

			InternalMallocDriver *mallocDriver = new (memAllocDriverPtr) InternalMallocDriver();

			rkit::SimpleObjectAllocation<rkit::IMallocDriver> mallocDriverPtr;
			mallocDriverPtr.m_alloc = mallocDriver;
			mallocDriverPtr.m_mem = memAllocDriverPtr;
			mallocDriverPtr.m_alloc = mallocDriver;

			rkit::GetMutableDrivers().m_mallocDriver = mallocDriverPtr;
		}
#endif

		RKIT_RETURN_OK;
	}

	::rkit::Result SandboxExports::Shutdown(void *gameSession)
	{

	}
}
