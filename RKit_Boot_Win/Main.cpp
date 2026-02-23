#include "rkit/Core/Drivers.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/MallocDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ProgramDriver.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Vector.h"

#include "rkit/Mem/MemMapDriver.h"
#include "rkit/Mem/MemModule.h"
#include "rkit/Mem/SimpleMemMapMallocDriver.h"

#include "rkit/Win32/SystemModuleInitParameters_Win32.h"
#include "rkit/Win32/ModuleAPI_Win32.h"
#include "rkit/Win32/IncludeWindows.h"

#include <cstdlib>
#include <clocale>
#include <new>

namespace rkit
{
	struct ModuleDriver_Win32 final : public IModuleDriver
	{
		IModule *LoadModuleInternal(uint32_t moduleNamespace, const Utf8Char_t *moduleName, const ModuleInitParameters *initParams, IMallocDriver &mallocDriver) override;
	};

	struct ConsoleLogDriver_Win32 final : public ILogDriver
	{
		void LogMessage(LogSeverity severity, const rkit::StringSliceView &msg) override;
		void VLogMessage(LogSeverity severity, const rkit::StringSliceView &fmt, const FormatParameterList<Utf8Char_t> &args) override;

		static void FormatMessageToFile(FILE *f, const rkit::StringSliceView &fmt, const FormatParameterList<Utf8Char_t> &args);

	private:
		class FileOutputFormatter final : public IFormatStringWriter<Utf8Char_t>
		{
		public:
			explicit FileOutputFormatter(FILE *f);

			void WriteChars(const ConstSpan<Utf8Char_t> &chars) override;
			CharacterEncoding GetEncoding() const override;

		private:
			FILE *m_f;
		};
	};

	class MemMapDriver_Win32 final : public mem::IMemMapDriver
	{
	public:
		MemMapDriver_Win32();

		size_t GetNumPageTypes() const override;
		const mem::MemMapPageTypeProperties &GetPageType(size_t pageTypeIndex) const override;

		bool CreateMapping(mem::MemMapPageRange &outPageRange, size_t size, size_t pageTypeIndex, mem::MemMapState initialState) override;
		bool CreateMappingAt(mem::MemMapPageRange &outPageRange, void *baseAddress, size_t size, size_t pageTypeIndex, mem::MemMapState initialState) override;
		bool ChangeState(void *startAddress, size_t size, size_t pageTypeIndex, mem::MemMapState oldState, mem::MemMapState newState) override;
		void ReleaseMapping(const mem::MemMapPageRange &pageRange) override;

	private:
		mem::MemMapPageTypeProperties m_defaultPageProperties;

		bool InternalCreateMappingAt(mem::MemMapPageRange &outPageRange, void *baseAddress, size_t size, size_t pageTypeIndex, mem::MemMapState initialState) const;
	};

	class Module_Win32 final : public IModule
	{
	public:
		Module_Win32(HMODULE hmodule, FARPROC initFunc, IMallocDriver *mallocDriver);
		~Module_Win32();

		Result Init(const ModuleInitParameters *initParams) override;
		void Unload() override;

	private:
		HMODULE m_hmodule;
		FARPROC m_initFunc;
		IMallocDriver *m_mallocDriver;
		bool m_isInitialized;

		ModuleAPI_Win32 m_moduleAPI;
	};

	struct Win32Globals
	{
		rkit::Drivers m_drivers;
		rkit::ModuleDriver_Win32 m_moduleDriver;
		rkit::ConsoleLogDriver_Win32 m_consoleLogDriver;
		rkit::MemMapDriver_Win32 m_memMapDriver;
	};

	struct alignas(alignof(Win32Globals)) Win32GlobalsBuffer
	{
		uint8_t m_bytes[sizeof(Win32Globals)];
	};

	static Win32GlobalsBuffer g_winGlobalsBuffer;
	static Win32Globals &g_winGlobals = *reinterpret_cast<Win32Globals *>(g_winGlobalsBuffer.m_bytes);

