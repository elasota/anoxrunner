#pragma once

#include "StringProto.h"

#include <cstdint>
#include <cstddef>

namespace rkit
{
	template<class T>
	class Span;

	template<class T>
	class UniquePtr;

	class UniqueThreadRef;

	struct FileAttributes;
	struct Result;

	struct IDirectoryScan;
	struct ISeekableReadStream;
	struct ISeekableReadWriteStream;
	struct ISeekableWriteStream;
	struct ISystemLibrary;
	struct IEvent;
	struct IMutex;
	struct IThread;
	struct IThreadContext;

	namespace render
	{
		struct IDisplayManager;
	}

	struct IPlatformDriver
	{
		virtual ~IPlatformDriver() {}
	};

	struct ISystemLibrary
	{
		virtual ~ISystemLibrary() {}

		virtual bool GetFunction(void *fnPtrAddress, const StringView &fnName) = 0;
	};

	enum class FileLocation
	{
		kDataSourceDirectory,		// Source directory for compilatble data
		kConfigDirectory,			// Configuration data
		kGameDirectory,				// Compiled game data directory
		kUserSettingsDirectory,
		kAbsolute,
	};

	enum class SystemLibraryType
	{
		kVulkan,
	};

	struct ISystemDriver
	{
		virtual ~ISystemDriver() {}

		virtual void RemoveCommandLineArgs(size_t firstArg, size_t numArgs) = 0;
		virtual Span<const StringView> GetCommandLine() const = 0;
		virtual void AssertionFailure(const char *expr, const char *file, unsigned int line) = 0;
		virtual void FirstChanceResultFailure(const Result &result) = 0;

		virtual UniquePtr<ISeekableReadStream> OpenFileRead(FileLocation location, const char *path) = 0;
		virtual UniquePtr<ISeekableWriteStream> OpenFileWrite(FileLocation location, const char *path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) = 0;
		virtual UniquePtr<ISeekableReadWriteStream> OpenFileReadWrite(FileLocation location, const char *path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) = 0;

		virtual Result CreateThread(UniqueThreadRef &outThread, UniquePtr<IThreadContext> &&threadContext) = 0;
		virtual Result CreateMutex(UniquePtr<IMutex> &outMutex) = 0;
		virtual Result CreateEvent(UniquePtr<IEvent> &outEvent, bool autoReset, bool startSignaled) = 0;
		virtual void SleepMSec(uint32_t msec) const = 0;

		virtual Result OpenDirectoryScan(FileLocation location, const char *path, UniquePtr<IDirectoryScan> &outDirectoryScan) = 0;
		virtual Result GetFileAttributes(FileLocation location, const char *path, bool &outExists, FileAttributes &outAttribs) = 0;

		virtual Result SetGameDirectoryOverride(const StringView &path) = 0;
		virtual Result SetSettingsDirectory(const StringView &path) = 0;
		virtual char GetPathSeparator() const = 0;

		virtual IPlatformDriver *GetPlatformDriver() const = 0;

		virtual Result OpenSystemLibrary(UniquePtr<ISystemLibrary> &outLibrary, SystemLibraryType libType) const = 0;

		virtual uint32_t GetProcessorCount() const = 0;

		virtual render::IDisplayManager *GetDisplayManager() const = 0;
	};
}
