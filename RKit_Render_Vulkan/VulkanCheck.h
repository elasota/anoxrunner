#pragma once

#include "rkit/Core/CoreDefs.h"

#include "IncludeVulkanCore.h"

namespace rkit::render::vulkan::results
{
	Result FirstChanceVulkanFailure(VkResult result);

	template<class T>
	struct ResultTypeEnforcer
	{
	};

	template<>
	struct ResultTypeEnforcer<VkResult>
	{
		inline static VkResult PassThroughResult(VkResult result)
		{
			return result;
		}
	};

	template<class T>
	inline T PassThroughResult(const T &result)
	{
		return ResultTypeEnforcer<T>::PassThroughResult(result);
	}
}

#define RKIT_VK_CHECK(expr) do {\
	VkResult RKIT_PP_CONCAT(exprResult_, __LINE__) = ::rkit::render::vulkan::results::PassThroughResult((expr));\
	if (RKIT_PP_CONCAT(exprResult_, __LINE__) != VK_SUCCESS)\
		return ::rkit::render::vulkan::results::FirstChanceVulkanFailure(RKIT_PP_CONCAT(exprResult_, __LINE__));\
} while (false)
