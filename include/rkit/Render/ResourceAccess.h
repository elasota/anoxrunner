#pragma once

namespace rkit
{
	template<class T>
	class EnumMask;
}

namespace rkit { namespace render
{
	enum class ResourceAccess
	{
		kRenderTarget,
		kPresentSource,

		kCount,
	};

	typedef EnumMask<ResourceAccess> ResourceAccessMask_t;
} } // rkit::render