	IModule *ModuleDriver_Win32::LoadModuleInternal(uint32_t moduleNamespace, const Utf8Char_t *moduleName, const ModuleInitParameters *initParams, IMallocDriver &mallocDriver)
	{
		// FIXME
		const char *moduleNameAscii = reinterpret_cast<const char *>(moduleName);

		// Base module driver does no deduplication
		char *nameBuf = static_cast<char *>(mallocDriver.Alloc(strlen(moduleNameAscii) + strlen("????_.dll") + 1));
		if (!nameBuf)
			return nullptr;

		strcpy(nameBuf, "RKit_");
		strcat(nameBuf, moduleNameAscii);
		strcat(nameBuf, ".dll");

		rkit::utils::ExtractFourCC(moduleNamespace, nameBuf[0], nameBuf[1], nameBuf[2], nameBuf[3]);

		void *moduleMemory = mallocDriver.Alloc(sizeof(Module_Win32));
		if (!moduleMemory)
		{
			mallocDriver.Free(nameBuf);
			return nullptr;
		}

		HMODULE hmodule = LoadLibraryA(nameBuf);
		mallocDriver.Free(nameBuf);

		if (hmodule == nullptr)
		{
			mallocDriver.Free(moduleMemory);
			return nullptr;
		}

		FARPROC initFunc = GetProcAddress(hmodule, "InitializeRKitModule");
		if (initFunc == nullptr)
		{
			FreeLibrary(hmodule);
			mallocDriver.Free(moduleMemory);
			return nullptr;
		}

		IModule *module = new (moduleMemory) Module_Win32(hmodule, initFunc, &mallocDriver);

		PackedResultAndExtCode initResult = RKIT_TRY_EVAL(module->Init(initParams));
		if (!utils::ResultIsOK(initResult))
		{
			module->Unload();
			return nullptr;
		}

		return module;
	}

	void ConsoleLogDriver_Win32::LogMessage(LogSeverity severity, const StringSliceView &msg)
	{
		if (severity == LogSeverity::kInfo)
		{
			fwrite(msg.GetChars(), 1, msg.Length(), stdout);
			fputc('\n', stdout);
		}
		else if (severity == LogSeverity::kWarning)
		{
			fputs("WARNING: ", stderr);
			fwrite(msg.GetChars(), 1, msg.Length(), stdout);
			fputc('\n', stderr);
		}
		else if (severity == LogSeverity::kError)
		{
			fputs("ERROR: ", stderr);
			fwrite(msg.GetChars(), 1, msg.Length(), stdout);
			fputc('\n', stderr);
		}
	}

	void ConsoleLogDriver_Win32::VLogMessage(LogSeverity severity, const StringSliceView &fmt, const FormatParameterList<Utf8Char_t> &args)
	{
		if (severity == LogSeverity::kInfo)
		{
			FormatMessageToFile(stdout, fmt, args);
			fputc('\n', stdout);
		}
		else if (severity == LogSeverity::kWarning)
		{
			fputs("WARNING: ", stderr);
			FormatMessageToFile(stdout, fmt, args);
			fputc('\n', stderr);
		}
		else if (severity == LogSeverity::kError)
		{
			fputs("ERROR: ", stderr);
			FormatMessageToFile(stderr, fmt, args);
			fputc('\n', stderr);
		}
	}


	void ConsoleLogDriver_Win32::FormatMessageToFile(FILE *f, const rkit::StringSliceView &fmt, const FormatParameterList<Utf8Char_t> &args)
	{
		FileOutputFormatter outputFormatter(f);

		GetDrivers().m_utilitiesDriver->FormatString(outputFormatter, fmt, args);
	}


	ConsoleLogDriver_Win32::FileOutputFormatter::FileOutputFormatter(FILE *f)
		: m_f(f)
	{
	}

	void ConsoleLogDriver_Win32::FileOutputFormatter::WriteChars(const ConstSpan<Utf8Char_t> &chars)
	{
		fwrite(chars.Ptr(), 1, chars.Count(), m_f);
	}

