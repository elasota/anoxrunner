#include "rkit/Core/Algorithm.h"
#include "rkit/Core/AsyncFile.h"
#include "rkit/Core/DirectoryScan.h"
#include "rkit/Core/Drivers.h"
#include "rkit/Core/Event.h"
#include "rkit/Core/Future.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/MallocDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/Span.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Thread.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"

#include "rkit/Win32/Win32PlatformDriver.h"
#include "rkit/Win32/SystemModuleInitParameters_Win32.h"

#include "Win32AsyncIOThread.h"
#include "Win32DisplayManager.h"
#include "ConvUtil.h"

#include <shellapi.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <timezoneapi.h>
#include <KnownFolders.h>


namespace rkit
{
	class Thread_Win32;

	typedef HRESULT(WINAPI *SetThreadDescriptionProc_Win32_t)(HANDLE hThread, PCWSTR lpThreadDescription);

	class File_Win32 final : public ISeekableReadWriteStream
	{
	public:
		File_Win32(HANDLE hfile, FilePos_t initialSize);
		~File_Win32();

		Result WritePartial(const void *data, size_t count, size_t &outCountWritten) override;
		Result Flush() override;

		Result ReadPartial(void *data, size_t count, size_t &outCountRead) override;

		Result SeekStart(FilePos_t pos) override;
		Result SeekCurrent(FileOffset_t pos) override;
		Result SeekEnd(FileOffset_t pos) override;

		FilePos_t Tell() const override;
		FilePos_t GetSize() const override;

		Result Truncate(FilePos_t pos) override;

		HANDLE GetHandle() const;

	private:
		HANDLE m_hfile;
		FilePos_t m_filePos;
		FilePos_t m_fileSize;
	};

	class AsyncFileInstance_Win32 final : public RefCounted
	{
	public:
		explicit AsyncFileInstance_Win32(UniquePtr<File_Win32> &&file);
		~AsyncFileInstance_Win32();

		HANDLE GetHandle() const;

	private:
		HANDLE m_hfile;
		UniquePtr<File_Win32> m_file;
	};

	class AsyncReadWriteRequesterInstance_Win32 final : public RefCounted
	{
	public:
		explicit AsyncReadWriteRequesterInstance_Win32(AsyncIOThread_Win32 &asioThread, const RCPtr<AsyncFileInstance_Win32> &instance);

		void PostReadRequest(IJobQueue &jobQueue, void *readBuffer, FilePos_t pos, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback);
		void PostWriteRequest(IJobQueue &jobQueue, const void *writeBuffer, FilePos_t pos, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback);
		void PostAppendRequest(IJobQueue &jobQueue, const void *writeBuffer, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback);

	private:
		void OpenRequest();
		void CloseRequest();

		void PostToIOQueue();

		Result CheckedPostReadRequest(IJobQueue &jobQueue, void *readBuffer, FilePos_t pos, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback);
		Result CheckedPostWriteRequest(IJobQueue &jobQueue, const void *writeBuffer, FilePos_t pos, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback);
		Result CheckedPostAppendRequest(IJobQueue &jobQueue, const void *writeBuffer, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback);

		void ContinueRequest();
		Result CheckedContinueRequest();

		static VOID WINAPI StaticCompleteRequest(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
		static void StaticExecute(void *userdata);
		static void StaticCancel(void *userdata);
		static void StaticFlush(void *userdata);

		void Execute();
		void Cancel();
		void Flush();
		void CompleteRequest(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered);

		enum class RequestType
		{
			kRead,
			kWrite,
			kAppend,
		};

		struct OverlappedHolder
		{
			OVERLAPPED m_overlapped;

			void *m_userdata;
			AsyncIOCompletionCallback_t m_callback;

			RequestType m_requestType;
			void *m_dataBuffer;
			FilePos_t m_startPosition;
			size_t m_remainingBytes;
			size_t m_bytesProcessed;

			DWORD m_amountRequestedByLastRequest;
		};

		AsyncIOTaskItem_Win32 m_taskItem;

		AsyncIOThread_Win32 &m_asioThread;
		HANDLE m_hfile;
		const RCPtr<AsyncFileInstance_Win32> m_instance;
		OverlappedHolder m_overlappedHolder;
		RCPtr<AsyncReadWriteRequesterInstance_Win32> m_keepalive;

#if RKIT_IS_DEBUG
		std::atomic<uint32_t> m_outstandingRequests;
#endif
	};

	class AsyncReadWriteRequester_Win32 final : public IAsyncReadRequester, public IAsyncWriteRequester
	{
	public:
		explicit AsyncReadWriteRequester_Win32(const RCPtr<AsyncReadWriteRequesterInstance_Win32> &instance);

		void PostReadRequest(IJobQueue &jobQueue, void *readBuffer, FilePos_t pos, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback) override;
		void PostWriteRequest(IJobQueue &jobQueue, const void *writeBuffer, FilePos_t pos, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback) override;
		void PostAppendRequest(IJobQueue &jobQueue, const void *writeBuffer, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback) override;

	private:
		RCPtr<AsyncReadWriteRequesterInstance_Win32> m_instance;
	};

	class AsyncFile_Win32 final : public IAsyncReadWriteFile
	{
	public:
		explicit AsyncFile_Win32(AsyncIOThread_Win32 &asioThread, RCPtr<AsyncFileInstance_Win32> &&instance);
		~AsyncFile_Win32();

		Result CreateReadRequester(UniquePtr<IAsyncReadRequester> &requester) override;
		Result CreateWriteRequester(UniquePtr<IAsyncWriteRequester> &requester) override;

	private:
		AsyncIOThread_Win32 &m_asioThread;
		RCPtr<AsyncFileInstance_Win32> m_instance;
	};

	class DirectoryScan_Win32 final : public IDirectoryScan
	{
	public:
		DirectoryScan_Win32();
		~DirectoryScan_Win32();

		Result GetNext(bool &haveItem, DirectoryScanItem &outItem) override;

		WIN32_FIND_DATAW *GetFindData();
		void SetHandle(HANDLE hdl);

	private:
		bool CheckItem() const;
		Result ProduceItem(DirectoryScanItem &outItem);
		Result GetAnotherItem();

		HANDLE m_handle;
		WIN32_FIND_DATAW m_findData;
		Vector<char> m_fileNameUTF8;
		bool m_exhausted;
		bool m_haveItem;
	};

	class SystemLibrary_Win32 final : public ISystemLibrary, public NoCopy
	{
	public:
		SystemLibrary_Win32();
		~SystemLibrary_Win32();

		void SetHModule(const HMODULE &hmodule);

		bool GetFunction(void *fnPtr, const StringView &fnName) override;

	private:
		HMODULE m_hmodule;
	};

	class Mutex_Win32 final : public IMutex
	{
	public:
		Mutex_Win32();
		~Mutex_Win32();

		void Lock() override;
		bool TryLock(uint32_t maxMSec) override;
		void Unlock() override;

	private:
		CRITICAL_SECTION m_critSection;
	};

	class Event_Win32 final : public IEvent
	{
	public:
		Event_Win32();
		~Event_Win32();

		void Signal() override;
		void Reset() override;
		void Wait() override;
		bool TimedWait(uint32_t msec) override;

		Result Initialize(bool autoReset, bool startSignaled);

	private:
		HANDLE m_event = nullptr;
	};

	struct ThreadKickoffInfo_Win32
	{
		HANDLE m_hKickoffEvent;
		Result *m_outResult;
		UniquePtr<IThreadContext> m_threadContext;
#if RKIT_IS_DEBUG
		const wchar_t *m_threadName;
		SetThreadDescriptionProc_Win32_t m_setThreadDescriptionProc;
#endif
	};

	class Thread_Win32 final : public IThread
	{
	public:
		Thread_Win32();
		~Thread_Win32();

		void SetHandle(HANDLE hThread);
		Result *GetResultPtr();

		void Finalize(Result &outResult) override;

	private:
		HANDLE m_hThread;
		Result m_result;
	};

	class SystemDriver_Win32 final : public NoCopy, public ISystemDriver, public IWin32PlatformDriver
	{
	public:
		SystemDriver_Win32(IMallocDriver *alloc, const SystemModuleInitParameters_Win32 &initParams);
		~SystemDriver_Win32();

		Result Initialize();

		void RemoveCommandLineArgs(size_t firstArg, size_t numArgs) override;
		Span<const StringView> GetCommandLine() const override;
		void AssertionFailure(const char *expr, const char *file, unsigned int line) override;
		void FirstChanceResultFailure(const Result &result) override;

		Result AsyncOpenFileRead(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const Future<UniquePtr<ISeekableReadStream>> &outStream, FileLocation location, const CIPathView &path) override;
		Result AsyncOpenFileReadAbs(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const Future<UniquePtr<ISeekableReadStream>> &outStream, const OSAbsPathView &path) override;

		Result AsyncOpenFileAsyncRead(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const Future<AsyncFileOpenReadResult> &outStream, FileLocation location, const CIPathView &path) override;
		Result AsyncOpenFileAsyncReadAbs(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const Future<AsyncFileOpenReadResult> &outStream, const OSAbsPathView &path) override;

