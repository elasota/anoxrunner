#include "rkit/Render/RenderDriver.h"

#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/NoCopy.h"
#include "rkit/Core/MallocDriver.h"

#include "VulkanAPI.h"

namespace rkit::render::vulkan
{
	class VulkanAllocationCallbacks final : public NoCopy
	{
	public:
		explicit VulkanAllocationCallbacks(IMallocDriver *alloc);

		const VkAllocationCallbacks *GetCallbacks() const;

	private:
		VulkanAllocationCallbacks() = delete;

		static void DecodePadding(const uint8_t *paddedAddress, uint8_t &outAlignmentBits, size_t &outFullPaddingAmount);
		static size_t ComputeFullPaddingOffsetForAddress(uintptr_t ptr, uint8_t alignmentBits);
		static void EncodePadding(uint8_t *baseAddress, uint8_t alignmentBits, size_t fullPaddingOffset);

		static void ComputePaddingRequirement(size_t alignment, uint8_t &outAlignmentBits, size_t &outMaxPaddingAmount);

		void *Allocate(size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
		void Free(void *pMemory);
		void InternalAllocationNotify(size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);
		void InternalFreeNotify(size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);
		void *Reallocate(void *pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);

		static void *VKAPI_PTR AllocateCallback(void *pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
		static void VKAPI_PTR FreeCallback(void *pUserData, void *pMemory);
		static void VKAPI_PTR InternalAllocationNotificationCallback(void *pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);
		static void VKAPI_PTR InternalFreeNotificationCallback(void *pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);
		static void *VKAPI_PTR ReallocationCallback(void *pUserData, void *pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);

		IMallocDriver *m_alloc;
		VkAllocationCallbacks m_callbacks;
	};

	class RenderVulkanDriver final : public rkit::render::IRenderDriver
	{
	public:
		rkit::Result InitDriver() override;
		void ShutdownDriver() override;

		uint32_t GetDriverNamespaceID() const override { return rkit::IModuleDriver::kDefaultNamespace; }
		rkit::StringView GetDriverName() const override { return "Render_Vulkan"; }

		const VkAllocationCallbacks *GetAllocCallbacks() const;

	private:
		Result LoadVulkanAPI();

		IMallocDriver *m_alloc = nullptr;

		VkInstance m_vkInstance;
		bool m_vkInstanceIsInitialized = false;

		VulkanAPI m_vkAPI;

		UniquePtr<ISystemLibrary> m_vkLibrary;
		UniquePtr<VulkanAllocationCallbacks> m_allocationCallbacks;
	};

	typedef rkit::CustomDriverModuleStub<RenderVulkanDriver> RenderVulkanModule;

	VulkanAllocationCallbacks::VulkanAllocationCallbacks(IMallocDriver *alloc)
		: m_alloc(alloc)
	{
		m_callbacks.pUserData = this;
		m_callbacks.pfnAllocation = AllocateCallback;
		m_callbacks.pfnReallocation = ReallocationCallback;
		m_callbacks.pfnFree = FreeCallback;
		m_callbacks.pfnInternalAllocation = InternalAllocationNotificationCallback;
		m_callbacks.pfnInternalFree = InternalFreeNotificationCallback;
	}

	const VkAllocationCallbacks *VulkanAllocationCallbacks::GetCallbacks() const
	{
		return &m_callbacks;
	}

	void VulkanAllocationCallbacks::DecodePadding(const uint8_t *paddedAddress, uint8_t &outAlignmentBits, size_t &outFullPaddingAmount)
	{
		size_t fullPaddingAmount = 0;
		fullPaddingAmount++;
		paddedAddress--;

		uint8_t alignmentBits = *paddedAddress;

		size_t paddingValueSize = (alignmentBits + 7) / 8;

		paddedAddress -= paddingValueSize;
		fullPaddingAmount -= paddingValueSize;

		size_t paddingAmount = 0;
		for (size_t i = 0; i < paddingValueSize; i++)
			fullPaddingAmount += static_cast<size_t>(paddedAddress[i]) << (i * 8);

		outAlignmentBits = alignmentBits;
		outFullPaddingAmount = fullPaddingAmount;
	}

	size_t VulkanAllocationCallbacks::ComputeFullPaddingOffsetForAddress(uintptr_t ptr, uint8_t alignmentBits)
	{
		size_t alignment = static_cast<size_t>(1) << alignmentBits;
		size_t alignmentMask = alignment - 1;

		size_t ptrPositionAboveBaseline = static_cast<size_t>(ptr & alignmentMask);

		size_t paddingValueSize = (alignmentBits + 7) / 8;

		ptrPositionAboveBaseline += paddingValueSize;	// Padding value
		ptrPositionAboveBaseline += 1;					// Alignment bits

		size_t invResidual = (ptrPositionAboveBaseline & alignmentMask);
		if (invResidual != 0)
			ptrPositionAboveBaseline += (alignment - invResidual);

		return ptrPositionAboveBaseline;
	}

	void VulkanAllocationCallbacks::EncodePadding(uint8_t *baseAddress, uint8_t alignmentBits, size_t fullPaddingOffset)
	{
		fullPaddingOffset--;
		baseAddress--;

		*baseAddress = alignmentBits;

		size_t paddingValueSize = (alignmentBits + 7) / 8;

		baseAddress -= paddingValueSize;
		fullPaddingOffset -= paddingValueSize;

		for (size_t i = 0; i < paddingValueSize; i++)
			baseAddress[i] = static_cast<uint8_t>((fullPaddingOffset >> (i * 8)) & 0xffu);
	}

	void VulkanAllocationCallbacks::ComputePaddingRequirement(size_t alignment, uint8_t &outAlignmentBits, size_t &outMaxPaddingAmount)
	{
		if (alignment == 0)
			alignment = 1;

		uint8_t alignmentBits = 0;

		while (alignment > 0)
		{
			alignmentBits++;
			alignment >>= 1;
		}

		alignmentBits--;

		alignment = static_cast<size_t>(1) << alignmentBits;

		size_t alignmentMask = alignment - 1;

		size_t maxPaddingAmount = 1;

		size_t paddingValueSize = (alignmentBits + 7) / 8;

		maxPaddingAmount += paddingValueSize;

		size_t residual = (maxPaddingAmount & alignmentMask);
		if (residual != 0)
			maxPaddingAmount += (alignment - residual);

		outAlignmentBits = alignmentBits;
		outMaxPaddingAmount = maxPaddingAmount;
	}

	void *VulkanAllocationCallbacks::Allocate(size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
	{
		uint8_t alignmentBits = 0;
		size_t maxPaddingAmount = 0;

		ComputePaddingRequirement(alignment, alignmentBits, maxPaddingAmount);

		if (std::numeric_limits<size_t>::max() - maxPaddingAmount < size)
			return nullptr;

		uint8_t *memBuffer = static_cast<uint8_t *>(m_alloc->Alloc(size + maxPaddingAmount));

		size_t fullPadding = ComputeFullPaddingOffsetForAddress(reinterpret_cast<uintptr_t>(memBuffer), alignmentBits);

		EncodePadding(memBuffer, alignmentBits, fullPadding);

		return memBuffer + fullPadding;
	}

	void VulkanAllocationCallbacks::Free(void *pMemory)
	{
		if (pMemory == nullptr)
			return;

		uint8_t *mem = static_cast<uint8_t *>(pMemory);

		uint8_t alignmentBits = 0;
		size_t fullPadding = 0;
		DecodePadding(mem, alignmentBits, fullPadding);

		m_alloc->Free(mem - fullPadding);
	}

	void VulkanAllocationCallbacks::InternalAllocationNotify(size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
	{
	}

	void VulkanAllocationCallbacks::InternalFreeNotify(size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
	{
	}

	void *VulkanAllocationCallbacks::Reallocate(void *pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
	{
		uint8_t *originalMem = static_cast<uint8_t *>(pOriginal);

		uint8_t originalAlignmentBits = 0;
		size_t originalFullPadding = 0;
		DecodePadding(originalMem, originalAlignmentBits, originalFullPadding);

		uint8_t newAlignmentBits = 0;
		size_t newMaxPaddingAmount = 0;
		ComputePaddingRequirement(alignment, newAlignmentBits, newMaxPaddingAmount);

		if (newMaxPaddingAmount < originalFullPadding)
			newMaxPaddingAmount = originalFullPadding;	// In case padding amount is smaller this time, we must copy all of the original padding

		if (std::numeric_limits<size_t>::max() - newMaxPaddingAmount < size)
			return nullptr;

		uint8_t *newMem = static_cast<uint8_t *>(m_alloc->Realloc(originalMem, newMaxPaddingAmount + size));

		size_t newFullPadding = ComputeFullPaddingOffsetForAddress(reinterpret_cast<uintptr_t>(newMem), newAlignmentBits);

		if (newFullPadding != originalFullPadding)
		{
			// Really shouldn't copy all of size here, but this is hopefully very rare
			memmove(newMem + newFullPadding, newMem + originalFullPadding, size);
		}

		EncodePadding(newMem, newAlignmentBits, newFullPadding);
		return newMem + newFullPadding;
	}

	void *VKAPI_PTR VulkanAllocationCallbacks::AllocateCallback(void *pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
	{
		return static_cast<VulkanAllocationCallbacks *>(pUserData)->Allocate(size, alignment, allocationScope);
	}

	void VKAPI_PTR VulkanAllocationCallbacks::FreeCallback(void *pUserData, void *pMemory)
	{
		static_cast<VulkanAllocationCallbacks *>(pUserData)->Free(pMemory);
	}

	void VKAPI_PTR VulkanAllocationCallbacks::InternalAllocationNotificationCallback(void *pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
	{
		static_cast<VulkanAllocationCallbacks *>(pUserData)->InternalAllocationNotify(size, allocationType, allocationScope);
	}

	void VKAPI_PTR VulkanAllocationCallbacks::InternalFreeNotificationCallback(void *pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
	{
		static_cast<VulkanAllocationCallbacks *>(pUserData)->InternalFreeNotify(size, allocationType, allocationScope);
	}

	void *VKAPI_PTR VulkanAllocationCallbacks::ReallocationCallback(void *pUserData, void *pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
	{
		return static_cast<VulkanAllocationCallbacks *>(pUserData)->Reallocate(pOriginal, size, alignment, allocationScope);
	}

	const VkAllocationCallbacks *RenderVulkanDriver::GetAllocCallbacks() const
	{
		return m_allocationCallbacks->GetCallbacks();
	}

	void VulkanAPI::TerminalResolveFunctionLoader(FunctionLoaderInfo &loaderInfo)
	{
		loaderInfo.m_nextCallback = nullptr;
	}

	Result RenderVulkanDriver::LoadVulkanAPI()
	{
		ISystemDriver *sysDriver = GetDrivers().m_systemDriver;
		RKIT_CHECK(sysDriver->OpenSystemLibrary(m_vkLibrary, SystemLibraryType::kVulkan));

		FunctionLoaderInfo loaderInfo;
		loaderInfo.m_nextCallback = m_vkAPI.GetFirstResolveFunctionCallback();

		while (loaderInfo.m_nextCallback != nullptr)
		{
			loaderInfo.m_nextCallback(&m_vkAPI, loaderInfo);

			bool foundFunction = m_vkLibrary->GetFunction(loaderInfo.m_pfnAddress, loaderInfo.m_fnName);

			if (!foundFunction && !loaderInfo.m_isOptional)
			{
				rkit::log::ErrorFmt("Failed to find required Vulkan function %s", loaderInfo.m_fnName.GetChars());
				return ResultCode::kModuleLoadFailed;
			}
		}

		return ResultCode::kOK;
	}

	Result RenderVulkanDriver::InitDriver()
	{
		m_alloc = GetDrivers().m_mallocDriver;

		RKIT_CHECK(LoadVulkanAPI());

		

		RKIT_CHECK(NewWithAlloc<VulkanAllocationCallbacks>(m_allocationCallbacks, m_alloc, m_alloc));

		return rkit::ResultCode::kNotYetImplemented;
	}

	void RenderVulkanDriver::ShutdownDriver()
	{
		if (m_vkInstanceIsInitialized)
			m_vkAPI.vkDestroyInstance(m_vkInstance, GetAllocCallbacks());

		m_vkLibrary.Reset();
	}
}

RKIT_IMPLEMENT_MODULE("RKit", "Render_Vulkan", ::rkit::render::vulkan::RenderVulkanModule)
