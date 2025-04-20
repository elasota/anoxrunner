#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/PathProto.h"
#include "rkit/Core/StringProto.h"

#include "rkit/Render/BackendType.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace anox
{
	struct IUtilitiesDriver;

	namespace utils
	{
		struct IDataBuilder
		{
			virtual ~IDataBuilder() {}

			virtual rkit::Result Run(const rkit::StringView &targetName, const rkit::OSAbsPathView &sourceDir, const rkit::OSAbsPathView &intermedDir, const rkit::OSAbsPathView &dataDir, const rkit::OSAbsPathView &dataSourceDir, rkit::render::BackendType backendType) = 0;

			static rkit::Result Create(IUtilitiesDriver *utils, rkit::UniquePtr<IDataBuilder> &outDataBuilder);
		};
	}
}