		Result OpenFileRead(UniquePtr<ISeekableReadStream> &outStream, FileLocation location, const CIPathView &path) override;
		Result OpenFileReadAbs(UniquePtr<ISeekableReadStream> &outStream, const OSAbsPathView &path) override;
		Result OpenFileAsyncRead(AsyncFileOpenReadResult &outResult, FileLocation location, const CIPathView &path) override;
		Result OpenFileAsyncReadAbs(AsyncFileOpenReadResult &outResult, const OSAbsPathView &path) override;
		Result OpenFileWrite(UniquePtr<ISeekableWriteStream> &outStream, FileLocation location, const CIPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) override;
		Result OpenFileWriteAbs(UniquePtr<ISeekableWriteStream> &outStream, const OSAbsPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) override;
		Result OpenFileReadWrite(UniquePtr<ISeekableReadWriteStream> &outStream, FileLocation location, const CIPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) override;
		Result OpenFileReadWriteAbs(UniquePtr<ISeekableReadWriteStream> &outStream, const OSAbsPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) override;

		Result CreateThread(UniqueThreadRef &outThread, UniquePtr<IThreadContext> &&threadContext, const StringView &threadName) override;
		Result CreateMutex(UniquePtr<IMutex> &mutex) override;
		Result CreateEvent(UniquePtr<IEvent> &outEvent, bool autoReset, bool startSignaled) override;
		void SleepMSec(uint32_t msec) const override;

		Result OpenDirectoryScan(FileLocation location, const CIPathView &path, UniquePtr<IDirectoryScan> &outDirectoryScan) override;
		Result OpenDirectoryScanAbs(const OSAbsPathView &path, UniquePtr<IDirectoryScan> &outDirectoryScan) override;
		Result GetFileAttributes(FileLocation location, const CIPathView &path, bool &outExists, FileAttributes &outAttribs) override;
		Result GetFileAttributesAbs(const OSAbsPathView &path, bool &outExists, FileAttributes &outAttribs) override;

		Result CopyFileFromAbsToAbs(const OSAbsPathView &srcPath, const OSAbsPathView &destPath, bool overwrite) override;
		Result CopyFileToAbs(FileLocation location, const CIPathView &path, const OSAbsPathView &destPath, bool overwrite) override;
		Result CopyFileFromAbs(const OSAbsPathView &srcPath, FileLocation location, const CIPathView &path, bool overwrite) override;
		Result CopyFile(FileLocation srcLocation, const CIPathView &srcPath, FileLocation destLocation, const CIPathView &destPath, bool overwrite) override;

		Result MoveFileFromAbsToAbs(const OSAbsPathView &srcPath, const OSAbsPathView &destPath, bool overwrite) override;
		Result MoveFileToAbs(FileLocation location, const CIPathView &path, const OSAbsPathView &destPath, bool overwrite) override;
		Result MoveFileFromAbs(const OSAbsPathView &srcPath, FileLocation location, const CIPathView &path, bool overwrite) override;
		Result MoveFile(FileLocation srcLocation, const CIPathView &srcPath, FileLocation destLocation, const CIPathView &destPath, bool overwrite) override;

		Result SetGameDirectoryOverride(const OSAbsPathView &path) override;
		Result SetSettingsDirectory(const StringView &path) override;
		char GetPathSeparator() const override;

		IPlatformDriver *GetPlatformDriver() const override;

		HINSTANCE GetHInstance() const override;

		Result OpenSystemLibrary(UniquePtr<ISystemLibrary> &outLibrary, SystemLibraryType libType) const override;

		uint32_t GetProcessorCount() const override;

		render::IDisplayManager *GetDisplayManager() const override;

	private:
		static DWORD OpenFlagsToDisposition(bool createIfNotExists, bool truncateIfExists);
		Result OpenFileGeneral(UniquePtr<File_Win32> &outStream, const OSAbsPathView &path, bool createDirectories, DWORD access, DWORD shareMode, DWORD disposition, DWORD extraFlags);
		Result OpenFileAsyncGeneral(UniquePtr<AsyncFile_Win32> &outStream, FilePos_t &outInitialSize, const OSAbsPathView &path, bool createDirectories, DWORD access, DWORD shareMode, DWORD disposition, DWORD extraFlags);
		static Result CheckCreateDirectories(Vector<wchar_t> &pathChars);

		Result ResolveAbsPath(OSAbsPath &outPath, FileLocation location, const CIPathView &path);

		static DWORD WINAPI ThreadStartRoutine(LPVOID lpThreadParameter);

		IMallocDriver *m_alloc;
		Vector<Vector<char> > m_commandLineCharBuffers;
		Vector<StringView> m_commandLine;
		UniquePtr<render::DisplayManagerBase_Win32> m_displayManager;
		UniquePtr<AsyncIOThread_Win32> m_asioThread;
		LPWSTR *m_argvW;

		WString m_exePathStr;
		WString m_programDirStr;

		OSAbsPath m_gameDirectoryOverride;
		OSAbsPath m_settingsDirectory;

		HINSTANCE m_hInstance;

		HMODULE m_kernelBaseModule = nullptr;

#if RKIT_IS_DEBUG
		SetThreadDescriptionProc_Win32_t m_setThreadDescriptionProc;
#endif
	};

	class OpenFileReadJobRunner final : public IJobRunner
	{
	public:
		OpenFileReadJobRunner(const Future<UniquePtr<ISeekableReadStream>> &streamFuture, const OSAbsPath &filePath, ISystemDriver &systemDriver);
		~OpenFileReadJobRunner();

		Result Run() override;

	private:
		Result CheckedRun();

		Future<UniquePtr<ISeekableReadStream>> m_streamFuture;
		OSAbsPath m_filePath;
		ISystemDriver &m_systemDriver;
		bool m_completed = false;
	};

	class OpenFileAsyncReadJobRunner final : public IJobRunner
	{
	public:
		OpenFileAsyncReadJobRunner(const Future<AsyncFileOpenReadResult> &streamFuture, const OSAbsPath &filePath, ISystemDriver &systemDriver);
		~OpenFileAsyncReadJobRunner();

		Result Run() override;

	private:
		Result CheckedRun();

		Future<AsyncFileOpenReadResult> m_streamFuture;
		OSAbsPath m_filePath;
		ISystemDriver &m_systemDriver;
		bool m_completed = false;
	};

	class SystemModule_Win32
	{
	public:
		static Result Init(const ModuleInitParameters *);
		static void Shutdown();

	private:
		static SimpleObjectAllocation<SystemDriver_Win32> ms_systemDriver;
	};

	Result ConvUtil_Win32::UTF8ToUTF16(const char *str8, Vector<wchar_t> &outStr16)
	{
		int charsRequired = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str8, -1, nullptr, 0);
		if (charsRequired == 0)
			return ResultCode::kInvalidUnicode;

		RKIT_CHECK(outStr16.Resize(charsRequired));

		int convResult = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str8, -1, outStr16.GetBuffer(), static_cast<int>(outStr16.Count()));
		if (convResult == 0)
			return ResultCode::kInvalidUnicode;

