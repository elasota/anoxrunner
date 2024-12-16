#include "rkit/Render/RenderDriver.h"

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/MallocDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/NoCopy.h"
#include "rkit/Core/RefCounted.h"
#include "rkit/Core/Vector.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanAutoObject.h"

#include <cstring>

namespace rkit::render::vulkan
{
	namespace results
	{
		Result FirstChanceVulkanFailure(VkResult result)
		{
			rkit::log::ErrorFmt("Vulkan error %x", static_cast<unsigned int>(result));
			return Result(ResultCode::kGraphicsAPIException, static_cast<uint32_t>(result));
		}
	}

	class RenderVulkanDriver;

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

	class RenderVulkanPhysicalDevice final : public RefCounted
	{
	public:
		explicit RenderVulkanPhysicalDevice(VkPhysicalDevice physDevice);

		Result InitPhysicalDevice(const RenderVulkanDriver &driver);

	private:
		VkPhysicalDevice m_physDevice;
		VkPhysicalDeviceProperties m_props;
	};

	class RenderVulkanAdapter final : public IRenderAdapter
	{
	public:
		explicit RenderVulkanAdapter(const RCPtr<RenderVulkanPhysicalDevice> &physDevice);

	private:
		RCPtr<RenderVulkanPhysicalDevice> m_physDevice;
	};

	class RenderVulkanDriver final : public rkit::render::IRenderDriver
	{
	public:
		rkit::Result InitDriver(const DriverInitParameters *initParams) override;
		void ShutdownDriver() override;

		uint32_t GetDriverNamespaceID() const override { return rkit::IModuleDriver::kDefaultNamespace; }
		rkit::StringView GetDriverName() const override { return "Render_Vulkan"; }

		const VkAllocationCallbacks *GetAllocCallbacks() const;

		bool IsInstanceExtensionEnabled(const StringView &extName) const;
		bool IsExtensionEnabled(const char *layerName, const StringView &extName) const;

		Result EnumerateAdapters(Vector<UniquePtr<IRenderAdapter>> &adapters) const override;
		Result CreateDevice(IRenderAdapter &adapter) override;

		const VulkanAPI &GetAPI() const;

	private:
		struct ExtensionEnumeration
		{
			const char *m_layerName = nullptr;
			Vector<VkExtensionProperties> m_extensions;
		};

		struct ExtensionEnumerationFinal
		{
			const char *m_layerName = nullptr;
			Vector<StringView> m_extensions;
		};

		struct QueryItem
		{
			QueryItem(const StringView &name, bool isRequired);

			StringView m_name;
			bool m_required = false;
			bool m_isAvailable = false;
		};

		Result LoadVulkanAPI();
		Result LoadVulkanExtensionAPIs();

		Result EnumerateExtensions(const char *layerName, Vector<ExtensionEnumeration> &extProperties);
		Result EnumerateLayers(Vector<VkLayerProperties> &layerProperties);

