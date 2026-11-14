#pragma once

#include "rkit/Core/Opaque.h"
#include "rkit/Render/ResourceTable.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	class OpaqueConstructor;
}

namespace rkit::render
{
	struct ResourceTableSpec;
}

namespace rkit::render::vulkan
{
	class VulkanDeviceBase;
	class HeterogeneousResourceTableImpl;

	class ResourceTableBase : public IResourceTable
	{
	public:
		virtual ~ResourceTableBase() {}
	};

	class HeterogeneousResourceTable final : public ResourceTableBase, public Opaque<HeterogeneousResourceTableImpl>
	{
	public:
		enum class DescriptorSetType
		{
			Texture,
			RWTexture,
			Buffer,
			Sampler,

			Count,
		};

		friend class OpaqueConstructor;

		static UniquePtr<HeterogeneousResourceTable> Create(VulkanDeviceBase &device, const ResourceTableSpec &spec);

	private:
		explicit HeterogeneousResourceTable(VulkanDeviceBase &device);
	};

} // rkit::render::vulkan