		return ResultCode::kOK;
	}

	Result ConvUtil_Win32::UTF16ToUTF8(const wchar_t *str16, Vector<char> &outStr8)
	{
		int bytesRequired = WideCharToMultiByte(CP_UTF8, 0, str16, -1, nullptr, 0, nullptr, nullptr);
		if (bytesRequired == 0)
			return ResultCode::kInvalidUnicode;

		RKIT_CHECK(outStr8.Resize(bytesRequired));

		int convResult = WideCharToMultiByte(CP_UTF8, 0, str16, -1, outStr8.GetBuffer(), static_cast<int>(outStr8.Count()), nullptr, nullptr);
		if (convResult == 0)
			return ResultCode::kInvalidUnicode;

		return ResultCode::kOK;
	}


	UTCMSecTimestamp_t ConvUtil_Win32::FileTimeToUTCMSec(const FILETIME &ftime)
	{
		ULARGE_INTEGER ftu64;
		ftu64.HighPart = ftime.dwHighDateTime;
		ftu64.LowPart = ftime.dwLowDateTime;

		uint64_t adjustedFT = ftu64.QuadPart - kUnixEpochStartFT;

		return adjustedFT / 10000u;
	}

	FILETIME ConvUtil_Win32::UTCMSecToFileTime(UTCMSecTimestamp_t timestamp)
	{
		uint64_t adjustedFT = timestamp * 10000u;

		ULARGE_INTEGER ftu64;
		ftu64.QuadPart = adjustedFT + kUnixEpochStartFT;

		FILETIME result;
		result.dwHighDateTime = ftu64.HighPart;
		result.dwLowDateTime = ftu64.LowPart;

		return result;
	}

	File_Win32::File_Win32(HANDLE hfile, FilePos_t initialSize)
		: m_hfile(hfile), m_filePos(0), m_fileSize(initialSize)
	{
	}

	File_Win32::~File_Win32()
	{
		CloseHandle(m_hfile);
	}

	Result File_Win32::WritePartial(const void *data, size_t count, size_t &outCountWritten)
	{
		size_t countWritten = 0;
		while (count > 0)
		{
			DWORD amount = 0;
			if (count > MAXDWORD)
				amount = MAXDWORD;
			else
				amount = static_cast<DWORD>(count);

			DWORD actualAmount = 0;
			BOOL succeeded = WriteFile(m_hfile, data, amount, &actualAmount, nullptr);

			count -= actualAmount;
			countWritten += actualAmount;

			m_filePos += actualAmount;
			if (m_fileSize < m_filePos)
				m_fileSize = m_filePos;

			data = static_cast<const void *>(static_cast<const char *>(data) + actualAmount);

			if (!succeeded || actualAmount == 0)
			{
				outCountWritten = countWritten;
				return ResultCode::kIOError;
			}
		}

		outCountWritten = countWritten;
		return ResultCode::kOK;
	}

	Result File_Win32::Flush()
	{
		if (!FlushFileBuffers(m_hfile))
			return ResultCode::kIOError;

		return ResultCode::kOK;
	}

	Result File_Win32::ReadPartial(void *data, size_t count, size_t &outCountRead)
	{
		FilePos_t maxSize = m_fileSize - m_filePos;
		if (count > maxSize)
			count = static_cast<size_t>(maxSize);

		size_t countRead = 0;
		while (count > 0)
		{
			DWORD amount = 0;
			if (count > MAXDWORD)
				amount = MAXDWORD;
			else
				amount = static_cast<DWORD>(count);

			DWORD actualAmount = 0;
			BOOL succeeded = ReadFile(m_hfile, data, amount, &actualAmount, nullptr);

			count -= actualAmount;
			countRead += actualAmount;

			m_filePos += actualAmount;

			data = static_cast<void *>(static_cast<char *>(data) + actualAmount);

			if (!succeeded || actualAmount == 0)
			{
				outCountRead = countRead;
				return ResultCode::kIOError;
			}
		}

		outCountRead = countRead;
		return ResultCode::kOK;
	}

	Result File_Win32::SeekStart(FilePos_t pos)
	{
		if (pos > m_fileSize)
			return ResultCode::kIOSeekOutOfRange;

		if (pos == m_filePos)
			return ResultCode::kOK;

		LARGE_INTEGER newPos;
		newPos.QuadPart = static_cast<LONGLONG>(pos);

		if (!SetFilePointerEx(m_hfile, newPos, nullptr, FILE_BEGIN))
			return ResultCode::kIOError;

		m_filePos = pos;
		return ResultCode::kOK;
	}

	Result File_Win32::SeekCurrent(FileOffset_t offset)
	{
		if (offset < -static_cast<FileOffset_t>(m_filePos))
			return ResultCode::kIOSeekOutOfRange;
		if (offset > static_cast<FileOffset_t>(m_fileSize - m_filePos))
			return ResultCode::kIOSeekOutOfRange;

		return SeekStart(static_cast<FilePos_t>(static_cast<FileOffset_t>(m_filePos) + offset));
	}

	Result File_Win32::SeekEnd(FileOffset_t offset)
	{
		if (offset < -static_cast<FileOffset_t>(m_fileSize))
			return ResultCode::kIOSeekOutOfRange;
		if (offset > 0)
			return ResultCode::kIOSeekOutOfRange;

		return SeekStart(static_cast<FilePos_t>(static_cast<FileOffset_t>(m_fileSize) + offset));
	}

	FilePos_t File_Win32::Tell() const
	{
		return m_filePos;
	}

	FilePos_t File_Win32::GetSize() const
	{
		return m_fileSize;
	}

	Result File_Win32::Truncate(FilePos_t newSize)
	{
		if (m_fileSize == newSize)
			return ResultCode::kOK;

		if (m_fileSize < newSize)
			return ResultCode::kOperationFailed;

		FilePos_t oldFilePos = m_filePos;

		if (newSize != m_filePos)
		{
			LARGE_INTEGER newPos;
			newPos.QuadPart = static_cast<LONGLONG>(newSize);

			if (!::SetFilePointerEx(m_hfile, newPos, nullptr, FILE_BEGIN))
				return ResultCode::kOperationFailed;
		}

		BOOL succeeded = ::SetEndOfFile(m_hfile);

		if (succeeded)
			m_fileSize = newSize;

		if (oldFilePos < newSize)
		{
			LARGE_INTEGER newPos;
			newPos.QuadPart = static_cast<LONGLONG>(oldFilePos);

			::SetFilePointerEx(m_hfile, newPos, nullptr, FILE_BEGIN);
		}

		if (succeeded)
			return ResultCode::kOK;
		else
			return ResultCode::kOperationFailed;
	}

	HANDLE File_Win32::GetHandle() const
	{
		return m_hfile;
	}

	AsyncFileInstance_Win32::AsyncFileInstance_Win32(UniquePtr<File_Win32> &&file)
		: m_hfile(file->GetHandle())
		, m_file(std::move(file))
	{
	}

	AsyncFileInstance_Win32::~AsyncFileInstance_Win32()
	{
	}

	HANDLE AsyncFileInstance_Win32::GetHandle() const
	{
		return m_hfile;
	}

	AsyncReadWriteRequesterInstance_Win32::AsyncReadWriteRequesterInstance_Win32(AsyncIOThread_Win32 &asioThread, const RCPtr<AsyncFileInstance_Win32> &instance)
		: m_hfile(instance->GetHandle())
		, m_instance(instance)
		, m_overlappedHolder {}
		, m_asioThread(asioThread)
#if RKIT_IS_DEBUG
		, m_outstandingRequests(0)
