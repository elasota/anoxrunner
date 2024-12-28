#pragma once

#include "rkit/Core/StringProtos.h"
#include "rkit/Core/Optional.h"

#include <cstdint>

namespace rkit
{
	struct Result;

	struct ISeekableReadStream;
	struct ISeekableReadWriteStream;

	template<class T>
	class UniquePtr;
}

namespace rkit::utils
{
	struct IShadowFile
	{
		virtual Result TryOpenFileRead(UniquePtr<ISeekableReadStream> &outStream, const StringSliceView &str) = 0;
		virtual Result TryOpenFileReadWrite(UniquePtr<ISeekableReadWriteStream> &outStream, const StringSliceView &str) = 0;

		virtual Result DeleteEntry(const StringSliceView &str) = 0;

		virtual Result CommitChanges() = 0;
	};
}
