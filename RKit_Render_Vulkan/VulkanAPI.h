#pragma once

#include "VulkanAPI_Common.h"

namespace rkit { namespace render { namespace vulkan
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
		RKIT_VK_API(vkGetPhysicalDeviceMemoryProperties);
		RKIT_VK_API(vkCreateDevice);
		RKIT_VK_API(vkDestroyDevice);
		RKIT_VK_API(vkGetDeviceProcAddr);
		RKIT_VK_API_EXT(vkCreateDebugUtilsMessengerEXT, VK_EXT_debug_utils);
		RKIT_VK_API_EXT(vkDestroyDebugUtilsMessengerEXT, VK_EXT_debug_utils);
		RKIT_VK_API_EXT(vkDestroySurfaceKHR, VK_KHR_surface);
		RKIT_VK_API_EXT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, VK_KHR_surface);
		RKIT_VK_API_EXT(vkGetPhysicalDeviceSurfaceSupportKHR, VK_KHR_surface);
		RKIT_VK_API(vkDestroyInstance);	// This must be last!
		RKIT_VK_API_END;
	};

	struct VulkanDeviceAPI : public IVulkanAPI
	{
		RKIT_VK_API_START(VulkanDeviceAPI);
		RKIT_VK_API(vkQueueSubmit);
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
		RKIT_VK_API(vkCreateImage);
		RKIT_VK_API(vkDestroyImage);
		RKIT_VK_API(vkBindImageMemory);
		RKIT_VK_API(vkGetImageMemoryRequirements);
		RKIT_VK_API(vkDeviceWaitIdle);
		RKIT_VK_API(vkWaitForFences);
		RKIT_VK_API(vkResetFences);
		RKIT_VK_API(vkAllocateCommandBuffers);
		RKIT_VK_API(vkFreeCommandBuffers);
		RKIT_VK_API(vkBeginCommandBuffer);
		RKIT_VK_API(vkResetCommandBuffer);
		RKIT_VK_API(vkEndCommandBuffer);
		RKIT_VK_API(vkCreateCommandPool);
		RKIT_VK_API(vkDestroyCommandPool);
		RKIT_VK_API(vkResetCommandPool);
		RKIT_VK_API(vkCreateFramebuffer);
		RKIT_VK_API(vkDestroyFramebuffer);
		RKIT_VK_API(vkCreateImageView);
		RKIT_VK_API(vkDestroyImageView);
		RKIT_VK_API(vkCmdBeginRenderPass);
		RKIT_VK_API(vkCmdClearAttachments);
		RKIT_VK_API(vkCmdEndRenderPass);
		RKIT_VK_API(vkCmdPipelineBarrier);
		RKIT_VK_API(vkCmdCopyBufferToImage);
		RKIT_VK_API(vkAllocateMemory);
		RKIT_VK_API(vkFreeMemory);
		RKIT_VK_API(vkMapMemory);
		RKIT_VK_API(vkUnmapMemory);
		RKIT_VK_API(vkCreateBuffer);
		RKIT_VK_API(vkDestroyBuffer);
		RKIT_VK_API(vkGetBufferMemoryRequirements);
		RKIT_VK_API(vkBindBufferMemory);
		RKIT_VK_API_EXT(vkCreateSwapchainKHR, VK_KHR_swapchain);
		RKIT_VK_API_EXT(vkDestroySwapchainKHR, VK_KHR_swapchain);
		RKIT_VK_API_EXT(vkGetSwapchainImagesKHR, VK_KHR_swapchain);
		RKIT_VK_API_EXT(vkAcquireNextImageKHR, VK_KHR_swapchain);
		RKIT_VK_API_EXT(vkQueuePresentKHR, VK_KHR_swapchain);
		RKIT_VK_API_END;
	};
} } } // rkit::render::vulkan
