#pragma once

#include "rkit/Core/Drivers.h"
#include "rkit/Core/PathProto.h"

#include "rkit/Render/BackendType.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct ISeekableReadStream;

	struct FileAttributes;
}

namespace anox
{
	namespace afs
	{
		struct IArchive;
	}

	struct IUtilitiesDriver : public rkit::ICustomDriver
	{
		virtual rkit::Result OpenAFSArchive(rkit::UniquePtr<rkit::ISeekableReadStream> &&stream, rkit::UniquePtr<afs::IArchive> &outArchive) = 0;
		virtual rkit::Result RunDataBuild(const rkit::StringView &targetName, const rkit::OSAbsPathView &sourceDir, const rkit::OSAbsPathView &intermedDir, const rkit::OSAbsPathView &dataDir, const rkit::OSAbsPathView &dataSourceDir, rkit::render::BackendType backendType) = 0;
	};
}
