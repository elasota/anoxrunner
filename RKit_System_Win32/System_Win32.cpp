#include "rkit/Core/Algorithm.h"
#include "rkit/Core/DirectoryScan.h"
#include "rkit/Core/Drivers.h"
#include "rkit/Core/Event.h"
#include "rkit/Core/MallocDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Span.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Thread.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"

#include "rkit/Win32/Win32PlatformDriver.h"
#include "rkit/Win32/SystemModuleInitParameters_Win32.h"

#include "Win32DisplayManager.h"
#include "ConvUtil.h"

#include <shellapi.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <timezoneapi.h>
#include <KnownFolders.h>


namespace rkit
{
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

		bool Truncate(FilePos_t pos) override;

	private:
		HANDLE m_hfile;
		FilePos_t m_filePos;
		FilePos_t m_fileSize;
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

	class Thread_Win32;

	struct ThreadKickoffInfo_Win32
	{
		HANDLE m_hKickoffEvent;
		Result *m_outResult;
		UniquePtr<IThreadContext> m_threadContext;
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

		UniquePtr<ISeekableReadStream> OpenFileRead(FileLocation location, const char *path) override;
		UniquePtr<ISeekableWriteStream> OpenFileWrite(FileLocation location, const char *path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) override;
		UniquePtr<ISeekableReadWriteStream> OpenFileReadWrite(FileLocation location, const char *path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) override;

		Result CreateThread(UniqueThreadRef &outThread, UniquePtr<IThreadContext> &&threadContext) override;
		Result CreateMutex(UniquePtr<IMutex> &mutex) override;
		Result CreateEvent(UniquePtr<IEvent> &outEvent, bool autoReset, bool startSignaled) override;
		void SleepMSec(uint32_t msec) const override;

		Result OpenDirectoryScan(FileLocation location, const char *path, UniquePtr<IDirectoryScan> &outDirectoryScan) override;
		Result GetFileAttributes(FileLocation location, const char *path, bool &outExists, FileAttributes &outAttribs) override;

		Result ResolveFilePath(FileLocation location, const char *path, Vector<wchar_t> &outPath);

		Result SetGameDirectoryOverride(const StringView &path) override;
		Result SetSettingsDirectory(const StringView &path) override;
		char GetPathSeparator() const override;

		IPlatformDriver *GetPlatformDriver() const override;

		HINSTANCE GetHInstance() const override;

		Result OpenSystemLibrary(UniquePtr<ISystemLibrary> &outLibrary, SystemLibraryType libType) const override;

		uint32_t GetProcessorCount() const override;

		render::IDisplayManager *GetDisplayManager() const override;

	private:
		static DWORD OpenFlagsToDisposition(bool createIfNotExists, bool truncateIfExists);
		UniquePtr<File_Win32> OpenFileGeneral(FileLocation location, const char *path, bool createDirectories, DWORD access, DWORD shareMode, DWORD disposition);
		Result OpenFileGeneralChecked(UniquePtr<File_Win32> &outFile, FileLocation location, const char *path, bool createDirectories, DWORD access, DWORD shareMode, DWORD disposition);
		static Result CheckCreateDirectories(Vector<wchar_t> &pathChars);

		static DWORD WINAPI ThreadStartRoutine(LPVOID lpThreadParameter);

		IMallocDriver *m_alloc;
		Vector<Vector<char> > m_commandLineCharBuffers;
		Vector<StringView> m_commandLine;
		UniquePtr<render::DisplayManagerBase_Win32> m_displayManager;
		LPWSTR *m_argvW;

		WString m_gameDirectoryOverride;
		WString m_settingsDirectory;

		HINSTANCE m_hInstance;
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

