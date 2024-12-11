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

	struct FunctionLoaderInfo
	{
		ResolveFunctionCallback_t m_nextCallback;
		void *m_pfnAddress;
		bool m_isOptional;
		StringView m_fnName;
	};

#define RKIT_VK_API_WITH_OPTIONALITY(fn, isOptional)\
	public:\
		PFN_ ## fn fn;\
	private:\
	inline static void ResolveFunctionInfo_ ## fn(VulkanAPI *ftable, FunctionLoaderInfo& loaderInfo)\
	{\
		loaderInfo.m_pfnAddress = &ftable->fn;\
		loaderInfo.m_fnName = StringView(#fn);\
		loaderInfo.m_isOptional = isOptional;\
		VulkanAPI::ResolveFunctionLoaderForLine(FunctionLoaderLineTag<__LINE__ - 1>(), loaderInfo);\
	}\
	inline static void ResolveFunctionLoaderForLine(const FunctionLoaderLineTag<__LINE__> &lineTag, FunctionLoaderInfo &loaderInfo)\
	{\
		loaderInfo.m_nextCallback = ResolveFunctionInfo_ ## fn;\
	}

#define RKIT_VK_API(fn) RKIT_VK_API_WITH_OPTIONALITY(fn, false)
#define RKIT_VK_API_OPTIONAL(fn) RKIT_VK_API_WITH_OPTIONALITY(fn, true)

	struct VulkanAPI
	{
	private:
		static void TerminalResolveFunctionLoader(FunctionLoaderInfo &loaderInfo);

		inline static void ResolveFunctionLoaderForLine(const FunctionLoaderLineTag<__LINE__ + 4> &lineTag, FunctionLoaderInfo &loaderInfo)
		{
			return TerminalResolveFunctionLoader(loaderInfo);
		}

		RKIT_VK_API(vkCreateInstance)
		RKIT_VK_API(vkDestroyInstance)

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