	CharacterEncoding ConsoleLogDriver_Win32::FileOutputFormatter::GetEncoding() const
	{
		return CharacterEncoding::kUTF8;
	}


	MemMapDriver_Win32::MemMapDriver_Win32()
		: m_defaultPageProperties{}
	{
		m_defaultPageProperties.m_supportsState[static_cast<size_t>(mem::MemMapState::kRead)] = true;
		m_defaultPageProperties.m_supportsState[static_cast<size_t>(mem::MemMapState::kNoAccess)] = true;
		m_defaultPageProperties.m_supportsState[static_cast<size_t>(mem::MemMapState::kReadWrite)] = true;
		m_defaultPageProperties.m_supportsState[static_cast<size_t>(mem::MemMapState::kReserved)] = true;

		SYSTEM_INFO sysInfo;
		::GetSystemInfo(&sysInfo);

		m_defaultPageProperties.m_allocationGranularityPO2 = rkit::FindHighestSetBit(sysInfo.dwAllocationGranularity);
		m_defaultPageProperties.m_pageSizePO2 = rkit::FindHighestSetBit(sysInfo.dwPageSize);
	}

	size_t MemMapDriver_Win32::GetNumPageTypes() const
	{
		return 1;
	}

	const mem::MemMapPageTypeProperties &MemMapDriver_Win32::GetPageType(size_t pageTypeIndex) const
	{
		RKIT_ASSERT(pageTypeIndex == 0);
		return m_defaultPageProperties;
	}

	bool MemMapDriver_Win32::CreateMapping(mem::MemMapPageRange &outPageRange, size_t size, size_t pageTypeIndex, mem::MemMapState initialState)
	{
		return InternalCreateMappingAt(outPageRange, nullptr, size, pageTypeIndex, initialState);
	}

	bool MemMapDriver_Win32::CreateMappingAt(mem::MemMapPageRange &outPageRange, void *baseAddress, size_t size, size_t pageTypeIndex, mem::MemMapState initialState)
	{
		return InternalCreateMappingAt(outPageRange, baseAddress, size, pageTypeIndex, initialState);
	}

	bool MemMapDriver_Win32::InternalCreateMappingAt(mem::MemMapPageRange &outPageRange, void *baseAddress, size_t size, size_t pageTypeIndex, mem::MemMapState initialState) const
	{
		if (pageTypeIndex != 0)
			return false;

		const size_t allocGranularity = static_cast<size_t>(1) << m_defaultPageProperties.m_allocationGranularityPO2;
		const size_t allocMask = allocGranularity - 1;

		const size_t sizeGranularity = static_cast<size_t>(1) << m_defaultPageProperties.m_pageSizePO2;
		const size_t sizeMask = sizeGranularity - 1;

		if ((reinterpret_cast<uintptr_t>(baseAddress) & allocMask) != 0)
			return false;

		if ((size & sizeMask) != 0)
			return false;

		DWORD allocType = (MEM_COMMIT | MEM_RESERVE);
		DWORD initialProtect = 0;

		switch (initialState)
		{
		case mem::MemMapState::kReserved:
			allocType = MEM_RESERVE;
			break;
		case mem::MemMapState::kRead:
			initialProtect = PAGE_READONLY;
			break;
		case mem::MemMapState::kReadWrite:
			initialProtect = PAGE_READWRITE;
			break;
		case mem::MemMapState::kNoAccess:
			initialProtect = PAGE_NOACCESS;
			break;
		default:
			return false;
		}

		LPVOID addr = VirtualAlloc(baseAddress, size, allocType, initialProtect);
		if (!addr)
			return false;

		outPageRange.m_base = addr;
		outPageRange.m_size = size;
		return true;
	}


