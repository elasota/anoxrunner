#include "rkit/Core/Algorithm.h"
#include "rkit/Core/Drivers.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/MallocDriver.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Span.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/String.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"

#include "rkit/Win32/Win32PlatformDriver.h"
#include "rkit/Win32/SystemModuleInitParameters_Win32.h"

#include <shellapi.h>
#include <Shlwapi.h>


namespace rkit
{
	class File_Win32 final : public ISeekableReadWriteStream
	{
	public:
		File_Win32(HANDLE hfile, FilePos_t initialSize);
		~File_Win32();

		Result WritePartial(const void *data, size_t count, size_t &outCountWritten) override;
		Result ReadPartial(void *data, size_t count, size_t &outCountRead) override;

		Result SeekStart(FilePos_t pos) override;
		Result SeekCurrent(FileOffset_t pos) override;
		Result SeekEnd(FileOffset_t pos) override;
		FilePos_t Tell() const override;
		FilePos_t GetSize() const override;

	private:
		HANDLE m_hfile;
		FilePos_t m_filePos;
		FilePos_t m_fileSize;
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

		UniquePtr<ISeekableReadStream> OpenFileRead(FileLocation location, const char *path) override;
		UniquePtr<ISeekableWriteStream> OpenFileWrite(FileLocation location, const char *path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) override;
		UniquePtr<ISeekableReadWriteStream> OpenFileReadWrite(FileLocation location, const char *path, bool createIfNotExists, bool createDirectories, bool truncateIfExists) override;

		char GetPathSeparator() const override;

		IPlatformDriver *GetPlatformDriver() const override;

		HINSTANCE GetHInstance() const override;

	private:
		Result UTF8ToUTF16(const char *str8, Vector<wchar_t> &outStr16);
		Result UTF16ToUTF8(const wchar_t *str16, Vector<char> &outStr16);

		static DWORD OpenFlagsToDisposition(bool createIfNotExists, bool truncateIfExists);
		UniquePtr<File_Win32> OpenFileGeneral(FileLocation location, const char *path, bool createDirectories, DWORD access, DWORD shareMode, DWORD disposition);
		Result OpenFileGeneralChecked(UniquePtr<File_Win32> &outFile, FileLocation location, const char *path, bool createDirectories, DWORD access, DWORD shareMode, DWORD disposition);
		static Result CheckCreateDirectories(Vector<wchar_t> &pathChars);

		IMallocDriver *m_alloc;
		Vector<Vector<char> > m_commandLineCharBuffers;
		Vector<StringView> m_commandLine;
		LPWSTR *m_argvW;

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
			if (m_fileSize > m_filePos)
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

	SystemDriver_Win32::SystemDriver_Win32(IMallocDriver *alloc, const SystemModuleInitParameters_Win32 &initParams)
		: m_commandLine(alloc)
		, m_argvW(nullptr)
		, m_alloc(alloc)
		, m_hInstance(initParams.m_hinst)
	{
	}

	SystemDriver_Win32::~SystemDriver_Win32()
	{
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

			RKIT_CHECK(UTF16ToUTF8(m_argvW[i], charBuffer));

			m_commandLine[i] = StringView(charBuffer.GetBuffer(), charBuffer.Count() - 1);
		}

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

		Result exprConvResult = UTF8ToUTF16(expr, exprWChar);
		Result fileConvResult = UTF8ToUTF16(file, fileWChar);

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
		return OpenFileGeneral(location, path, createDirectories, GENERIC_WRITE, 0, OpenFlagsToDisposition(createIfNotExists, truncateIfExists));
	}

	UniquePtr<ISeekableReadWriteStream> SystemDriver_Win32::OpenFileReadWrite(FileLocation location, const char *path, bool createIfNotExists, bool createDirectories, bool truncateIfExists)
	{
		return OpenFileGeneral(location, path, createDirectories, GENERIC_READ | GENERIC_WRITE, 0, OpenFlagsToDisposition(createIfNotExists, truncateIfExists));
	}

	char SystemDriver_Win32::GetPathSeparator() const
	{
		return '\\';
	}

	IPlatformDriver *SystemDriver_Win32::GetPlatformDriver() const
	{
		return const_cast<SystemDriver_Win32 *>(this);
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

	Result SystemDriver_Win32::OpenFileGeneralChecked(UniquePtr<File_Win32> &outFile, FileLocation location, const char *path, bool createDirectories, DWORD access, DWORD shareMode, DWORD disposition)
	{
		Vector<wchar_t> pathW;

		RKIT_CHECK(UTF8ToUTF16(path, pathW));

		Vector<wchar_t> filePathWChars;
		const wchar_t *baseW = nullptr;	// Base path, must end with dir separator

		switch (location)
		{
		case FileLocation::kGameDirectory:
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

		if (createDirectories)
		{
			RKIT_CHECK(CheckCreateDirectories(fullPathW));
		}

		HANDLE fHandle = CreateFileW(fullPathW.GetBuffer(), access, shareMode, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (fHandle == INVALID_HANDLE_VALUE)
			return ResultCode::kFileOpenError;

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

	Result SystemDriver_Win32::UTF8ToUTF16(const char *str8, Vector<wchar_t> &outStr16)
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

	Result SystemDriver_Win32::UTF16ToUTF8(const wchar_t *str16, Vector<char> &outStr8)
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
