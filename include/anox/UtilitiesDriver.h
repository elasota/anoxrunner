#pragma once

#include "rkit/Core/Drivers.h"

#include "rkit/Render/BackendType.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct ISeekableReadStream;
	struct Result;
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
		virtual rkit::Result RunDataBuild(const rkit::StringView &targetName, const rkit::StringView &sourceDir, const rkit::StringView &intermedDir, const rkit::StringView &dataDir, rkit::render::BackendType backendType) = 0;
	};
}
