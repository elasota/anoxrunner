#pragma once

namespace rkit::render::vulkan
{
	enum class DescriptorHeapType
	{
		Texture,
		RWTexture,
		Sampler,
		Buffer,

		Count,
	};
}
