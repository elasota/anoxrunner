#pragma once

#include "IncludeVulkan.h"

#include "rkit/Core/StringView.h"

namespace rkit { namespace render { namespace vulkan
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

#define RKIT_VK_API_LABEL(label)	\
	private:\
	inline static void ResolveFunctionLoaderForLabel_ ## label(FunctionLoaderInfo &loaderInfo)\
	{\
		ThisType_t::ResolveFunctionLoaderForLine(FunctionLoaderLineTag<__LINE__ - 1>(), loaderInfo);\
	}

#define RKIT_VK_API_GOTO(label)	\
	private:\
	inline static void ResolveFunctionLoaderForLine(const FunctionLoaderLineTag<__LINE__> &lineTag, FunctionLoaderInfo &loaderInfo)\
	{\
		ThisType_t::ResolveFunctionLoaderForLabel_ ## label(loaderInfo);\
	}

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
} } } // rkit::render::vulkan