	bool MemMapDriver_Win32::ChangeState(void *startAddress, size_t size, size_t pageTypeIndex, mem::MemMapState oldState, mem::MemMapState newState)
	{
		if (oldState == newState)
			return true;

		DWORD newProtect = 0;
		switch (newState)
		{
		case mem::MemMapState::kReserved:
			return !!VirtualFree(startAddress, size, MEM_DECOMMIT);
		case mem::MemMapState::kRead:
			newProtect = PAGE_READONLY;
			break;
		case mem::MemMapState::kReadWrite:
			newProtect = PAGE_READWRITE;
			break;
		case mem::MemMapState::kNoAccess:
			newProtect = PAGE_NOACCESS;
			break;
		default:
			return false;
		}

		if (oldState == mem::MemMapState::kReserved)
			return !!VirtualAlloc(startAddress, size, MEM_COMMIT, newProtect);
		else
		{
			DWORD oldProtect = 0;
			return !!VirtualProtect(startAddress, size, newProtect, &oldProtect);
		}
	}

	void MemMapDriver_Win32::ReleaseMapping(const mem::MemMapPageRange &pageRange)
	{
		BOOL succeeded = VirtualFree(pageRange.m_base, 0, MEM_RELEASE);
		RKIT_ASSERT(succeeded);
		(void)succeeded;
	}

	Module_Win32::Module_Win32(HMODULE hmodule, FARPROC initFunc, IMallocDriver *mallocDriver)
		: m_hmodule(hmodule)
		, m_initFunc(initFunc)
		, m_mallocDriver(mallocDriver)
		, m_isInitialized(false)
		, m_moduleAPI{}
	{
	}

	Module_Win32::~Module_Win32()
	{
		if (m_isInitialized)
			m_moduleAPI.m_shutdownFunction();

		::FreeLibrary(m_hmodule);
	}

	Result Module_Win32::Init(const ModuleInitParameters *initParams)
	{
		typedef void (*initProc_t)(void *);

		m_moduleAPI.m_drivers = &g_winGlobals.m_drivers;
		m_moduleAPI.m_initFunction = nullptr;
		m_moduleAPI.m_shutdownFunction = nullptr;

		initProc_t initProc = reinterpret_cast<initProc_t>(m_initFunc);

		initProc(&m_moduleAPI);

		m_isInitialized = true;

		if (m_moduleAPI.m_initFunction)
		{
			RKIT_CHECK(m_moduleAPI.m_initFunction(initParams));
		}

		RKIT_RETURN_OK;
	}

	void Module_Win32::Unload()
	{
		IMallocDriver *mallocDriver = m_mallocDriver;
		void *thisMem = this;

		this->~Module_Win32();
		mallocDriver->Free(thisMem);
	}

	const Drivers &rkit::GetDrivers()
	{
		return g_winGlobals.m_drivers;
	}
}

