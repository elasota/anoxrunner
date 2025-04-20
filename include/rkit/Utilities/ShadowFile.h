#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/StringProto.h"
#include "rkit/Core/Optional.h"

#include <cstdint>

namespace rkit
{
	struct ISeekableReadStream;
	struct ISeekableReadWriteStream;

	template<class T>
	class UniquePtr;
}

namespace rkit::utils
{
	struct IShadowFile
	{
		virtual Result EntryExists(const StringSliceView &str, bool &outExists) = 0;

		virtual Result TryOpenFileRead(UniquePtr<ISeekableReadStream> &outStream, const StringSliceView &str) = 0;
		virtual Result TryOpenFileReadWrite(UniquePtr<ISeekableReadWriteStream> &outStream, const StringSliceView &str, bool createIfNotExists, bool createDirectories) = 0;

		virtual Result CopyEntry(const StringSliceView &oldName, const StringSliceView &newName) = 0;
		virtual Result MoveEntry(const StringSliceView &oldName, const StringSliceView &newName) = 0;

		virtual Result DeleteEntry(const StringSliceView &str) = 0;

		virtual Result CommitChanges() = 0;
	};
}