		static VKAPI_ATTR VkBool32 VKAPI_CALL StaticDebugMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *data, void *userdata);
		void DebugMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *data);

		IMallocDriver *m_alloc = nullptr;

		VkInstance m_vkInstance = nullptr;
		bool m_vkInstanceIsInitialized = false;

		VulkanAPI m_vkAPI;

		UniquePtr<ISystemLibrary> m_vkLibrary;
		UniquePtr<VulkanAllocationCallbacks> m_allocationCallbacks;

		Vector<ExtensionEnumerationFinal> m_extensions;

		ValidationLevel m_validationLevel = ValidationLevel::kNone;

		AutoObjectOwnershipContext<VkInstance> m_instContext;

		AutoDebugUtilsMessengerEXT_t m_debugUtils;
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
		fullPaddingAmount += paddingValueSize;

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

		EncodePadding(memBuffer + fullPadding, alignmentBits, fullPadding);

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

		uint8_t *newMem = static_cast<uint8_t *>(m_alloc->Realloc(originalMem - originalFullPadding, newMaxPaddingAmount + size));

		size_t newFullPadding = ComputeFullPaddingOffsetForAddress(reinterpret_cast<uintptr_t>(newMem), newAlignmentBits);

		if (newFullPadding != originalFullPadding)
		{
			// Really shouldn't copy all of size here, but this is hopefully very rare
			memmove(newMem + newFullPadding, newMem + originalFullPadding, size);
		}

		EncodePadding(newMem + newFullPadding, newAlignmentBits, newFullPadding);
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

	bool RenderVulkanDriver::IsExtensionEnabled(const char *layerName, const StringView &extName) const
	{
		for (const ExtensionEnumerationFinal &candidate : m_extensions)
		{
			bool isLayerMatch = false;
			if (candidate.m_layerName == nullptr)
				isLayerMatch = (layerName == nullptr);
			else
			{
				if (layerName != nullptr)
					isLayerMatch = !strcmp(layerName, candidate.m_layerName);
			}

			if (isLayerMatch)
			{
				for (const StringView &candidateExtName : candidate.m_extensions)
				{
					if (candidateExtName == extName)
						return true;
				}
			}
		}

		return false;
	}

	const VulkanAPI &RenderVulkanDriver::GetAPI() const
	{
		return m_vkAPI;
	}

	Result RenderVulkanDriver::EnumerateAdapters(Vector<UniquePtr<IRenderAdapter>> &adapters) const
	{
		if (!m_vkInstanceIsInitialized)
			return ResultCode::kInternalError;

		uint32_t physicalDeviceCount = 0;
		RKIT_VK_CHECK(m_vkAPI.vkEnumeratePhysicalDevices(m_vkInstance, &physicalDeviceCount, nullptr));

		Vector<UniquePtr<IRenderAdapter>> deviceHandles;
		RKIT_CHECK(deviceHandles.Resize(physicalDeviceCount));

		Vector<VkPhysicalDevice> physDevices;
		RKIT_CHECK(physDevices.Resize(physicalDeviceCount));

		RKIT_VK_CHECK(m_vkAPI.vkEnumeratePhysicalDevices(m_vkInstance, &physicalDeviceCount, physDevices.GetBuffer()));

		for (size_t i = 0; i < physicalDeviceCount; i++)
		{
			RCPtr<RenderVulkanPhysicalDevice> physDevice;
			RKIT_CHECK(NewWithAlloc<RenderVulkanPhysicalDevice>(physDevice, m_alloc, physDevices[i]));
			RKIT_CHECK(physDevice->InitPhysicalDevice(*this));

			UniquePtr<RenderVulkanAdapter> adapter;
			RKIT_CHECK(NewWithAlloc<RenderVulkanAdapter>(adapter, m_alloc, physDevice));
		}

		return ResultCode::kOK;
	}

	bool RenderVulkanDriver::IsInstanceExtensionEnabled(const StringView &extName) const
	{
		return IsExtensionEnabled(nullptr, extName);
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

			if (loaderInfo.m_requiredExtension != nullptr)
				continue;

			bool foundFunction = m_vkLibrary->GetFunction(loaderInfo.m_pfnAddress, loaderInfo.m_fnName);

			if (!foundFunction && !loaderInfo.m_isOptional)
			{
				rkit::log::ErrorFmt("Failed to find required Vulkan function %s", loaderInfo.m_fnName.GetChars());
				return ResultCode::kModuleLoadFailed;
			}
		}

		return ResultCode::kOK;
	}

	Result RenderVulkanDriver::LoadVulkanExtensionAPIs()
	{
		FunctionLoaderInfo loaderInfo;

		loaderInfo.m_nextCallback = m_vkAPI.GetFirstResolveFunctionCallback();

		while (loaderInfo.m_nextCallback != nullptr)
		{
			loaderInfo.m_nextCallback(&m_vkAPI, loaderInfo);

			if (loaderInfo.m_requiredExtension == nullptr)
				continue;

			if (!IsExtensionEnabled(nullptr, StringView::FromCString(loaderInfo.m_requiredExtension)))
			{
				loaderInfo.m_copyVoidFunctionCallback(loaderInfo.m_pfnAddress, static_cast<PFN_vkVoidFunction>(nullptr));
				continue;
			}

			PFN_vkVoidFunction func = m_vkAPI.vkGetInstanceProcAddr(m_vkInstance, loaderInfo.m_fnName.GetChars());
			if (func == static_cast<PFN_vkVoidFunction>(nullptr))
			{
				if (!loaderInfo.m_isOptional)
				{
					rkit::log::ErrorFmt("Failed to find required Vulkan extension function %s", loaderInfo.m_fnName.GetChars());
					return ResultCode::kModuleLoadFailed;
				}
				else
					continue;
			}

			loaderInfo.m_copyVoidFunctionCallback(loaderInfo.m_pfnAddress, func);
		}

		return ResultCode::kOK;
	}

	Result RenderVulkanDriver::EnumerateExtensions(const char *layerName, Vector<ExtensionEnumeration> &extProperties)
	{
		uint32_t propertyCount = 0;
		RKIT_VK_CHECK(m_vkAPI.vkEnumerateInstanceExtensionProperties(layerName, &propertyCount, nullptr));

		ExtensionEnumeration extEnum;
		extEnum.m_layerName = layerName;

		if (propertyCount > 0)
		{
			RKIT_CHECK(extEnum.m_extensions.Resize(propertyCount));

			if (propertyCount != 0)
			{
				RKIT_VK_CHECK(m_vkAPI.vkEnumerateInstanceExtensionProperties(layerName, &propertyCount, extEnum.m_extensions.GetBuffer()));
			}

			RKIT_CHECK(extProperties.Append(std::move(extEnum)));
		}

		return ResultCode::kOK;
	}

	Result RenderVulkanDriver::EnumerateLayers(Vector<VkLayerProperties> &layerProperties)
	{
		uint32_t layerCount = 0;
		RKIT_VK_CHECK(m_vkAPI.vkEnumerateInstanceLayerProperties(&layerCount, nullptr));
		RKIT_CHECK(layerProperties.Resize(layerCount));

		if (layerCount != 0)
		{
			RKIT_VK_CHECK(m_vkAPI.vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.GetBuffer()));
		}

		return ResultCode::kOK;
	}

	VkBool32 RenderVulkanDriver::StaticDebugMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *data, void *userdata)
	{
		static_cast<RenderVulkanDriver *>(userdata)->DebugMessage(severity, type, data);
		return VK_FALSE;
	}

	void RenderVulkanDriver::DebugMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *data)
	{
		if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			rkit::log::Error(data->pMessage);
		else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			rkit::log::Warning(data->pMessage);
		else
			rkit::log::LogInfo(data->pMessage);
	}

	RenderVulkanDriver::QueryItem::QueryItem(const StringView &name, bool isRequired)
		: m_name(name)
		, m_required(isRequired)
		, m_isAvailable(false)
	{
	}

	RenderVulkanPhysicalDevice::RenderVulkanPhysicalDevice(VkPhysicalDevice physDevice)
		: m_physDevice(physDevice)
		, m_props{}
	{
	}

	Result RenderVulkanPhysicalDevice::InitPhysicalDevice(const RenderVulkanDriver &driver)
	{
		const VulkanAPI &vkAPI = driver.GetAPI();

		vkAPI.vkGetPhysicalDeviceProperties(m_physDevice, &m_props);
		return ResultCode::kOK;
	}


	RenderVulkanAdapter::RenderVulkanAdapter(const RCPtr<RenderVulkanPhysicalDevice> &physDevice)
		: m_physDevice(physDevice)
	{
	}

	Result RenderVulkanDriver::InitDriver(const DriverInitParameters *initParamsBase)
	{
		const RenderDriverInitProperties *initParams = static_cast<const RenderDriverInitProperties *>(initParamsBase);

		m_validationLevel = initParams->m_validationLevel;

		m_alloc = GetDrivers().m_mallocDriver;

		IUtilitiesDriver *utils = GetDrivers().m_utilitiesDriver;

		RKIT_CHECK(LoadVulkanAPI());

		Vector<ExtensionEnumeration> availableExtensions;
		RKIT_CHECK(EnumerateExtensions(nullptr, availableExtensions));

		Vector<VkLayerProperties> availableLayers;
		RKIT_CHECK(EnumerateLayers(availableLayers));

		Vector<QueryItem> requestedLayers;
		Vector<QueryItem> requestedExtensions;

		// Determine required and optional extensions
		if (initParams->m_validationLevel >= ValidationLevel::kSimple)
		{
			RKIT_CHECK(requestedLayers.Append(QueryItem("VK_LAYER_KHRONOS_validation", false)));
			RKIT_CHECK(requestedExtensions.Append(QueryItem("VK_EXT_layer_settings", false)));
		}

		if (initParams->m_enableLogging)
		{
			RKIT_CHECK(requestedExtensions.Append(QueryItem("VK_EXT_debug_utils", false)));
		}

		Vector<const char *> layers;
		Vector<const char *> extensions;

		for (QueryItem &requestedLayer : requestedLayers)
		{
			for (const VkLayerProperties &availableLayer : availableLayers)
			{
				if (requestedLayer.m_name == StringView::FromCString(availableLayer.layerName))
				{
					requestedLayer.m_isAvailable = true;
					break;
				}
			}

			if (!requestedLayer.m_isAvailable)
			{
				if (requestedLayer.m_required)
				{
					rkit::log::ErrorFmt("Missing required Vulkan layer %s", requestedLayer.m_name.GetChars());
					return ResultCode::kOperationFailed;
				}
				else
					continue;
			}

			RKIT_CHECK(layers.Append(requestedLayer.m_name.GetChars()));
		}

		// Enumerate layer extensions
		for (const char *layer : layers)
		{
			RKIT_CHECK(EnumerateExtensions(layer, availableExtensions));
		}

		// Find all extensions to actually request
		for (QueryItem &requestedExtension : requestedExtensions)
		{
			for (const ExtensionEnumeration &layerExts : availableExtensions)
			{
				for (const VkExtensionProperties &availableExtension : layerExts.m_extensions)
				{
					if (requestedExtension.m_name == StringView::FromCString(availableExtension.extensionName))
					{
						requestedExtension.m_isAvailable = true;
						break;
					}
				}
			}

			if (!requestedExtension.m_isAvailable)
			{
				if (requestedExtension.m_required)
				{
					rkit::log::ErrorFmt("Missing required Vulkan extension %s", requestedExtension.m_name.GetChars());
					return ResultCode::kOperationFailed;
				}
				else
					continue;
			}

			RKIT_CHECK(extensions.Append(requestedExtension.m_name.GetChars()));
		}

		// Build extension list
		for (const ExtensionEnumeration &layerExts : availableExtensions)
		{
			ExtensionEnumerationFinal extEnumFinal;

			extEnumFinal.m_layerName = layerExts.m_layerName;

			for (const VkExtensionProperties &availableExtension : layerExts.m_extensions)
			{
				for (QueryItem &requestedExtension : requestedExtensions)
				{
					if (requestedExtension.m_name == StringView::FromCString(availableExtension.extensionName))
					{
						RKIT_CHECK(extEnumFinal.m_extensions.Append(requestedExtension.m_name));
						break;
					}
				}
			}

			if (extEnumFinal.m_extensions.Count() > 0)
			{
				RKIT_CHECK(m_extensions.Append(std::move(extEnumFinal)));
			}
		}
		

		RKIT_CHECK(NewWithAlloc<VulkanAllocationCallbacks>(m_allocationCallbacks, m_alloc, m_alloc));

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = utils->GetProgramName().GetChars();

		uint32_t vMajor = 0;
		uint32_t vMinor = 0;
		uint32_t vPatch = 0;
		utils->GetProgramVersion(vMajor, vMinor, vPatch);
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);

		utils->GetRKitVersion(vMajor, vMinor, vPatch);
		appInfo.pEngineName = "Gale Force Games RKit";
		appInfo.engineVersion = VK_MAKE_VERSION(vMajor, vMinor, vPatch);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo instCreateInfo = {};
		instCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instCreateInfo.pApplicationInfo = &appInfo;
		instCreateInfo.enabledLayerCount = static_cast<uint32_t>(layers.Count());
		if (layers.Count() > 0)
			instCreateInfo.ppEnabledLayerNames = layers.GetBuffer();

		instCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.Count());
		if (extensions.Count() > 0)
			instCreateInfo.ppEnabledExtensionNames = extensions.GetBuffer();

		const void **ppInstCreateNext = &instCreateInfo.pNext;

		VkDebugUtilsMessengerCreateInfoEXT debugUtilsInfo = {};
		if (initParams->m_enableLogging && IsInstanceExtensionEnabled("VK_EXT_debug_utils"))
		{
			debugUtilsInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debugUtilsInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
				;
			debugUtilsInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			debugUtilsInfo.pfnUserCallback = StaticDebugMessage;
			debugUtilsInfo.pUserData = this;

			*ppInstCreateNext = &debugUtilsInfo;
			ppInstCreateNext = &debugUtilsInfo.pNext;
		}

		VkLayerSettingsCreateInfoEXT layerSettingsInfo = {};

		Vector<VkLayerSettingEXT> layerSettings;
		VkBool32 vkTrue = VK_TRUE;
		VkBool32 vkFalse = VK_FALSE;
		if (initParams->m_validationLevel >= ValidationLevel::kAggressive && IsExtensionEnabled("VK_LAYER_KHRONOS_validation", "VK_EXT_layer_settings"))
		{
			layerSettingsInfo.sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT;

			{
				VkLayerSettingEXT layerSetting = {};
				layerSetting.pLayerName = "VK_LAYER_KHRONOS_validation";

				layerSetting.pSettingName = "validate_sync";
				layerSetting.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT;
				layerSetting.pValues = &vkTrue;
				layerSetting.valueCount = 1;

				RKIT_CHECK(layerSettings.Append(layerSetting));

				layerSetting.pSettingName = "thread_safety";
				RKIT_CHECK(layerSettings.Append(layerSetting));
			}

			layerSettingsInfo.pSettings = layerSettings.GetBuffer();
			layerSettingsInfo.settingCount = static_cast<uint32_t>(layerSettings.Count());

			*ppInstCreateNext = &layerSettingsInfo;
			ppInstCreateNext = &layerSettingsInfo.pNext;

		}

		RKIT_VK_CHECK(m_vkAPI.vkCreateInstance(&instCreateInfo, m_allocationCallbacks->GetCallbacks(), &m_vkInstance));

		m_vkInstanceIsInitialized = true;

		m_instContext.m_allocCallbacks = m_allocationCallbacks->GetCallbacks();
		m_instContext.m_api = &m_vkAPI;
		m_instContext.m_owner = m_vkInstance;

		RKIT_CHECK(LoadVulkanExtensionAPIs());

		return rkit::ResultCode::kOK;
	}

	void RenderVulkanDriver::ShutdownDriver()
	{
		m_debugUtils.Reset();

		if (m_vkInstanceIsInitialized)
			m_vkAPI.vkDestroyInstance(m_vkInstance, GetAllocCallbacks());

		m_vkLibrary.Reset();
		m_extensions.Reset();
	}
}

RKIT_IMPLEMENT_MODULE("RKit", "Render_Vulkan", ::rkit::render::vulkan::RenderVulkanModule)
