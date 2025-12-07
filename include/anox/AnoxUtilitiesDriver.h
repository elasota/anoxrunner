#pragma once

#include "rkit/Core/Drivers.h"
#include "rkit/Core/PathProto.h"

#include "rkit/Render/BackendType.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	template<class T>
	class Span;

	struct ISeekableReadStream;

	struct FileAttributes;
}

namespace anox
{
	namespace afs
	{
		struct IArchive;
	}

	namespace data
	{
		struct EntityDefsSchema;
	}

	struct IUtilitiesDriver : public rkit::ICustomDriver
	{
		virtual rkit::Result OpenAFSArchive(rkit::UniquePtr<rkit::ISeekableReadStream> &&stream, rkit::UniquePtr<afs::IArchive> &outArchive) = 0;
		virtual rkit::Result RunDataBuild(const rkit::StringView &targetName, const rkit::OSAbsPathView &sourceDir, const rkit::OSAbsPathView &intermedDir, const rkit::OSAbsPathView &dataDir, const rkit::OSAbsPathView &dataSourceDir, rkit::render::BackendType backendType) = 0;

		virtual const data::EntityDefsSchema &GetEntityDefs() const = 0;
	};
}
