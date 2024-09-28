#pragma once

#include "rkit/Core/Drivers.h"

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
	};
}
