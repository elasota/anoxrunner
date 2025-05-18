#pragma once

namespace rkit
{
	template<class T>
	class EnumMask;
}

namespace rkit { namespace render
{
	enum class ImagePlane
	{
		kColor,
		kDepth,
		kStencil,

		kCount,
	};

	typedef EnumMask<ImagePlane> ImagePlaneMask_t;
} } // rkit::render
