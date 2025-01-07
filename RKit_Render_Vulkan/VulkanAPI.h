#pragma once

#include "VulkanAPI_Common.h"

namespace rkit::render::vulkan
{
	struct VulkanGlobalAPI : public IVulkanAPI
	{
		RKIT_VK_API_START(VulkanGlobalAPI);
		RKIT_VK_API(vkEnumerateInstanceVersion);
		RKIT_VK_API(vkCreateInstance);
		RKIT_VK_API(vkGetInstanceProcAddr);
		RKIT_VK_API(vkEnumerateInstanceExtensionProperties);
		RKIT_VK_API(vkEnumerateInstanceLayerProperties);
		RKIT_VK_API_END;
	};

	struct VulkanInstanceAPI : public IVulkanAPI
	{
		RKIT_VK_API_START(VulkanInstanceAPI);
		RKIT_VK_API(vkGetPhysicalDeviceFeatures);
		RKIT_VK_API(vkEnumeratePhysicalDevices);
		RKIT_VK_API(vkEnumerateDeviceExtensionProperties);
		RKIT_VK_API(vkGetPhysicalDeviceProperties);
		RKIT_VK_API(vkGetPhysicalDeviceQueueFamilyProperties);
		RKIT_VK_API(vkCreateDevice);
		RKIT_VK_API(vkDestroyDevice);
		RKIT_VK_API(vkGetDeviceProcAddr);
		RKIT_VK_API_EXT(vkCreateDebugUtilsMessengerEXT, VK_EXT_debug_utils);
		RKIT_VK_API_EXT(vkDestroyDebugUtilsMessengerEXT, VK_EXT_debug_utils);
		RKIT_VK_API_EXT(vkDestroySurfaceKHR, VK_KHR_surface);
		RKIT_VK_API_EXT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, VK_KHR_surface);
		RKIT_VK_API(vkDestroyInstance);	// This must be last!
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
		RKIT_VK_API(vkDestroyPipeline);
		RKIT_VK_API(vkCreatePipelineCache);
		RKIT_VK_API(vkDestroyPipelineCache);
		RKIT_VK_API(vkCreateGraphicsPipelines);
		RKIT_VK_API(vkCreateShaderModule);
		RKIT_VK_API(vkDestroyShaderModule);
		RKIT_VK_API(vkCreateDescriptorSetLayout);
		RKIT_VK_API(vkDestroyDescriptorSetLayout);
		RKIT_VK_API(vkCreateSampler);
		RKIT_VK_API(vkDestroySampler);
		RKIT_VK_API(vkCreatePipelineLayout);
		RKIT_VK_API(vkDestroyPipelineLayout);
		RKIT_VK_API(vkCreateRenderPass);
		RKIT_VK_API(vkDestroyRenderPass);
		RKIT_VK_API(vkGetPipelineCacheData);
		RKIT_VK_API(vkMergePipelineCaches);
		RKIT_VK_API_EXT(vkCreateSwapchainKHR, VK_KHR_swapchain);
		RKIT_VK_API_EXT(vkDestroySwapchainKHR, VK_KHR_swapchain);
		RKIT_VK_API_END;
	};
}
