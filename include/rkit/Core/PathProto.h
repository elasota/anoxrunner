#pragma once

#include <cstdint>

namespace rkit
{
	namespace PathTraitFlags
	{
		enum PathTraitFlag
		{
			kIsCaseInsensitive	= 0x01,
			kAllowWildcards		= 0x02,
			kAllowCWD			= 0x04,
			kAllowParent		= 0x08,
		};
	}

	template<bool TIsAbsolute, class TPathTraits>
	class BasePath;

	template<bool TIsAbsolute, class TPathTraits>
	class BasePathView;

	template<bool TIsAbsolute, class TPathTraits>
	class BasePathIterator;

	template<uint32_t TTraitFlags>
	struct DefaultPathTraits;

	struct OSPathTraits;

	typedef DefaultPathTraits<PathTraitFlags::kIsCaseInsensitive> DefaultCaseInsensitivePathTraits;
	typedef DefaultPathTraits<0> DefaultCaseSensitivePathTraits;

	typedef BasePath<true, OSPathTraits> OSAbsPath;
	typedef BasePathView<true, OSPathTraits> OSAbsPathView;
	typedef BasePath<false, OSPathTraits> OSRelPath;
	typedef BasePathView<false, OSPathTraits> OSRelPathView;

	typedef BasePath<false, DefaultCaseInsensitivePathTraits> CIPath;
	typedef BasePathView<false, DefaultCaseInsensitivePathTraits> CIPathView;
	typedef BasePathIterator<false, DefaultCaseInsensitivePathTraits> CIPathIterator;

	typedef BasePath<false, DefaultCaseSensitivePathTraits> CSPath;
	typedef BasePathView<false, DefaultCaseSensitivePathTraits> CSPathView;
	typedef BasePathIterator<false, DefaultCaseSensitivePathTraits> CSPathIterator;
}
