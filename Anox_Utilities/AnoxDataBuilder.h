#pragma once

#include "rkit/Core/StringProto.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct Result;
}

namespace anox
{
	struct IUtilitiesDriver;

	namespace utils
	{
		struct IDataBuilder
		{
			virtual ~IDataBuilder() {}

			virtual rkit::Result Run(const rkit::StringView &targetName, const rkit::StringView &sourceDir, const rkit::StringView &intermedDir, const rkit::StringView &dataDir) = 0;

			static rkit::Result Create(IUtilitiesDriver *utils, rkit::UniquePtr<IDataBuilder> &outDataBuilder);
		};
	}
}