#endif
	{
		m_taskItem.m_cancelFunc = StaticCancel;
		m_taskItem.m_executeFunc = StaticExecute;
		m_taskItem.m_flushFunc = StaticFlush;
		m_taskItem.m_userdata = this;
	}

	void AsyncReadWriteRequesterInstance_Win32::PostReadRequest(IJobQueue &jobQueue, void *readBuffer, FilePos_t pos, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback)
	{
		OpenRequest();
		Result postResult = CheckedPostReadRequest(jobQueue, readBuffer, pos, amount, completionUserData, completeCallback);
		if (!utils::ResultIsOK(postResult))
		{
			CloseRequest();
			completeCallback(completionUserData, postResult, 0);
		}
	}

	void AsyncReadWriteRequesterInstance_Win32::PostWriteRequest(IJobQueue &jobQueue, const void *writeBuffer, FilePos_t pos, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback)
	{
		OpenRequest();
		Result postResult = CheckedPostWriteRequest(jobQueue, writeBuffer, pos, amount, completionUserData, completeCallback);
		if (!utils::ResultIsOK(postResult))
		{
			CloseRequest();
			completeCallback(completionUserData, postResult, 0);
		}
	}

	void AsyncReadWriteRequesterInstance_Win32::PostAppendRequest(IJobQueue &jobQueue, const void *writeBuffer, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback)
	{
		OpenRequest();
		Result postResult = CheckedPostAppendRequest(jobQueue, writeBuffer, amount, completionUserData, completeCallback);
		if (!utils::ResultIsOK(postResult))
		{
			CloseRequest();
			completeCallback(completionUserData, postResult, 0);
		}
	}

	void AsyncReadWriteRequesterInstance_Win32::OpenRequest()
	{
#if RKIT_IS_DEBUG
		uint32_t numOutstanding = m_outstandingRequests.fetch_add(1);
		RKIT_ASSERT(numOutstanding == 0);
#endif

		m_keepalive = RCPtr<AsyncReadWriteRequesterInstance_Win32>(this);
	}

	void AsyncReadWriteRequesterInstance_Win32::CloseRequest()
	{
#if RKIT_IS_DEBUG
		m_outstandingRequests.fetch_sub(1);
#endif

		m_keepalive.Reset();
	}

	void AsyncReadWriteRequesterInstance_Win32::PostToIOQueue()
	{
		m_asioThread.PostTask(m_taskItem);
	}

	Result AsyncReadWriteRequesterInstance_Win32::CheckedPostReadRequest(IJobQueue &jobQueue, void *readBuffer, FilePos_t pos, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback)
	{
		if (std::numeric_limits<FilePos_t>::max() - pos < amount)
			return ResultCode::kIntegerOverflow;

		m_overlappedHolder.m_overlapped = {};
		m_overlappedHolder.m_userdata = completionUserData;
		m_overlappedHolder.m_callback = completeCallback;
		m_overlappedHolder.m_requestType = RequestType::kRead;
		m_overlappedHolder.m_dataBuffer = readBuffer;
		m_overlappedHolder.m_startPosition = pos;
		m_overlappedHolder.m_remainingBytes = amount;
		m_overlappedHolder.m_bytesProcessed = 0;

		PostToIOQueue();

		return ResultCode::kOK;
	}

	Result AsyncReadWriteRequesterInstance_Win32::CheckedPostWriteRequest(IJobQueue &jobQueue, const void *writeBuffer, FilePos_t pos, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback)
	{
		if (std::numeric_limits<FilePos_t>::max() - pos < amount)
			return ResultCode::kIntegerOverflow;

		m_overlappedHolder.m_overlapped = {};
		m_overlappedHolder.m_userdata = completionUserData;
		m_overlappedHolder.m_callback = completeCallback;
		m_overlappedHolder.m_requestType = RequestType::kWrite;
		m_overlappedHolder.m_dataBuffer = const_cast<void *>(writeBuffer);
		m_overlappedHolder.m_startPosition = pos;
		m_overlappedHolder.m_remainingBytes = amount;
		m_overlappedHolder.m_bytesProcessed = 0;

		PostToIOQueue();

		return ResultCode::kOK;
	}

	Result AsyncReadWriteRequesterInstance_Win32::CheckedPostAppendRequest(IJobQueue &jobQueue, const void *writeBuffer, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback)
	{
		m_overlappedHolder.m_overlapped = {};
		m_overlappedHolder.m_userdata = completionUserData;
		m_overlappedHolder.m_callback = completeCallback;
		m_overlappedHolder.m_requestType = RequestType::kWrite;
		m_overlappedHolder.m_dataBuffer = const_cast<void *>(writeBuffer);
		m_overlappedHolder.m_startPosition = 0;
		m_overlappedHolder.m_remainingBytes = amount;
		m_overlappedHolder.m_bytesProcessed = 0;

		PostToIOQueue();

		return ResultCode::kOK;
	}

	void AsyncReadWriteRequesterInstance_Win32::ContinueRequest()
	{
		Result continueResult = CheckedContinueRequest();
		if (!utils::ResultIsOK(continueResult))
			m_overlappedHolder.m_callback(m_overlappedHolder.m_userdata, continueResult, m_overlappedHolder.m_bytesProcessed);
	}

	Result AsyncReadWriteRequesterInstance_Win32::CheckedContinueRequest()
	{
		DWORD operationAmount = MAXDWORD;
		if (operationAmount > m_overlappedHolder.m_remainingBytes)
			operationAmount = static_cast<DWORD>(m_overlappedHolder.m_remainingBytes);

		m_overlappedHolder.m_amountRequestedByLastRequest = operationAmount;

		switch (m_overlappedHolder.m_requestType)
		{
		case RequestType::kRead:
			m_overlappedHolder.m_overlapped.OffsetHigh = static_cast<DWORD>(m_overlappedHolder.m_startPosition >> 32);
			m_overlappedHolder.m_overlapped.OffsetHigh = static_cast<DWORD>(m_overlappedHolder.m_startPosition & 0xffffffffu);
			if (!ReadFileEx(m_hfile, m_overlappedHolder.m_dataBuffer, operationAmount, &m_overlappedHolder.m_overlapped, StaticCompleteRequest))
				return utils::CreateResultWithExtCode(ResultCode::kIOWriteError, GetLastError());
			break;
		case RequestType::kWrite:
			m_overlappedHolder.m_overlapped.OffsetHigh = static_cast<DWORD>(m_overlappedHolder.m_startPosition >> 32);
			m_overlappedHolder.m_overlapped.OffsetHigh = static_cast<DWORD>(m_overlappedHolder.m_startPosition & 0xffffffffu);
			if (!WriteFileEx(m_hfile, m_overlappedHolder.m_dataBuffer, operationAmount, &m_overlappedHolder.m_overlapped, StaticCompleteRequest))
				return utils::CreateResultWithExtCode(ResultCode::kIOWriteError, GetLastError());
			break;
		case RequestType::kAppend:
			m_overlappedHolder.m_overlapped.OffsetHigh = static_cast<DWORD>(-1);
			m_overlappedHolder.m_overlapped.OffsetHigh = static_cast<DWORD>(-1);
			if (!WriteFileEx(m_hfile, m_overlappedHolder.m_dataBuffer, operationAmount, &m_overlappedHolder.m_overlapped, StaticCompleteRequest))
				return utils::CreateResultWithExtCode(ResultCode::kIOWriteError, GetLastError());
			break;

		default:
			return ResultCode::kInternalError;
		};

		m_asioThread.PostInProgress(m_taskItem);

		return ResultCode::kOK;
	}

	VOID WINAPI AsyncReadWriteRequesterInstance_Win32::StaticCompleteRequest(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
	{
		uint8_t *ptr = reinterpret_cast<uint8_t *>(lpOverlapped);
		ptr -= offsetof(AsyncReadWriteRequesterInstance_Win32::OverlappedHolder, m_overlapped);
		ptr -= offsetof(AsyncReadWriteRequesterInstance_Win32, m_overlappedHolder);

		reinterpret_cast<AsyncReadWriteRequesterInstance_Win32 *>(ptr)->CompleteRequest(dwErrorCode, dwNumberOfBytesTransfered);
	}

	void AsyncReadWriteRequesterInstance_Win32::StaticExecute(void *userdata)
	{
		static_cast<AsyncReadWriteRequesterInstance_Win32 *>(userdata)->Execute();
	}

	void AsyncReadWriteRequesterInstance_Win32::StaticCancel(void *userdata)
	{
		static_cast<AsyncReadWriteRequesterInstance_Win32 *>(userdata)->Cancel();
	}

	void AsyncReadWriteRequesterInstance_Win32::StaticFlush(void *userdata)
	{
		static_cast<AsyncReadWriteRequesterInstance_Win32 *>(userdata)->Flush();
	}

	void AsyncReadWriteRequesterInstance_Win32::Execute()
	{
		ContinueRequest();
	}

	void AsyncReadWriteRequesterInstance_Win32::Cancel()
	{
		BOOL cancelled = ::CancelIoEx(m_hfile, &m_overlappedHolder.m_overlapped);
		if (cancelled || GetLastError() != ERROR_NOT_FOUND)
		{
			// Wait for the overlapped request
			DWORD bytesTransferred = 0;
			GetOverlappedResult(m_hfile, &m_overlappedHolder.m_overlapped, &bytesTransferred, TRUE);
		}
	}

	void AsyncReadWriteRequesterInstance_Win32::Flush()
	{
		DWORD bytesTransferred = 0;
		GetOverlappedResult(m_hfile, &m_overlappedHolder.m_overlapped, &bytesTransferred, TRUE);
	}

	void AsyncReadWriteRequesterInstance_Win32::CompleteRequest(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered)
	{
		m_asioThread.RemoveInProgress(m_taskItem);

		m_overlappedHolder.m_bytesProcessed += dwNumberOfBytesTransfered;
		m_overlappedHolder.m_dataBuffer = static_cast<uint8_t *>(m_overlappedHolder.m_dataBuffer) + dwNumberOfBytesTransfered;
		m_overlappedHolder.m_startPosition += dwNumberOfBytesTransfered;
		m_overlappedHolder.m_remainingBytes -= dwNumberOfBytesTransfered;

		if (dwNumberOfBytesTransfered != m_overlappedHolder.m_amountRequestedByLastRequest || m_overlappedHolder.m_remainingBytes == 0)
			m_overlappedHolder.m_callback(m_overlappedHolder.m_userdata, ResultCode::kOK, m_overlappedHolder.m_bytesProcessed);
		else
			ContinueRequest();
	}

	AsyncReadWriteRequester_Win32::AsyncReadWriteRequester_Win32(const RCPtr<AsyncReadWriteRequesterInstance_Win32> &instance)
		: m_instance(instance)
	{
	}

	void AsyncReadWriteRequester_Win32::PostReadRequest(IJobQueue &jobQueue, void *readBuffer, FilePos_t pos, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback)
	{
		return m_instance->PostReadRequest(jobQueue, readBuffer, pos, amount, completionUserData, completeCallback);
	}

	void AsyncReadWriteRequester_Win32::PostWriteRequest(IJobQueue &jobQueue, const void *writeBuffer, FilePos_t pos, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback)
	{
		return m_instance->PostWriteRequest(jobQueue, writeBuffer, pos, amount, completionUserData, completeCallback);
	}

	void AsyncReadWriteRequester_Win32::PostAppendRequest(IJobQueue &jobQueue, const void *writeBuffer, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completeCallback)
	{
		return m_instance->PostAppendRequest(jobQueue, writeBuffer, amount, completionUserData, completeCallback);
	}

	AsyncFile_Win32::AsyncFile_Win32(AsyncIOThread_Win32 &asioThread, RCPtr<AsyncFileInstance_Win32> &&instance)
		: m_asioThread(asioThread)
		, m_instance(std::move(instance))
	{
	}

	AsyncFile_Win32::~AsyncFile_Win32()
	{
	}

	Result AsyncFile_Win32::CreateReadRequester(UniquePtr<IAsyncReadRequester> &requester)
	{
		RCPtr<AsyncReadWriteRequesterInstance_Win32> requesterInstance;
		RKIT_CHECK(New<AsyncReadWriteRequesterInstance_Win32>(requesterInstance, m_asioThread, m_instance));

		return New<AsyncReadWriteRequester_Win32>(requester, requesterInstance);
	}

	Result AsyncFile_Win32::CreateWriteRequester(UniquePtr<IAsyncWriteRequester> &requester)
	{
		RCPtr<AsyncReadWriteRequesterInstance_Win32> requesterInstance;
		RKIT_CHECK(New<AsyncReadWriteRequesterInstance_Win32>(requesterInstance, m_asioThread, m_instance));

		return New<AsyncReadWriteRequester_Win32>(requester, requesterInstance);
	}

	DirectoryScan_Win32::DirectoryScan_Win32()
		: m_handle(INVALID_HANDLE_VALUE)
		, m_haveItem(true)
		, m_exhausted(false)
		, m_findData{}
	{
	}

	DirectoryScan_Win32::~DirectoryScan_Win32()
	{
		if (m_handle != INVALID_HANDLE_VALUE)
			FindClose(m_handle);
	}

	Result DirectoryScan_Win32::GetNext(bool &haveItem, DirectoryScanItem &outItem)
	{
		for (;;)
		{
			if (m_exhausted)
			{
				haveItem = false;
				return ResultCode::kOK;
			}

			if (m_haveItem && !CheckItem())
				m_haveItem = false;

			if (m_haveItem)
			{
				RKIT_CHECK(ProduceItem(outItem));

				haveItem = true;

				m_haveItem = false;
				return ResultCode::kOK;
			}

			RKIT_CHECK(GetAnotherItem());
		}
	}

	WIN32_FIND_DATAW *DirectoryScan_Win32::GetFindData()
	{
		return &m_findData;
	}

	void DirectoryScan_Win32::SetHandle(HANDLE hdl)
	{
		m_handle = hdl;
	}

	bool DirectoryScan_Win32::CheckItem() const
	{
		if (!wcscmp(m_findData.cFileName, L"."))
			return false;

		if (!wcscmp(m_findData.cFileName, L".."))
			return false;

		return true;
	}

	Result DirectoryScan_Win32::ProduceItem(DirectoryScanItem &outItem)
	{
		RKIT_CHECK(ConvUtil_Win32::UTF16ToUTF8(m_findData.cFileName, m_fileNameUTF8));

		outItem.m_fileName = OSRelPathView(WStringView(m_findData.cFileName, wcslen(m_findData.cFileName)));
		outItem.m_attribs.m_isDirectory = ((m_findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
		outItem.m_attribs.m_fileSize = (static_cast<FilePos_t>(m_findData.nFileSizeHigh) << 32) + m_findData.nFileSizeLow;
		outItem.m_attribs.m_fileTime = ConvUtil_Win32::FileTimeToUTCMSec(m_findData.ftLastWriteTime);

		return ResultCode::kOK;
	}

	Result DirectoryScan_Win32::GetAnotherItem()
	{
		BOOL haveMore = FindNextFileW(m_handle, &m_findData);
		if (haveMore)
			m_haveItem = true;
		else
		{
			m_haveItem = false;
			m_exhausted = true;
		}

		return ResultCode::kOK;
	}

	SystemLibrary_Win32::SystemLibrary_Win32()
		: m_hmodule(nullptr)
	{
	}

	SystemLibrary_Win32::~SystemLibrary_Win32()
	{
		if (m_hmodule)
			FreeLibrary(m_hmodule);
	}

	void SystemLibrary_Win32::SetHModule(const HMODULE &hmodule)
	{
		m_hmodule = hmodule;
	}

	bool SystemLibrary_Win32::GetFunction(void *fnPtr, const StringView &fnName)
	{
		void *procAddr = GetProcAddress(m_hmodule, fnName.GetChars());
		if (!procAddr)
			return false;

		memcpy(fnPtr, &procAddr, sizeof(void *));
		return true;
	}

	Mutex_Win32::Mutex_Win32()
	{
		InitializeCriticalSection(&m_critSection);
	}

	Mutex_Win32::~Mutex_Win32()
	{
		DeleteCriticalSection(&m_critSection);
	}

	void Mutex_Win32::Lock()
	{
		EnterCriticalSection(&m_critSection);
	}

	bool Mutex_Win32::TryLock(uint32_t maxMSec)
	{
		if (TryEnterCriticalSection(&m_critSection))
			return true;

		if (maxMSec > 0)
		{
			ULONGLONG baseTime = GetTickCount64();

			do
			{
				Sleep(1);
				if (TryEnterCriticalSection(&m_critSection))
					return true;
			} while ((GetTickCount64() - baseTime) < maxMSec);
		}

		return false;
	}

	void Mutex_Win32::Unlock()
	{
		LeaveCriticalSection(&m_critSection);
	}

	Event_Win32::Event_Win32()
		: m_event(nullptr)
	{
	}

	Event_Win32::~Event_Win32()
	{
		if (m_event)
			::CloseHandle(m_event);
	}

	void Event_Win32::Signal()
	{
		::SetEvent(m_event);
	}

	void Event_Win32::Reset()
	{
		::ResetEvent(m_event);
	}

	void Event_Win32::Wait()
	{
		::WaitForSingleObject(m_event, INFINITE);
	}

	bool Event_Win32::TimedWait(uint32_t msec)
	{
		if (msec >= INFINITE)
			msec = static_cast<DWORD>(INFINITE) - 1u;

		DWORD waitResult = ::WaitForSingleObject(m_event, msec);

		return (waitResult != WAIT_TIMEOUT);
	}

	Result Event_Win32::Initialize(bool autoReset, bool startSignaled)
	{
		m_event = ::CreateEventW(nullptr, autoReset ? FALSE : TRUE, startSignaled ? TRUE : FALSE, nullptr);
		if (!m_event)
			return ResultCode::kOperationFailed;

		return ResultCode::kOK;
	}

	Thread_Win32::Thread_Win32()
		: m_hThread(nullptr)
		, m_result(ResultCode::kOK)
	{
	}

	void Thread_Win32::SetHandle(HANDLE hThread)
	{
		m_hThread = hThread;
	}

	Thread_Win32::~Thread_Win32()
	{
		if (m_hThread)
		{
			WaitForSingleObject(m_hThread, INFINITE);
			::CloseHandle(m_hThread);
			m_hThread = nullptr;
		}
	}

	Result *Thread_Win32::GetResultPtr()
	{
		return &m_result;
	}

	void Thread_Win32::Finalize(Result &outResult)
	{
		if (m_hThread)
		{
			WaitForSingleObject(m_hThread, INFINITE);
			::CloseHandle(m_hThread);
			m_hThread = nullptr;
		}

		outResult = m_result;
	}

	SystemDriver_Win32::SystemDriver_Win32(IMallocDriver *alloc, const SystemModuleInitParameters_Win32 &initParams)
		: m_commandLine(alloc)
		, m_argvW(nullptr)
		, m_alloc(alloc)
		, m_hInstance(initParams.m_hinst)
		, m_exePathStr(initParams.m_executablePath)
		, m_programDirStr(initParams.m_programDir)
	{
	}

	SystemDriver_Win32::~SystemDriver_Win32()
	{
		m_displayManager.Reset();

		if (m_argvW)
			LocalFree(m_argvW);

		if (m_kernelBaseModule)
			FreeLibrary(m_kernelBaseModule);
	}

	Result SystemDriver_Win32::Initialize()
	{
		LPWSTR cmdLine = GetCommandLineW();

		int numArgsI = 0;
		m_argvW = CommandLineToArgvW(cmdLine, &numArgsI);

		if (!m_argvW)
			return ResultCode::kOutOfMemory;

		size_t numArgs = static_cast<size_t>(numArgsI);

		RKIT_CHECK(m_commandLine.Resize(numArgs));
		RKIT_CHECK(m_commandLineCharBuffers.Resize(numArgs));

		for (size_t i = 0; i < numArgs; i++)
		{
			Vector<char> &charBuffer = m_commandLineCharBuffers[i];

			RKIT_CHECK(ConvUtil_Win32::UTF16ToUTF8(m_argvW[i], charBuffer));

			m_commandLine[i] = StringView(charBuffer.GetBuffer(), charBuffer.Count() - 1);
		}

		RKIT_CHECK(render::DisplayManagerBase_Win32::Create(m_displayManager, m_alloc, m_hInstance));

#if RKIT_IS_DEBUG
		m_kernelBaseModule = LoadLibraryW(L"KernelBase.dll");
		if (m_kernelBaseModule)
			m_setThreadDescriptionProc = reinterpret_cast<SetThreadDescriptionProc_Win32_t>(GetProcAddress(m_kernelBaseModule, "SetThreadDescription"));
		else
			m_setThreadDescriptionProc = nullptr;
#endif

		RKIT_CHECK(New<AsyncIOThread_Win32>(m_asioThread, *this));
		RKIT_CHECK(m_asioThread->Initialize());

		return ResultCode::kOK;
	}

	void SystemDriver_Win32::RemoveCommandLineArgs(size_t firstArg, size_t numArgs)
	{
		if (firstArg >= m_commandLine.Count() || numArgs == 0)
			return;

		size_t maxArgs = m_commandLine.Count() - firstArg;
		if (numArgs > maxArgs)
			numArgs = maxArgs;

		m_commandLine.RemoveRange(firstArg, numArgs);
	}

	Span<const StringView> SystemDriver_Win32::GetCommandLine() const
	{
		return m_commandLine.ToSpan();
	}

	void SystemDriver_Win32::AssertionFailure(const char *expr, const char *file, unsigned int line)
	{
#ifndef NDEBUG
		Vector<wchar_t> exprWChar(m_alloc);
		Vector<wchar_t> fileWChar(m_alloc);

		Result exprConvResult = ConvUtil_Win32::UTF8ToUTF16(expr, exprWChar);
		Result fileConvResult = ConvUtil_Win32::UTF8ToUTF16(file, fileWChar);

		if (!rkit::utils::ResultIsOK(exprConvResult) || !rkit::utils::ResultIsOK(fileConvResult))
		{
			_wassert(L"Failed to convert assertion message", RKIT_PP_CONCAT(L, __FILE__), line);
		}
		else
		{
			_wassert(exprWChar.GetBuffer(), fileWChar.GetBuffer(), line);
		}
#else
		abort();
#endif
	}

	void SystemDriver_Win32::FirstChanceResultFailure(const Result &result)
	{
#if RKIT_IS_DEBUG
		if (IsDebuggerPresent())
			DebugBreak();
#endif
	}

	DWORD SystemDriver_Win32::OpenFlagsToDisposition(bool createIfNotExists, bool truncateIfExists)
	{
		if (createIfNotExists)
		{
			if (truncateIfExists)
				return CREATE_ALWAYS;
			else
				return OPEN_ALWAYS;
		}
		else
		{
			if (truncateIfExists)
				return TRUNCATE_EXISTING;
			else
				return OPEN_EXISTING;
		}
	}

	Result SystemDriver_Win32::AsyncOpenFileRead(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const Future<UniquePtr<ISeekableReadStream>> &outStream, FileLocation location, const CIPathView &path)
	{
		OSAbsPath absPath;
		RKIT_CHECK(ResolveAbsPath(absPath, location, path));

		return AsyncOpenFileReadAbs(jobQueue, outOpenJob, dependencyJob, outStream, absPath);
	}

	Result SystemDriver_Win32::AsyncOpenFileReadAbs(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const Future<UniquePtr<ISeekableReadStream>> &outStream, const OSAbsPathView &path)
	{
		OSAbsPath pathCopy;
		RKIT_CHECK(pathCopy.Set(path));

		UniquePtr<OpenFileReadJobRunner> runner;
		RKIT_CHECK(New<OpenFileReadJobRunner>(runner, outStream, pathCopy, *this));

		RKIT_CHECK(jobQueue.CreateJob(&outOpenJob, JobType::kIO, std::move(runner), dependencyJob));

		return ResultCode::kOK;
	}

	Result SystemDriver_Win32::AsyncOpenFileAsyncRead(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const Future<AsyncFileOpenReadResult> &outStream, FileLocation location, const CIPathView &path)
	{
		OSAbsPath absPath;
		RKIT_CHECK(ResolveAbsPath(absPath, location, path));

		return AsyncOpenFileAsyncReadAbs(jobQueue, outOpenJob, dependencyJob, outStream, absPath);
	}

	Result SystemDriver_Win32::AsyncOpenFileAsyncReadAbs(IJobQueue &jobQueue, RCPtr<Job> &outOpenJob, Job *dependencyJob, const Future<AsyncFileOpenReadResult> &outStream, const OSAbsPathView &path)
	{
		OSAbsPath pathCopy;
		RKIT_CHECK(pathCopy.Set(path));

		UniquePtr<OpenFileAsyncReadJobRunner> runner;
		RKIT_CHECK(New<OpenFileAsyncReadJobRunner>(runner, outStream, pathCopy, *this));

		RKIT_CHECK(jobQueue.CreateJob(&outOpenJob, JobType::kIO, std::move(runner), dependencyJob));

		return ResultCode::kOK;
	}

	Result SystemDriver_Win32::OpenFileRead(UniquePtr<ISeekableReadStream> &outStream, FileLocation location, const CIPathView &path)
	{
		OSAbsPath absPath;
		RKIT_CHECK(ResolveAbsPath(absPath, location, path));

		return OpenFileReadAbs(outStream, absPath);
	}

	Result SystemDriver_Win32::OpenFileReadAbs(UniquePtr<ISeekableReadStream> &outStream, const OSAbsPathView &path)
	{
		UniquePtr<File_Win32> file;
		RKIT_CHECK(OpenFileGeneral(file, path, false, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0));

		outStream = std::move(file);
		return ResultCode::kOK;
	}

	Result SystemDriver_Win32::OpenFileAsyncRead(AsyncFileOpenReadResult &outStream, FileLocation location, const CIPathView &path)
	{
		OSAbsPath absPath;
		RKIT_CHECK(ResolveAbsPath(absPath, location, path));

		return OpenFileAsyncReadAbs(outStream, absPath);
	}

	Result SystemDriver_Win32::OpenFileAsyncReadAbs(AsyncFileOpenReadResult &outStream, const OSAbsPathView &path)
	{
		UniquePtr<AsyncFile_Win32> file;
		FilePos_t initialSize = 0;
		RKIT_CHECK(OpenFileAsyncGeneral(file, initialSize, path, false, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0));

		outStream.m_file = std::move(file);
		outStream.m_initialSize = initialSize;

		return ResultCode::kOK;
	}

	Result SystemDriver_Win32::OpenFileWrite(UniquePtr<ISeekableWriteStream> &outStream, FileLocation location, const CIPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists)
	{
		OSAbsPath absPath;
		RKIT_CHECK(ResolveAbsPath(absPath, location, path));

		return OpenFileWriteAbs(outStream, absPath, createIfNotExists, createDirectories, truncateIfExists);
	}

	Result SystemDriver_Win32::OpenFileWriteAbs(UniquePtr<ISeekableWriteStream> &outStream, const OSAbsPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists)
	{
		UniquePtr<File_Win32> file;
		RKIT_CHECK(OpenFileGeneral(file, path, createDirectories, GENERIC_WRITE, FILE_SHARE_READ, OpenFlagsToDisposition(createIfNotExists, truncateIfExists), 0));

		outStream = std::move(file);
		return ResultCode::kOK;
	}

	Result SystemDriver_Win32::OpenFileReadWrite(UniquePtr<ISeekableReadWriteStream> &outStream, FileLocation location, const CIPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists)
	{
		OSAbsPath absPath;
		RKIT_CHECK(ResolveAbsPath(absPath, location, path));

		return OpenFileReadWriteAbs(outStream, absPath, createIfNotExists, createDirectories, truncateIfExists);
	}

	Result SystemDriver_Win32::OpenFileReadWriteAbs(UniquePtr<ISeekableReadWriteStream> &outStream, const OSAbsPathView &path, bool createIfNotExists, bool createDirectories, bool truncateIfExists)
	{
		UniquePtr<File_Win32> file;
		RKIT_CHECK(OpenFileGeneral(file, path, createDirectories, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, OpenFlagsToDisposition(createIfNotExists, truncateIfExists), 0));

		outStream = std::move(file);
		return ResultCode::kOK;
	}

	Result SystemDriver_Win32::CreateThread(UniqueThreadRef &outThread, UniquePtr<IThreadContext> &&threadContextRef, const StringView &threadName)
	{
		UniquePtr<IThreadContext> threadContext(std::move(threadContextRef));

		HANDLE hKickoffEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);

		if (!hKickoffEvent)
			return ResultCode::kOperationFailed;

		UniquePtr<Thread_Win32> thread;
		Result createThreadResult = NewWithAlloc<Thread_Win32>(thread, m_alloc);

		if (!utils::ResultIsOK(createThreadResult))
		{
			::CloseHandle(hKickoffEvent);
			return createThreadResult;
		}


		ThreadKickoffInfo_Win32 kickoffInfo;
		kickoffInfo.m_hKickoffEvent = hKickoffEvent;
		kickoffInfo.m_outResult = thread->GetResultPtr();
		kickoffInfo.m_threadContext = std::move(threadContext);

#if RKIT_IS_DEBUG
		Vector<wchar_t> threadNameVector(m_alloc);
		RKIT_CHECK(ConvUtil_Win32::UTF8ToUTF16(threadName.GetChars(), threadNameVector));

		kickoffInfo.m_threadName = threadNameVector.GetBuffer();
		kickoffInfo.m_setThreadDescriptionProc = m_setThreadDescriptionProc;
#endif

		DWORD threadID = 0;
		HANDLE hThread = ::CreateThread(nullptr, 0, ThreadStartRoutine, &kickoffInfo, 0, &threadID);

		if (!hThread)
		{
			::CloseHandle(hKickoffEvent);
			return ResultCode::kOperationFailed;
		}

		WaitForSingleObject(hKickoffEvent, INFINITE);
		CloseHandle(hKickoffEvent);

		thread->SetHandle(hThread);

		outThread = UniqueThreadRef(UniquePtr<IThread>(std::move(thread)));

		return ResultCode::kOK;
	}

	Result SystemDriver_Win32::CreateMutex(UniquePtr<IMutex> &outMutex)
	{
		UniquePtr<Mutex_Win32> mutex;
		RKIT_CHECK(NewWithAlloc<Mutex_Win32>(mutex, m_alloc));

		outMutex = std::move(mutex);

		return ResultCode::kOK;
	}

	Result SystemDriver_Win32::CreateEvent(UniquePtr<IEvent> &outEvent, bool autoReset, bool startSignaled)
	{
		UniquePtr<Event_Win32> event;
		RKIT_CHECK(NewWithAlloc<Event_Win32>(event, m_alloc));

		RKIT_CHECK(event->Initialize(autoReset, startSignaled));

		outEvent = std::move(event);

		return ResultCode::kOK;
	}

	void SystemDriver_Win32::SleepMSec(uint32_t msec) const
	{
		::Sleep(msec);
	}

	Result SystemDriver_Win32::OpenDirectoryScan(FileLocation location, const CIPathView &path, UniquePtr<IDirectoryScan> &outDirectoryScan)
	{
		OSAbsPath absPath;
		RKIT_CHECK(ResolveAbsPath(absPath, location, path));

		return OpenDirectoryScanAbs(absPath, outDirectoryScan);
	}

	Result SystemDriver_Win32::GetFileAttributes(FileLocation location, const CIPathView &path, bool &outExists, FileAttributes &outAttribs)
	{
		OSAbsPath absPath;
		RKIT_CHECK(ResolveAbsPath(absPath, location, path));

		return GetFileAttributesAbs(absPath, outExists, outAttribs);
	}


	Result SystemDriver_Win32::CopyFileFromAbsToAbs(const OSAbsPathView &srcPath, const OSAbsPathView &destPath, bool overwrite)
	{
		BOOL succeeded = ::CopyFileW(srcPath.GetChars(), destPath.GetChars(), !overwrite);

		if (succeeded)
			return ResultCode::kOK;
		else
			return ResultCode::kIOError;
	}

	Result SystemDriver_Win32::CopyFileToAbs(FileLocation location, const CIPathView &path, const OSAbsPathView &destPath, bool overwrite)
	{
		OSAbsPath srcPath;
		RKIT_CHECK(ResolveAbsPath(srcPath, location, path));

		return CopyFileFromAbsToAbs(srcPath, destPath, overwrite);
	}

	Result SystemDriver_Win32::CopyFileFromAbs(const OSAbsPathView &srcPath, FileLocation location, const CIPathView &path, bool overwrite)
	{
		OSAbsPath destPath;
		RKIT_CHECK(ResolveAbsPath(destPath, location, path));

		return CopyFileFromAbsToAbs(srcPath, destPath, overwrite);
	}

	Result SystemDriver_Win32::CopyFile(FileLocation srcLocation, const CIPathView &srcPath, FileLocation destLocation, const CIPathView &destPath, bool overwrite)
	{
		OSAbsPath osSrcPath;
		RKIT_CHECK(ResolveAbsPath(osSrcPath, srcLocation, srcPath));

		OSAbsPath osDestPath;
		RKIT_CHECK(ResolveAbsPath(osDestPath, destLocation, destPath));

		return CopyFileFromAbsToAbs(osSrcPath, osDestPath, overwrite);
	}

	Result SystemDriver_Win32::MoveFileFromAbsToAbs(const OSAbsPathView &srcPath, const OSAbsPathView &destPath, bool overwrite)
	{
		DWORD flags = MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH;
		if (overwrite)
			flags |= MOVEFILE_REPLACE_EXISTING;

		BOOL succeeded = ::MoveFileExW(srcPath.GetChars(), destPath.GetChars(), flags);

		if (succeeded)
			return ResultCode::kOK;
		else
			return ResultCode::kIOError;
	}

	Result SystemDriver_Win32::MoveFileToAbs(FileLocation location, const CIPathView &path, const OSAbsPathView &destPath, bool overwrite)
	{
		OSAbsPath srcPath;
		RKIT_CHECK(ResolveAbsPath(srcPath, location, path));

		return MoveFileFromAbsToAbs(srcPath, destPath, overwrite);
	}

	Result SystemDriver_Win32::MoveFileFromAbs(const OSAbsPathView &srcPath, FileLocation location, const CIPathView &path, bool overwrite)
	{
		OSAbsPath destPath;
		RKIT_CHECK(ResolveAbsPath(destPath, location, path));

		return MoveFileFromAbsToAbs(srcPath, destPath, overwrite);
	}

	Result SystemDriver_Win32::MoveFile(FileLocation srcLocation, const CIPathView &srcPath, FileLocation destLocation, const CIPathView &destPath, bool overwrite)
	{
		OSAbsPath osSrcPath;
		RKIT_CHECK(ResolveAbsPath(osSrcPath, srcLocation, srcPath));

		OSAbsPath osDestPath;
		RKIT_CHECK(ResolveAbsPath(osDestPath, destLocation, destPath));

		return MoveFileFromAbsToAbs(osSrcPath, osDestPath, overwrite);
	}

	Result SystemDriver_Win32::OpenDirectoryScanAbs(const OSAbsPathView &path, UniquePtr<IDirectoryScan> &outDirectoryScan)
	{
		UniquePtr<DirectoryScan_Win32> dirScan;
		RKIT_CHECK(New<DirectoryScan_Win32>(dirScan));

		ConstSpan<wchar_t> pathChars = path.ToStringView().ToSpan();

		Vector<wchar_t> wildcardPath;
		RKIT_CHECK(wildcardPath.Resize(pathChars.Count()));

		CopySpanNonOverlapping(wildcardPath.ToSpan(), pathChars);

		RKIT_CHECK(wildcardPath.Append(ConstSpan<wchar_t>(L"\\*", 3)));

		HANDLE ffHandle = FindFirstFileW(wildcardPath.GetBuffer(), dirScan->GetFindData());
		if (ffHandle == INVALID_HANDLE_VALUE)
			return ResultCode::kFileOpenError;

		dirScan->SetHandle(ffHandle);

		outDirectoryScan = dirScan.StaticCast<IDirectoryScan>();

		return ResultCode::kOK;
	}

	Result SystemDriver_Win32::GetFileAttributesAbs(const OSAbsPathView &path, bool &outExists, FileAttributes &outAttribs)
	{
		DWORD attribs = GetFileAttributesW(path.GetChars());

		if (attribs == INVALID_FILE_ATTRIBUTES)
		{
			outExists = false;
			outAttribs = FileAttributes();
			return ResultCode::kOK;
		}

		if ((attribs & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			outExists = true;
			outAttribs = FileAttributes();
			outAttribs.m_isDirectory = true;
			return ResultCode::kOK;
		}

		HANDLE hFile = CreateFileW(path.GetChars(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (hFile == INVALID_HANDLE_VALUE)
			return ResultCode::kFileOpenError;

		BY_HANDLE_FILE_INFORMATION finfo;
		BOOL gotAttribs = GetFileInformationByHandle(hFile, &finfo);

		CloseHandle(hFile);

		if (!gotAttribs)
			return ResultCode::kFileOpenError;

		outExists = true;
		outAttribs = FileAttributes();
		outAttribs.m_fileSize = (static_cast<uint64_t>(finfo.nFileSizeHigh) << 32) + finfo.nFileSizeLow;
		outAttribs.m_fileTime = ConvUtil_Win32::FileTimeToUTCMSec(finfo.ftLastWriteTime);
		outAttribs.m_isDirectory = false;

		return ResultCode::kOK;
	}

	Result SystemDriver_Win32::SetGameDirectoryOverride(const OSAbsPathView &path)
	{
		return m_gameDirectoryOverride.Set(path);
	}

	Result SystemDriver_Win32::SetSettingsDirectory(const StringView &path)
	{
		BaseStringConstructionBuffer<wchar_t> docsPathStrBuf;

		wchar_t *docsPath = nullptr;
		HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docsPath);
		if (!SUCCEEDED(hr))
			return utils::CreateResultWithExtCode(ResultCode::kOperationFailed, static_cast<uint32_t>(hr));

		size_t docsPathLength = wcslen(docsPath);

		Result resizeResult = docsPathStrBuf.Allocate(docsPathLength);
		if (!utils::ResultIsOK(resizeResult))
		{
			CoTaskMemFree(docsPath);
			return resizeResult;
		}

		CopySpanNonOverlapping(docsPathStrBuf.GetSpan().SubSpan(0, docsPathLength), ConstSpan<wchar_t>(docsPath, docsPathLength));
		CoTaskMemFree(docsPath);

		OSAbsPath osPath;
		RKIT_CHECK(osPath.Set(WString(std::move(docsPathStrBuf))));

		OSRelPath settingsSubPath;
		RKIT_CHECK(settingsSubPath.SetFromUTF8(path));

		RKIT_CHECK(osPath.Append(settingsSubPath));

		m_settingsDirectory = osPath;

		return ResultCode::kOK;
	}

	char SystemDriver_Win32::GetPathSeparator() const
	{
		return '\\';
	}

	IPlatformDriver *SystemDriver_Win32::GetPlatformDriver() const
	{
		return const_cast<SystemDriver_Win32 *>(this);
	}

	Result SystemDriver_Win32::OpenSystemLibrary(UniquePtr<ISystemLibrary> &outLibrary, SystemLibraryType libType) const
	{
		IMallocDriver *alloc = GetDrivers().m_mallocDriver;

		const wchar_t *libName = nullptr;

		switch (libType)
		{
		case SystemLibraryType::kVulkan:
			libName = L"vulkan-1.dll";
			break;
		default:
			break;
		}

		if (!libName)
			return ResultCode::kInvalidParameter;

		UniquePtr<SystemLibrary_Win32> sysLibrary;
		RKIT_CHECK(New<SystemLibrary_Win32>(sysLibrary));

		HMODULE hmodule = LoadLibraryW(libName);
		if (!hmodule)
			return ResultCode::kFileOpenError;

		sysLibrary->SetHModule(hmodule);
		outLibrary = std::move(sysLibrary);

		return ResultCode::kOK;
	}

	uint32_t SystemDriver_Win32::GetProcessorCount() const
	{
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);

		return sysInfo.dwNumberOfProcessors;
	}

	render::IDisplayManager *SystemDriver_Win32::GetDisplayManager() const
	{
		return m_displayManager.Get();
	}

	HINSTANCE SystemDriver_Win32::GetHInstance() const
	{
		return m_hInstance;
	}

	Result SystemDriver_Win32::CheckCreateDirectories(Vector<wchar_t> &pathChars)
	{
		size_t dirScanTermination = 0;
		for (size_t i = 0; i < pathChars.Count(); i++)
		{
			if (pathChars[i] == ':')
				dirScanTermination = i + 2;
		}

		size_t i = pathChars.Count() - 1;

		size_t firstFailedDirectoryPos = 0;

		while (i > dirScanTermination)
		{
			if (pathChars[i] == '\\')
			{
				pathChars[i] = 0;
				BOOL isDirectory = PathIsDirectoryW(pathChars.GetBuffer());
				pathChars[i] = '\\';
				if (isDirectory)
					break;
				else
					firstFailedDirectoryPos = i;
			}

			i--;
		}

		if (firstFailedDirectoryPos != 0)
		{
			for (size_t i = firstFailedDirectoryPos; i < pathChars.Count(); i++)
			{
				if (pathChars[i] == '\\')
				{
					pathChars[i] = 0;

					if (!CreateDirectoryW(pathChars.GetBuffer(), nullptr))
						return ResultCode::kFileOpenError;

					pathChars[i] = '\\';
				}
			}
		}

		return ResultCode::kOK;
	}

	DWORD WINAPI SystemDriver_Win32::ThreadStartRoutine(LPVOID lpThreadParameter)
	{
		ThreadKickoffInfo_Win32 *kickoff = static_cast<ThreadKickoffInfo_Win32 *>(lpThreadParameter);

		HANDLE hKickoffEvent = kickoff->m_hKickoffEvent;
		Result *outResult = kickoff->m_outResult;
		UniquePtr<IThreadContext> context = std::move(kickoff->m_threadContext);

#if RKIT_IS_DEBUG
		if (kickoff->m_threadName && kickoff->m_threadName[0] && kickoff->m_setThreadDescriptionProc)
			kickoff->m_setThreadDescriptionProc(GetCurrentThread(), kickoff->m_threadName);
#endif

		::SetEvent(hKickoffEvent);

		*outResult = context->Run();

		return 0;
	}

	Result SystemDriver_Win32::ResolveAbsPath(OSAbsPath &outPath, FileLocation location, const CIPathView &path)
	{
	
		switch (location)
		{
		case FileLocation::kProgramDirectory:
			RKIT_CHECK(outPath.Set(m_programDirStr));
			break;

		case FileLocation::kGameDirectory:
			{
				if (m_gameDirectoryOverride.Length() == 0)
					return ResultCode::kInvalidParameter;

				outPath = m_gameDirectoryOverride;
			}
			break;

		case FileLocation::kUserSettingsDirectory:
			{
				if (m_settingsDirectory.Length() == 0)
					return ResultCode::kInvalidParameter;

				outPath = m_settingsDirectory;
			}
			break;

		default:
			return ResultCode::kInvalidParameter;
		}

		OSRelPath relPath;
		RKIT_CHECK(relPath.ConvertFrom(path));
		RKIT_CHECK(outPath.Append(relPath));

		return ResultCode::kOK;
	}

	Result SystemDriver_Win32::OpenFileGeneral(UniquePtr<File_Win32> &outFile, const OSAbsPathView &path, bool createDirectories, DWORD access, DWORD shareMode, DWORD disposition, DWORD extraFlags)
	{
		if (createDirectories)
		{
			Vector<wchar_t> dirCharsVector;
			RKIT_CHECK(dirCharsVector.Resize(path.Length() + 1));

			CopySpanNonOverlapping(dirCharsVector.ToSpan().SubSpan(0, path.Length()), path.ToStringView().ToSpan());
			dirCharsVector[path.Length()] = L'\0';

			RKIT_CHECK(CheckCreateDirectories(dirCharsVector));
		}

		HANDLE fHandle = CreateFileW(path.GetChars(), access, shareMode, nullptr, disposition, FILE_ATTRIBUTE_NORMAL | extraFlags, nullptr);

		if (fHandle == INVALID_HANDLE_VALUE)
			return utils::SoftFaultResult(ResultCode::kFileOpenError);

		LARGE_INTEGER fileSize;
		if (!GetFileSizeEx(fHandle, &fileSize))
		{
			CloseHandle(fHandle);
			return ResultCode::kFileOpenError;
		}

		Result createObjResult = New<File_Win32>(outFile, fHandle, static_cast<FilePos_t>(fileSize.QuadPart));
		if (!utils::ResultIsOK(createObjResult))
		{
			CloseHandle(fHandle);
			return ResultCode::kFileOpenError;
		}

		return ResultCode::kOK;
	}

	Result SystemDriver_Win32::OpenFileAsyncGeneral(UniquePtr<AsyncFile_Win32> &outStream, FilePos_t &outInitialSize, const OSAbsPathView &path, bool createDirectories, DWORD access, DWORD shareMode, DWORD disposition, DWORD extraFlags)
	{
		UniquePtr<File_Win32> file;
		RKIT_CHECK(OpenFileGeneral(file, path, createDirectories, access, shareMode, disposition, extraFlags | FILE_FLAG_OVERLAPPED));

		const FilePos_t initialSize = file->GetSize();

		RCPtr<AsyncFileInstance_Win32> asyncFileInstance;
		RKIT_CHECK(New<AsyncFileInstance_Win32>(asyncFileInstance, std::move(file)));

		UniquePtr<AsyncFile_Win32> stream;
		RKIT_CHECK(New<AsyncFile_Win32>(stream, *m_asioThread, std::move(asyncFileInstance)));

		outStream = std::move(stream);
		outInitialSize = initialSize;

		return ResultCode::kOK;
	}

	OpenFileReadJobRunner::OpenFileReadJobRunner(const Future<UniquePtr<ISeekableReadStream>> &streamFuture, const OSAbsPath &filePath, ISystemDriver &systemDriver)
		: m_streamFuture(streamFuture)
		, m_filePath(filePath)
		, m_systemDriver(systemDriver)
	{
		RKIT_ASSERT(streamFuture.GetFutureContainer().IsValid());
	}

	OpenFileReadJobRunner::~OpenFileReadJobRunner()
	{
		if (!m_completed)
			this->m_streamFuture.GetFutureContainer()->Fail();
	}

	Result OpenFileReadJobRunner::Run()
	{
		Result runResult = CheckedRun();
		if (!utils::ResultIsOK(runResult))
			m_streamFuture.GetFutureContainer()->Fail();

		return runResult;
	}

	Result OpenFileReadJobRunner::CheckedRun()
	{
		UniquePtr<ISeekableReadStream> stream;
		RKIT_CHECK(m_systemDriver.OpenFileReadAbs(stream, m_filePath));

		m_completed = true;
		m_streamFuture.GetFutureContainer()->Complete(std::move(stream));

		return ResultCode::kOK;
	}


	OpenFileAsyncReadJobRunner::OpenFileAsyncReadJobRunner(const Future<AsyncFileOpenReadResult> &streamFuture, const OSAbsPath &filePath, ISystemDriver &systemDriver)
		: m_streamFuture(streamFuture)
		, m_filePath(filePath)
		, m_systemDriver(systemDriver)
	{
	}

	OpenFileAsyncReadJobRunner::~OpenFileAsyncReadJobRunner()
	{
		if (!m_completed)
			this->m_streamFuture.GetFutureContainer()->Fail();
	}

	Result OpenFileAsyncReadJobRunner::Run()
	{
		Result runResult = CheckedRun();
		if (!utils::ResultIsOK(runResult))
		{
			m_streamFuture.GetFutureContainer()->Fail();
			m_completed = true;
		}

		return runResult;
	}

	Result OpenFileAsyncReadJobRunner::CheckedRun()
	{
		AsyncFileOpenReadResult result;
		RKIT_CHECK(m_systemDriver.OpenFileAsyncReadAbs(result, m_filePath));

		m_completed = true;
		m_streamFuture.GetFutureContainer()->Complete(std::move(result));

		return ResultCode::kOK;
	}

	Result SystemModule_Win32::Init(const ModuleInitParameters *baseInitParams)
	{
		IMallocDriver *alloc = GetDrivers().m_mallocDriver;

		UniquePtr<SystemDriver_Win32> driver;

		RKIT_CHECK(NewWithAlloc<SystemDriver_Win32>(driver, alloc, alloc, *static_cast<const SystemModuleInitParameters_Win32 *>(baseInitParams)));
		ms_systemDriver = driver.Detach();
		GetMutableDrivers().m_systemDriver = ms_systemDriver;

		RKIT_CHECK(ms_systemDriver->Initialize());

		return ResultCode::kOK;
	}

	void SystemModule_Win32::Shutdown()
	{
		SafeDelete(ms_systemDriver);
		GetMutableDrivers().m_systemDriver = ms_systemDriver;
	}

	SimpleObjectAllocation<SystemDriver_Win32> SystemModule_Win32::ms_systemDriver;
}

RKIT_IMPLEMENT_MODULE("RKit", "System_Win32", ::rkit::SystemModule_Win32)
