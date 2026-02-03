#pragma once

#include "FutureProtos.h"
#include "PathProto.h"
#include "StringProto.h"
#include "StreamProtos.h"

#include <cstdint>
#include <cstddef>

namespace rkit
{
	template<class T>
	class Span;

	template<class T>
	class UniquePtr;

	template<class T>
	class RCPtr;

	template<class T>
	class Future;

	class Job;

	class UniqueThreadRef;

	struct FileAttributes;

	struct IDirectoryScan;
	struct IJobQueue;
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

		virtual bool GetFunction(void *fnPtrAddress, const AsciiStringView &fnName) = 0;
	};

	enum class FileLocation
	{
		kProgramDirectory,			// Same directory as the program
		kGameDirectory,				// Compiled game data directory
		kUserSettingsDirectory,
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
		virtual void FirstChanceResultFailure(PackedResultAndExtCode result) = 0;

		virtual Result AsyncOpenFileRead(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<UniquePtr<ISeekableReadStream>> &outStream, FileLocation location, const CIPathView &path, bool allowFailure) = 0;
		virtual Result AsyncOpenFileReadAbs(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<UniquePtr<ISeekableReadStream>> &outStream, const OSAbsPathView &path, bool allowFailure) = 0;

		virtual Result AsyncOpenFileAsyncRead(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<AsyncFileOpenReadResult> &outStream, FileLocation location, const CIPathView &path, bool allowFailure) = 0;
		virtual Result AsyncOpenFileAsyncReadAbs(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<AsyncFileOpenReadResult> &outStream, const OSAbsPathView &path, bool allowFailure) = 0;

		virtual Result OpenFileRead(UniquePtr<ISeekableReadStream> &outStream, FileLocation location, const CIPathView &path, bool allowFailure) = 0;
		virtual Result OpenFileReadAbs(UniquePtr<ISeekableReadStream> &outStream, const OSAbsPathView &path, bool allowFailure) = 0;
		virtual Result OpenFileAsyncRead(AsyncFileOpenReadResult &outResult, FileLocation location, const CIPathView &path, bool allowFailure) = 0;
		virtual Result OpenFileAsyncReadAbs(AsyncFileOpenReadResult &outResult, const OSAbsPathView &path, bool allowFailure) = 0;
		virtual Result OpenFileWrite(UniquePtr<ISeekableWriteStream> &outStream, FileLocation location, const CIPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists, bool allowFailure) = 0;
		virtual Result OpenFileWriteAbs(UniquePtr<ISeekableWriteStream> &outStream, const OSAbsPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists, bool allowFailure) = 0;
		virtual Result OpenFileReadWrite(UniquePtr<ISeekableReadWriteStream> &outStream, FileLocation location, const CIPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists, bool allowFailure) = 0;
		virtual Result OpenFileReadWriteAbs(UniquePtr<ISeekableReadWriteStream> &outStream, const OSAbsPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists, bool allowFailure) = 0;

		virtual Result CreateThread(UniqueThreadRef &outThread, UniquePtr<IThreadContext> &&threadContext, const StringView &threadName) = 0;
		virtual Result CreateMutex(UniquePtr<IMutex> &outMutex) = 0;
		virtual Result CreateEvent(UniquePtr<IEvent> &outEvent, bool autoReset, bool startSignaled) = 0;
		virtual void SleepMSec(uint32_t msec) const = 0;

		virtual Result OpenDirectoryScan(UniquePtr<IDirectoryScan> &outDirectoryScan, FileLocation location, const CIPathView &path, bool allowFailure) = 0;
		virtual Result OpenDirectoryScanAbs(UniquePtr<IDirectoryScan> &outDirectoryScan, const OSAbsPathView &path, bool allowFailure) = 0;
		virtual Result GetFileAttributes(bool &outSucceeded, bool &outExists, FileAttributes &outAttribs, FileLocation location, const CIPathView &path, bool allowFailure) = 0;
		virtual Result GetFileAttributesAbs(bool &outSucceeded, bool &outExists, FileAttributes &outAttribs, const OSAbsPathView &path, bool allowFailure) = 0;

		virtual Result CopyFileFromAbsToAbs(bool &outSucceeded, const OSAbsPathView &srcPath, const OSAbsPathView &destPath, bool overwrite, bool allowFailure) = 0;
		virtual Result CopyFileToAbs(bool &outSucceeded, FileLocation location, const CIPathView &path, const OSAbsPathView &destPath, bool overwrite, bool allowFailure) = 0;
		virtual Result CopyFileFromAbs(bool &outSucceeded, const OSAbsPathView &srcPath, FileLocation location, const CIPathView &path, bool overwrite, bool allowFailure) = 0;
		virtual Result CopyFile(bool &outSucceeded, FileLocation srcLocation, const CIPathView &srcPath, FileLocation destLocation, const CIPathView &destPath, bool overwrite, bool allowFailure) = 0;

		virtual Result MoveFileFromAbsToAbs(bool &outSucceeded, const OSAbsPathView &srcPath, const OSAbsPathView &destPath, bool overwrite, bool allowFailure) = 0;
		virtual Result MoveFileToAbs(bool &outSucceeded, FileLocation location, const CIPathView &path, const OSAbsPathView &destPath, bool overwrite, bool allowFailure) = 0;
		virtual Result MoveFileFromAbs(bool &outSucceeded, const OSAbsPathView &srcPath, FileLocation location, const CIPathView &path, bool overwrite, bool allowFailure) = 0;
		virtual Result MoveFile(bool &outSucceeded, FileLocation srcLocation, const CIPathView &srcPath, FileLocation destLocation, const CIPathView &destPath, bool overwrite, bool allowFailure) = 0;

		virtual Result SetGameDirectoryOverride(const OSAbsPathView &path) = 0;
		virtual Result SetSettingsDirectory(const StringView &path) = 0;
		virtual char GetPathSeparator() const = 0;

		virtual IPlatformDriver *GetPlatformDriver() const = 0;

		virtual Result OpenSystemLibrary(UniquePtr<ISystemLibrary> &outLibrary, SystemLibraryType libType) const = 0;

		virtual uint32_t GetProcessorCount() const = 0;

		virtual render::IDisplayManager *GetDisplayManager() const = 0;

	};
}
