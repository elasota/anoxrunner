#pragma once

namespace rkit
{
	template<class T>
	class EnumMask;
}

namespace rkit::render
{
	enum class ImagePlane
	{
		kColor,
		kDepth,
		kStencil,

		kCount,
	};

	typedef EnumMask<ImagePlane> ImagePlaneMask_t;
}