	bool File_Win32::Truncate(FilePos_t newSize)
	{
		if (m_fileSize == newSize)
			return true;

		if (m_fileSize < newSize)
			return false;

		FilePos_t oldFilePos = m_filePos;

		if (newSize != m_filePos)
		{
			LARGE_INTEGER newPos;
			newPos.QuadPart = static_cast<LONGLONG>(newSize);

			if (!::SetFilePointerEx(m_hfile, newPos, nullptr, FILE_BEGIN))
				return false;
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

		return (succeeded != FALSE);
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

		outItem.m_fileName = StringView(m_fileNameUTF8.GetBuffer(), m_fileNameUTF8.Count() - 1);
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
	{
	}

	SystemDriver_Win32::~SystemDriver_Win32()
	{
		m_displayManager.Reset();

		if (m_argvW)
			LocalFree(m_argvW);
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

		if (!exprConvResult.IsOK() || !fileConvResult.IsOK())
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

	UniquePtr<ISeekableReadStream> SystemDriver_Win32::OpenFileRead(FileLocation location, const char *path)
	{
		return OpenFileGeneral(location, path, false, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING);
	}

	UniquePtr<ISeekableWriteStream> SystemDriver_Win32::OpenFileWrite(FileLocation location, const char *path, bool createIfNotExists, bool createDirectories, bool truncateIfExists)
	{
		return OpenFileGeneral(location, path, createDirectories, GENERIC_WRITE, FILE_SHARE_READ, OpenFlagsToDisposition(createIfNotExists, truncateIfExists));
	}

	UniquePtr<ISeekableReadWriteStream> SystemDriver_Win32::OpenFileReadWrite(FileLocation location, const char *path, bool createIfNotExists, bool createDirectories, bool truncateIfExists)
	{
		return OpenFileGeneral(location, path, createDirectories, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, OpenFlagsToDisposition(createIfNotExists, truncateIfExists));
	}

	Result SystemDriver_Win32::CreateThread(UniqueThreadRef &outThread, UniquePtr<IThreadContext> &&threadContextRef)
	{
		UniquePtr<IThreadContext> threadContext(std::move(threadContextRef));

		HANDLE hKickoffEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);

		if (!hKickoffEvent)
			return ResultCode::kOperationFailed;

		UniquePtr<Thread_Win32> thread;
		Result createThreadResult = NewWithAlloc<Thread_Win32>(thread, m_alloc);

		if (!createThreadResult.IsOK())
		{
			::CloseHandle(hKickoffEvent);
			return createThreadResult;
		}

		ThreadKickoffInfo_Win32 kickoffInfo;
		kickoffInfo.m_hKickoffEvent = hKickoffEvent;
		kickoffInfo.m_outResult = thread->GetResultPtr();
		kickoffInfo.m_threadContext = std::move(threadContext);

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

	Result SystemDriver_Win32::OpenDirectoryScan(FileLocation location, const char *path, UniquePtr<IDirectoryScan> &outDirectoryScan)
	{
		UniquePtr<DirectoryScan_Win32> dirScan;
		RKIT_CHECK(New<DirectoryScan_Win32>(dirScan));

		Vector<wchar_t> pathW;
		RKIT_CHECK(ResolveFilePath(location, path, pathW));

		RKIT_CHECK(pathW.Resize(pathW.Count() - 1));

		while (pathW.Count() > 0)
		{
			wchar_t lastChar = pathW[pathW.Count() - 1];

			if (lastChar != '/' && lastChar != '\\')
				break;

			RKIT_CHECK(pathW.Resize(pathW.Count() - 1));
		}

		RKIT_CHECK(pathW.Append(L'\\'));
		RKIT_CHECK(pathW.Append(L'*'));
		RKIT_CHECK(pathW.Append(L'\0'));

		HANDLE ffHandle = FindFirstFileW(pathW.GetBuffer(), dirScan->GetFindData());
		if (ffHandle == INVALID_HANDLE_VALUE)
			return ResultCode::kFileOpenError;

		dirScan->SetHandle(ffHandle);

		outDirectoryScan = dirScan.StaticCast<IDirectoryScan>();

		return ResultCode::kOK;
	}

	Result SystemDriver_Win32::GetFileAttributes(FileLocation location, const char *path, bool &outExists, FileAttributes &outAttribs)
	{
		Vector<wchar_t> pathW;

		RKIT_CHECK(ResolveFilePath(location, path, pathW));

		DWORD attribs = GetFileAttributesW(pathW.GetBuffer());

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

		HANDLE hFile = CreateFileW(pathW.GetBuffer(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

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

	Result SystemDriver_Win32::SetGameDirectoryOverride(const StringView &path)
	{
		Vector<wchar_t> str;
		RKIT_CHECK(ConvUtil_Win32::UTF8ToUTF16(path.GetChars(), str));
		str[str.Count() - 1] = L'\\';

		return m_gameDirectoryOverride.Set(str.ToSpan());
	}

	Result SystemDriver_Win32::SetSettingsDirectory(const StringView &path)
	{
		Vector<wchar_t> fullPath;

		wchar_t *docsPath = nullptr;
		HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docsPath);
		if (!SUCCEEDED(hr))
			return Result(ResultCode::kOperationFailed, static_cast<uint32_t>(hr));

		Result resizeResult = fullPath.Resize(wcslen(docsPath) + 1);
		if (!resizeResult.IsOK())
		{
			CoTaskMemFree(docsPath);
			return resizeResult;
		}
		wcscpy(fullPath.GetBuffer(), docsPath);

		CoTaskMemFree(docsPath);

		fullPath[fullPath.Count() - 1] = L'\\';

		Vector<wchar_t> convertedPath;
		RKIT_CHECK(ConvUtil_Win32::UTF8ToUTF16(path.GetChars(), convertedPath));

		RKIT_CHECK(fullPath.Append(convertedPath.ToSpan()));
		fullPath[fullPath.Count() - 1] = L'\\';
		RKIT_CHECK(fullPath.Append(L'\0'));

		return m_settingsDirectory.Set(fullPath.ToSpan());
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

	UniquePtr<File_Win32> SystemDriver_Win32::OpenFileGeneral(FileLocation location, const char *path, bool createDirectories, DWORD access, DWORD shareMode, DWORD disposition)
	{
		UniquePtr<File_Win32> filePtr;
		Result result = OpenFileGeneralChecked(filePtr, location, path, createDirectories, access, shareMode, disposition);
		if (!result.IsOK())
			return UniquePtr<File_Win32>();

		return filePtr;
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

		::SetEvent(hKickoffEvent);

		*outResult = context->Run();

		return 0;
	}

	Result SystemDriver_Win32::ResolveFilePath(FileLocation location, const char *path, Vector<wchar_t> &outPath)
	{
		Vector<wchar_t> pathW;

		RKIT_CHECK(ConvUtil_Win32::UTF8ToUTF16(path, pathW));

		Vector<wchar_t> filePathWChars;
		const wchar_t *baseW = nullptr;	// Base path, must end with dir separator

		switch (location)
		{
		case FileLocation::kConfigDirectory:
			{
				DWORD cwdBufferSize = GetCurrentDirectoryW(0, nullptr);
				if (cwdBufferSize == 0)
					return ResultCode::kIOError;

				RKIT_CHECK(filePathWChars.Resize(cwdBufferSize + 1));

				DWORD cwdLen = GetCurrentDirectoryW(cwdBufferSize, filePathWChars.GetBuffer());
				if (cwdLen != cwdBufferSize - 1)
					return ResultCode::kIOError;

				filePathWChars[cwdLen] = L'\\';
				filePathWChars[cwdLen + 1] = L'\0';
				baseW = filePathWChars.GetBuffer();
			}
			break;

		case FileLocation::kAbsolute:
			baseW = L"";
			break;

		case FileLocation::kDataSourceDirectory:
			{
				const wchar_t *dataSrcDir = L"datasrc";
				const size_t dataSrcDirLen = wcslen(dataSrcDir);

				DWORD cwdBufferSize = GetCurrentDirectoryW(0, nullptr);
				if (cwdBufferSize == 0)
					return ResultCode::kIOError;

				RKIT_CHECK(filePathWChars.Resize(cwdBufferSize + 2 + dataSrcDirLen));

				DWORD cwdLen = GetCurrentDirectoryW(cwdBufferSize, filePathWChars.GetBuffer());
				if (cwdLen != cwdBufferSize - 1)
					return ResultCode::kIOError;

				filePathWChars[cwdLen] = L'\\';
				filePathWChars[cwdLen + 1] = L'\0';

				wcscat(filePathWChars.GetBuffer(), dataSrcDir);
				wcscat(filePathWChars.GetBuffer(), L"\\");

				baseW = filePathWChars.GetBuffer();
			}
			break;

		case FileLocation::kGameDirectory:
			{
				if (m_gameDirectoryOverride.Length() == 0)
					return ResultCode::kInvalidParameter;

				baseW = m_gameDirectoryOverride.CStr();
			}
			break;

		case FileLocation::kUserSettingsDirectory:
			{
				if (m_settingsDirectory.Length() == 0)
					return ResultCode::kInvalidParameter;

				baseW = m_settingsDirectory.CStr();
			}
			break;

		default:
			return ResultCode::kInvalidParameter;
		}

		size_t baseLen = wcslen(baseW);
		size_t pathLen = pathW.Count() - 1;

		size_t fullPathBufferSize = 1;	// 1 for null terminator

		RKIT_CHECK(SafeAdd(fullPathBufferSize, fullPathBufferSize, baseLen));
		RKIT_CHECK(SafeAdd(fullPathBufferSize, fullPathBufferSize, pathLen));

		Vector<wchar_t> fullPathW;
		RKIT_CHECK(fullPathW.Resize(fullPathBufferSize));

		fullPathW[0] = L'\0';

		wcscpy(fullPathW.GetBuffer(), baseW);
		wcscat(fullPathW.GetBuffer(), pathW.GetBuffer());

		for (wchar_t &wc : fullPathW)
		{
			if (wc == '/')
				wc = '\\';
		}

		for (size_t i = 1; i < fullPathW.Count(); i++)
		{
			if (fullPathW[i] == '\\' && fullPathW[i - 1] == '\\')
				return ResultCode::kFileOpenError;
		}

		outPath = std::move(fullPathW);

		return ResultCode::kOK;
	}

	Result SystemDriver_Win32::OpenFileGeneralChecked(UniquePtr<File_Win32> &outFile, FileLocation location, const char *path, bool createDirectories, DWORD access, DWORD shareMode, DWORD disposition)
	{
		Vector<wchar_t> fullPathW;

		RKIT_CHECK(ResolveFilePath(location, path, fullPathW));

		if (createDirectories)
		{
			RKIT_CHECK(CheckCreateDirectories(fullPathW));
		}

		HANDLE fHandle = CreateFileW(fullPathW.GetBuffer(), access, shareMode, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (fHandle == INVALID_HANDLE_VALUE)
			return Result::SoftFault(ResultCode::kFileOpenError);

		LARGE_INTEGER fileSize;
		if (!GetFileSizeEx(fHandle, &fileSize))
		{
			CloseHandle(fHandle);
			return ResultCode::kFileOpenError;
		}

		Result createObjResult = New<File_Win32>(outFile, fHandle, static_cast<FilePos_t>(fileSize.QuadPart));
		if (!createObjResult.IsOK())
		{
			CloseHandle(fHandle);
			return ResultCode::kFileOpenError;
		}

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