namespace rkit { namespace boot { namespace win32
{
	int __declspec(dllexport) MainCommon(HINSTANCE hInstance)
	{
		setlocale(LC_ALL, "C");
		::SetConsoleOutputCP(CP_UTF8);

		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

		memset(rkit::g_winGlobalsBuffer.m_bytes, 0, sizeof(rkit::g_winGlobalsBuffer.m_bytes));
		new (rkit::g_winGlobalsBuffer.m_bytes) rkit::Win32Globals();

		rkit::Drivers *drivers = &rkit::g_winGlobals.m_drivers;

		rkit::mem::SimpleMemMapMallocDriver bootMallocDriver(rkit::g_winGlobals.m_memMapDriver);

		drivers->m_mallocDriver.m_obj = &bootMallocDriver;
		drivers->m_moduleDriver.m_obj = &rkit::g_winGlobals.m_moduleDriver;

		rkit::mem::MemModuleInitParameters memModuleParams = {};
		memModuleParams.m_mmapDriver = &rkit::g_winGlobals.m_memMapDriver;

		rkit::IModule *memModule = drivers->m_moduleDriver->LoadModule(::rkit::IModuleDriver::kDefaultNamespace, u8"Mem", &memModuleParams);
		if (!memModule)
		{
			return rkit::utils::ResultToExitCode(rkit::utils::PackResult(rkit::ResultCode::kModuleLoadFailed));
		}

#if RKIT_IS_DEBUG
		drivers->m_logDriver.m_obj = &rkit::g_winGlobals.m_consoleLogDriver;
#endif

		rkit::Utf16String modulePathStr;
		rkit::Utf16String moduleDirStr;

		{
			DWORD requiredSize = 16;

			for (;;)
			{
				rkit::Vector<rkit::Utf16Char_t> moduleFileNameChars;

				{
					rkit::PackedResultAndExtCode resizeResult = RKIT_TRY_EVAL(moduleFileNameChars.Resize(requiredSize));
					if (!rkit::utils::ResultIsOK(resizeResult))
						return rkit::utils::ResultToExitCode(resizeResult);
				}

				rkit::Utf16StringConstructionBuffer cbuf;

				DWORD moduleStrSize = GetModuleFileNameW(nullptr, reinterpret_cast<wchar_t *>(moduleFileNameChars.GetBuffer()), requiredSize);

				if (moduleStrSize == requiredSize)
				{
					if (requiredSize >= 0x1000000)
						return rkit::utils::ResultToExitCode(rkit::utils::PackResult(rkit::ResultCode::kOutOfMemory));

					requiredSize *= 2;
					continue;
				}

				rkit::PackedResultAndExtCode setResult = RKIT_TRY_EVAL(modulePathStr.Set(moduleFileNameChars.ToSpan().SubSpan(0, moduleStrSize)));
				if (!rkit::utils::ResultIsOK(setResult))
					return rkit::utils::ResultToExitCode(setResult);

				DWORD dirEnd = moduleStrSize;
				DWORD dirEndScan = dirEnd;
				while (dirEndScan > 0 && moduleFileNameChars[dirEndScan] != '\\')
					dirEndScan--;


				setResult = RKIT_TRY_EVAL(moduleDirStr.Set(moduleFileNameChars.ToSpan().SubSpan(0, dirEndScan)));
				if (!rkit::utils::ResultIsOK(setResult))
					return rkit::utils::ResultToExitCode(setResult);

				break;
			}
		}

		rkit::IModule *systemModule = nullptr;

		{
			rkit::SystemModuleInitParameters_Win32 systemParams(hInstance, std::move(modulePathStr), std::move(moduleDirStr));

			modulePathStr.Clear();
			moduleDirStr.Clear();

			systemModule = ::rkit::g_winGlobals.m_moduleDriver.LoadModule(::rkit::IModuleDriver::kDefaultNamespace, u8"System_Win32", &systemParams);
			if (!systemModule)
				return rkit::utils::ResultToExitCode(rkit::utils::PackResult(rkit::ResultCode::kModuleLoadFailed));
		}

		rkit::IModule *programLauncherModule = ::rkit::g_winGlobals.m_moduleDriver.LoadModule(::rkit::IModuleDriver::kDefaultNamespace, u8"ProgramLauncher");
		if (!programLauncherModule)
		{
			systemModule->Unload();
			return rkit::utils::ResultToExitCode(rkit::utils::PackResult(rkit::ResultCode::kModuleLoadFailed));
		}

		rkit::PackedResultAndExtCode result = RKIT_TRY_EVAL(drivers->m_programDriver->InitProgram());
		if (rkit::utils::ResultIsOK(result))
		{
			for (;;)
			{
				bool isExiting = false;
				result = RKIT_TRY_EVAL(drivers->m_programDriver->RunFrame(isExiting));

				if (!rkit::utils::ResultIsOK(result) || isExiting)
					break;
			}
		}

		drivers->m_programDriver->ShutdownProgram();

		programLauncherModule->Unload();
		systemModule->Unload();

		_CrtCheckMemory();
		_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
		_CrtDumpMemoryLeaks();

		memModule->Unload();

		rkit::g_winGlobals.~Win32Globals();

		return 0;
	}
} } }

RKIT_IMPLEMENT_PER_MODULE_FUNCTIONS
