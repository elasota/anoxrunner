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
		virtual void FirstChanceResultFailure(ResultCode result) = 0;

		Result TryAsyncTryOpenFileRead(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<UniquePtr<ISeekableReadStream>> &outStream, FileLocation location, const CIPathView &path);
		Result TryAsyncTryOpenFileReadAbs(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<UniquePtr<ISeekableReadStream>> &outStream, const OSAbsPathView &path);

		virtual Result TryAsyncOpenFileAsyncRead(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<AsyncFileOpenReadResult> &outStream, FileLocation location, const CIPathView &path) = 0;
		virtual Result TryAsyncOpenFileAsyncReadAbs(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<AsyncFileOpenReadResult> &outStream, const OSAbsPathView &path) = 0;

		virtual Result TryOpenFileRead(UniquePtr<ISeekableReadStream> &outStream, FileLocation location, const CIPathView &path) = 0;
		virtual Result TryOpenFileReadAbs(UniquePtr<ISeekableReadStream> &outStream, const OSAbsPathView &path) = 0;
		virtual Result TryOpenFileAsyncRead(AsyncFileOpenReadResult &outResult, FileLocation location, const CIPathView &path) = 0;
		virtual Result TryOpenFileAsyncReadAbs(AsyncFileOpenReadResult &outResult, const OSAbsPathView &path) = 0;
		virtual Result TryOpenFileWrite(UniquePtr<ISeekableWriteStream> &outStream, FileLocation location, const CIPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) = 0;
		virtual Result TryOpenFileWriteAbs(UniquePtr<ISeekableWriteStream> &outStream, const OSAbsPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) = 0;
		virtual Result TryOpenFileReadWrite(UniquePtr<ISeekableReadWriteStream> &outStream, FileLocation location, const CIPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) = 0;
		virtual Result TryOpenFileReadWriteAbs(UniquePtr<ISeekableReadWriteStream> &outStream, const OSAbsPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) = 0;

		virtual Result CreateThread(UniqueThreadRef &outThread, UniquePtr<IThreadContext> &&threadContext, const StringView &threadName) = 0;
		virtual Result CreateMutex(UniquePtr<IMutex> &outMutex) = 0;
		virtual Result CreateEvent(UniquePtr<IEvent> &outEvent, bool autoReset, bool startSignaled) = 0;
		virtual void SleepMSec(uint32_t msec) const = 0;

		virtual Result OpenDirectoryScan(FileLocation location, const CIPathView &path, UniquePtr<IDirectoryScan> &outDirectoryScan) = 0;
		virtual Result OpenDirectoryScanAbs(const OSAbsPathView &path, UniquePtr<IDirectoryScan> &outDirectoryScan) = 0;
		virtual Result GetFileAttributes(FileLocation location, const CIPathView &path, bool &outExists, FileAttributes &outAttribs) = 0;
		virtual Result GetFileAttributesAbs(const OSAbsPathView &path, bool &outExists, FileAttributes &outAttribs) = 0;

		virtual Result CopyFileFromAbsToAbs(const OSAbsPathView &srcPath, const OSAbsPathView &destPath, bool overwrite) = 0;
		virtual Result CopyFileToAbs(FileLocation location, const CIPathView &path, const OSAbsPathView &destPath, bool overwrite) = 0;
		virtual Result CopyFileFromAbs(const OSAbsPathView &srcPath, FileLocation location, const CIPathView &path, bool overwrite) = 0;
		virtual Result CopyFile(FileLocation srcLocation, const CIPathView &srcPath, FileLocation destLocation, const CIPathView &destPath, bool overwrite) = 0;

		virtual Result MoveFileFromAbsToAbs(const OSAbsPathView &srcPath, const OSAbsPathView &destPath, bool overwrite) = 0;
		virtual Result MoveFileToAbs(FileLocation location, const CIPathView &path, const OSAbsPathView &destPath, bool overwrite) = 0;
		virtual Result MoveFileFromAbs(const OSAbsPathView &srcPath, FileLocation location, const CIPathView &path, bool overwrite) = 0;
		virtual Result MoveFile(FileLocation srcLocation, const CIPathView &srcPath, FileLocation destLocation, const CIPathView &destPath, bool overwrite) = 0;

		virtual Result SetGameDirectoryOverride(const OSAbsPathView &path) = 0;
		virtual Result SetSettingsDirectory(const StringView &path) = 0;
		virtual char GetPathSeparator() const = 0;

		virtual IPlatformDriver *GetPlatformDriver() const = 0;

		virtual Result OpenSystemLibrary(UniquePtr<ISystemLibrary> &outLibrary, SystemLibraryType libType) const = 0;

		virtual uint32_t GetProcessorCount() const = 0;

		virtual render::IDisplayManager *GetDisplayManager() const = 0;

		Result AsyncTryOpenFileRead(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<UniquePtr<ISeekableReadStream>> &outStream, FileLocation location, const CIPathView &path);
		Result AsyncTryOpenFileReadAbs(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<UniquePtr<ISeekableReadStream>> &outStream, const OSAbsPathView &path);

		Result AsyncOpenFileAsyncRead(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<AsyncFileOpenReadResult> &outStream, FileLocation location, const CIPathView &path);
		Result AsyncOpenFileAsyncReadAbs(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<AsyncFileOpenReadResult> &outStream, const OSAbsPathView &path);

		Result OpenFileRead(UniquePtr<ISeekableReadStream> &outStream, FileLocation location, const CIPathView &path);
		Result OpenFileReadAbs(UniquePtr<ISeekableReadStream> &outStream, const OSAbsPathView &path);
		Result OpenFileAsyncRead(AsyncFileOpenReadResult &outResult, FileLocation location, const CIPathView &path);
		Result OpenFileAsyncReadAbs(AsyncFileOpenReadResult &outResult, const OSAbsPathView &path);
		Result OpenFileWrite(UniquePtr<ISeekableWriteStream> &outStream, FileLocation location, const CIPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists);
		Result OpenFileWriteAbs(UniquePtr<ISeekableWriteStream> &outStream, const OSAbsPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists);
		Result OpenFileReadWrite(UniquePtr<ISeekableReadWriteStream> &outStream, FileLocation location, const CIPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists);
		Result OpenFileReadWriteAbs(UniquePtr<ISeekableReadWriteStream> &outStream, const OSAbsPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists);

	protected:
		virtual Result InternalAsyncTryOpenFileRead(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<UniquePtr<ISeekableReadStream>> &outStream, FileLocation location, const CIPathView &path, bool allowFailure) = 0;
		virtual Result InternalAsyncTryOpenFileReadAbs(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<UniquePtr<ISeekableReadStream>> &outStream, const OSAbsPathView &path, bool allowFailure) = 0;

		virtual Result InternalAsyncOpenFileAsyncRead(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<AsyncFileOpenReadResult> &outStream, FileLocation location, const CIPathView &path, bool allowFailure) = 0;
		virtual Result InternalAsyncOpenFileAsyncReadAbs(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const FutureContainerPtr<AsyncFileOpenReadResult> &outStream, const OSAbsPathView &path, bool allowFailure) = 0;
	};
}
