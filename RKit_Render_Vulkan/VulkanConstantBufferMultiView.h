#pragma once

#include "rkit/Core/Opaque.h"

#include "rkit/Render/ResourceTable.h"
#include "rkit/Render/ConstantBufferMultiView.h"

#include "IncludeVulkan.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	class OpaqueConstructor;
}

namespace rkit::render
{
	struct ResourceTableSpec;
	struct IBufferResource;
	struct ConstantBufferMultiViewSpec;
}

namespace rkit::render::vulkan
{
	class VulkanDeviceBase;
	class VulkanConstantBufferMultiViewImpl;

	class VulkanConstantBufferMultiView final : public Opaque<VulkanConstantBufferMultiViewImpl>, public IConstantBufferMultiView
	{
	public:
		friend class OpaqueConstructor;

		VkDescriptorSet GetDescriptorSet() const;

		static UniquePtr<VulkanConstantBufferMultiView> Create(VulkanDeviceBase &device, const ConstantBufferMultiViewSpec &spec);

	private:
		explicit VulkanConstantBufferMultiView(VulkanDeviceBase &device);
	};

} // rkit::render::vulkan
