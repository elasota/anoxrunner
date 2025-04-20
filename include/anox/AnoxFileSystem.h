#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/PathProto.h"

namespace rkit
{
	struct ISeekableReadStream;

	template<class T>
	class UniquePtr;
}

namespace anox
{
	struct IGameDataFileSystem
	{
		virtual ~IGameDataFileSystem() {}

		virtual rkit::Result OpenNamedFile(rkit::UniquePtr<rkit::ISeekableReadStream> &stream, const rkit::CIPathView &path) = 0;
	};
}
