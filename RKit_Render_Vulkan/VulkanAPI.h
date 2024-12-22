#pragma once

#include "IncludeVulkan.h"

#include "rkit/Core/StringView.h"

namespace rkit::render::vulkan
{
	template<int TLine>
	struct FunctionLoaderLineTag
	{
	};

	struct FunctionLoaderInfo;
	struct IVulkanAPI;

	typedef void (*ResolveFunctionCallback_t)(IVulkanAPI *ftable, FunctionLoaderInfo &loaderInfo);
	typedef void (*CopyVoidFunctionCallback_t)(void *pfnAddress, PFN_vkVoidFunction voidFunction);

	struct IVulkanAPI
	{
		virtual ResolveFunctionCallback_t GetFirstResolveFunctionCallback() const = 0;
	};

	struct FunctionLoaderInfo
	{
		ResolveFunctionCallback_t m_nextCallback = nullptr;
		CopyVoidFunctionCallback_t m_copyVoidFunctionCallback = nullptr;
		void *m_pfnAddress = nullptr;
		bool m_isOptional = false;
		const char *m_requiredExtension = nullptr;
		StringView m_fnName;
	};

#define RKIT_VK_API_WITH_PROPERTIES(fn, isOptional, requiredExtension)\
	private:\
	inline static void ResolveFunctionInfo_ ## fn(IVulkanAPI *ftable, FunctionLoaderInfo& loaderInfo)\
	{\
		ThisType_t *ftableTyped = static_cast<ThisType_t*>(ftable);\
		loaderInfo.m_pfnAddress = &ftableTyped->fn;\
		loaderInfo.m_fnName = StringView(#fn);\
		loaderInfo.m_isOptional = isOptional;\
		loaderInfo.m_requiredExtension = requiredExtension;\
		loaderInfo.m_copyVoidFunctionCallback = WriteVoidFunctionTemplate<PFN_ ## fn>;\
		ThisType_t::ResolveFunctionLoaderForLine(FunctionLoaderLineTag<__LINE__ - 1>(), loaderInfo);\
	}\
	inline static void ResolveFunctionLoaderForLine(const FunctionLoaderLineTag<__LINE__> &lineTag, FunctionLoaderInfo &loaderInfo)\
	{\
		loaderInfo.m_nextCallback = ResolveFunctionInfo_ ## fn;\
	}\
	public:\
		PFN_ ## fn fn = static_cast<PFN_ ## fn>(nullptr)

#define RKIT_VK_API(fn) RKIT_VK_API_WITH_PROPERTIES(fn, false, nullptr)
#define RKIT_VK_API_EXT(fn, ext) RKIT_VK_API_WITH_PROPERTIES(fn, false, #ext)

#define RKIT_VK_API_START(type)	\
private:\
	template<class T>\
	inline static void WriteVoidFunctionTemplate(void *pfnAddress, PFN_vkVoidFunction voidFunction)\
	{\
		*static_cast<T *>(pfnAddress) = reinterpret_cast<T>(voidFunction);\
	}\
	inline static void TerminalResolveFunctionLoader(FunctionLoaderInfo &loaderInfo)\
	{\
		loaderInfo.m_nextCallback = nullptr;\
	}\
	inline static void ResolveFunctionLoaderForLine(const FunctionLoaderLineTag<__LINE__> &lineTag, FunctionLoaderInfo &loaderInfo)\
	{\
		return TerminalResolveFunctionLoader(loaderInfo);\
	}\
	typedef type ThisType_t

#define RKIT_VK_API_END	\
	inline static void InitialResolveFunctionLoader(FunctionLoaderInfo &loaderInfo)\
	{\
		ResolveFunctionLoaderForLine(FunctionLoaderLineTag<__LINE__ - 1>(), loaderInfo);\
	}\
public:\
	inline ResolveFunctionCallback_t GetFirstResolveFunctionCallback() const override\
	{\
		FunctionLoaderInfo loaderInfo = {};\
		InitialResolveFunctionLoader(loaderInfo);\
		return loaderInfo.m_nextCallback;\
	}

	struct VulkanGlobalAPI : public IVulkanAPI
	{
		RKIT_VK_API_START(VulkanGlobalAPI);
		RKIT_VK_API(vkCreateInstance);
		RKIT_VK_API(vkDestroyInstance);
		RKIT_VK_API(vkEnumerateInstanceExtensionProperties);
		RKIT_VK_API(vkEnumerateInstanceLayerProperties);
		RKIT_VK_API(vkGetInstanceProcAddr);
		RKIT_VK_API_END;
	};

	struct VulkanInstanceAPI : public IVulkanAPI
	{
		RKIT_VK_API_START(VulkanInstanceAPI);
		RKIT_VK_API(vkEnumeratePhysicalDevices);
		RKIT_VK_API(vkGetPhysicalDeviceProperties);
		RKIT_VK_API(vkGetPhysicalDeviceQueueFamilyProperties);
		RKIT_VK_API(vkCreateDevice);
		RKIT_VK_API(vkDestroyDevice);
		RKIT_VK_API(vkGetDeviceProcAddr);
		RKIT_VK_API_EXT(vkCreateDebugUtilsMessengerEXT, VK_EXT_debug_utils);
		RKIT_VK_API_EXT(vkDestroyDebugUtilsMessengerEXT, VK_EXT_debug_utils);
		RKIT_VK_API_END;
	};

	struct VulkanDeviceAPI : public IVulkanAPI
	{
		RKIT_VK_API_START(VulkanDeviceAPI);
		RKIT_VK_API(vkGetDeviceQueue);
		RKIT_VK_API(vkCreateSemaphore);
		RKIT_VK_API(vkDestroySemaphore);
		RKIT_VK_API(vkCreateFence);
		RKIT_VK_API(vkDestroyFence);
		RKIT_VK_API_END;
	};
}
