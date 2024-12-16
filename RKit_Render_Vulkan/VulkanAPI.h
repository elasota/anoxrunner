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
	struct VulkanAPI;

	typedef void (*ResolveFunctionCallback_t)(VulkanAPI *ftable, FunctionLoaderInfo &loaderInfo);
	typedef void (*CopyVoidFunctionCallback_t)(void *pfnAddress, PFN_vkVoidFunction voidFunction);

	struct FunctionLoaderInfo
	{
		ResolveFunctionCallback_t m_nextCallback;
		CopyVoidFunctionCallback_t m_copyVoidFunctionCallback;
		void *m_pfnAddress;
		bool m_isOptional;
		bool m_isInstance;
		const char *m_requiredExtension;
		StringView m_fnName;
	};

#define RKIT_VK_API_WITH_PROPERTIES(fn, isOptional, isInstance, requiredExtension)\
	private:\
	inline static void ResolveFunctionInfo_ ## fn(VulkanAPI *ftable, FunctionLoaderInfo& loaderInfo)\
	{\
		loaderInfo.m_pfnAddress = &ftable->fn;\
		loaderInfo.m_fnName = StringView(#fn);\
		loaderInfo.m_isOptional = isOptional;\
		loaderInfo.m_isInstance = isInstance;\
		loaderInfo.m_requiredExtension = requiredExtension;\
		loaderInfo.m_copyVoidFunctionCallback = WriteVoidFunctionTemplate<PFN_ ## fn>;\
		VulkanAPI::ResolveFunctionLoaderForLine(FunctionLoaderLineTag<__LINE__ - 1>(), loaderInfo);\
	}\
	inline static void ResolveFunctionLoaderForLine(const FunctionLoaderLineTag<__LINE__> &lineTag, FunctionLoaderInfo &loaderInfo)\
	{\
		loaderInfo.m_nextCallback = ResolveFunctionInfo_ ## fn;\
	}\
	public:\
		PFN_ ## fn fn = static_cast<PFN_ ## fn>(nullptr)

#define RKIT_VK_API_GLOBAL(fn) RKIT_VK_API_WITH_PROPERTIES(fn, false, false, nullptr)
#define RKIT_VK_API(fn) RKIT_VK_API_WITH_PROPERTIES(fn, false, true, nullptr)
#define RKIT_VK_API_EXT(fn, ext) RKIT_VK_API_WITH_PROPERTIES(fn, false, true, #ext)

	struct VulkanAPI
	{
	private:
		template<class T>
		inline static void WriteVoidFunctionTemplate(void *pfnAddress, PFN_vkVoidFunction voidFunction)
		{
			*static_cast<T *>(pfnAddress) = reinterpret_cast<T>(voidFunction);
		}

		static void TerminalResolveFunctionLoader(FunctionLoaderInfo &loaderInfo);

		inline static void ResolveFunctionLoaderForLine(const FunctionLoaderLineTag<__LINE__ + 4> &lineTag, FunctionLoaderInfo &loaderInfo)
		{
			return TerminalResolveFunctionLoader(loaderInfo);
		}

		RKIT_VK_API_GLOBAL(vkCreateInstance);
		RKIT_VK_API_GLOBAL(vkDestroyInstance);
		RKIT_VK_API_GLOBAL(vkEnumerateInstanceExtensionProperties);
		RKIT_VK_API_GLOBAL(vkEnumerateInstanceLayerProperties);
		RKIT_VK_API_GLOBAL(vkGetInstanceProcAddr);
		RKIT_VK_API(vkEnumeratePhysicalDevices);
		RKIT_VK_API(vkGetPhysicalDeviceProperties);
		RKIT_VK_API_EXT(vkCreateDebugUtilsMessengerEXT, VK_EXT_debug_utils);
		RKIT_VK_API_EXT(vkDestroyDebugUtilsMessengerEXT, VK_EXT_debug_utils);

		inline static void InitialResolveFunctionLoader(FunctionLoaderInfo &loaderInfo)
		{
			ResolveFunctionLoaderForLine(FunctionLoaderLineTag<__LINE__ - 4>(), loaderInfo);
		}

	public:
		inline static ResolveFunctionCallback_t GetFirstResolveFunctionCallback()
		{
			FunctionLoaderInfo loaderInfo = {};
			InitialResolveFunctionLoader(loaderInfo);
			return loaderInfo.m_nextCallback;
		}
	};
}
