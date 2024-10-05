#pragma once

#include "StringProto.h"

#include <cstddef>

namespace rkit
{
	template<class T>
	class Span;

	template<class T>
	class UniquePtr;

	struct ISeekableReadStream;
	struct ISeekableReadWriteStream;
	struct ISeekableWriteStream;

	enum class FileLocation
	{
		kGameDirectory,
		kAbsolute,
	};

	struct ISystemDriver
	{
		virtual ~ISystemDriver() {}

		virtual void RemoveCommandLineArgs(size_t firstArg, size_t numArgs) = 0;
		virtual Span<const StringView> GetCommandLine() const = 0;
		virtual void AssertionFailure(const char *expr, const char *file, unsigned int line) = 0;

		virtual UniquePtr<ISeekableReadStream> OpenFileRead(FileLocation location, const char *path) = 0;
		virtual UniquePtr<ISeekableWriteStream> OpenFileWrite(FileLocation location, const char *path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) = 0;
		virtual UniquePtr<ISeekableReadWriteStream> OpenFileReadWrite(FileLocation location, const char *path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) = 0;

		virtual char GetPathSeparator() const = 0;
	};
}
