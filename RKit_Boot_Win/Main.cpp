#include "rkit/Core/Drivers.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/MallocDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ProgramDriver.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Vector.h"

#include "rkit/Win32/SystemModuleInitParameters_Win32.h"
#include "rkit/Win32/ModuleAPI_Win32.h"
#include "rkit/Win32/IncludeWindows.h"

#include <cstdlib>
#include <clocale>
#include <new>

#include <crtdbg.h>

namespace rkit
{
	struct MallocDriver_Win32 final : public IMallocDriver
	{
		void *Alloc(size_t size) override;
		void *Realloc(void *ptr, size_t size) override;
		void Free(void *ptr) override;
		void CheckIntegrity() override;
	};

	struct ModuleDriver_Win32 final : public IModuleDriver
	{
		IModule *LoadModule(uint32_t moduleNamespace, const char *moduleName, const ModuleInitParameters *initParams) override;
	};

	struct ConsoleLogDriver_Win32 final : public ILogDriver
	{
		void LogMessage(LogSeverity severity, const char *msg) override;
		void VLogMessage(LogSeverity severity, const char *fmt, va_list arg) override;
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
		rkit::MallocDriver_Win32 m_mallocDriver;
		rkit::ConsoleLogDriver_Win32 m_consoleLogDriver;
	};

	struct alignas(alignof(Win32Globals)) Win32GlobalsBuffer
	{
		uint8_t m_bytes[sizeof(Win32Globals)];
	};

	static Win32GlobalsBuffer g_winGlobalsBuffer;
	static Win32Globals &g_winGlobals = *reinterpret_cast<Win32Globals *>(g_winGlobalsBuffer.m_bytes);

	void *MallocDriver_Win32::Alloc(size_t size)
	{
		if (size == 0)
			return nullptr;

		void *ptr = malloc(size);

		return ptr;
	}

	void *MallocDriver_Win32::Realloc(void *ptr, size_t size)
	{
		if (ptr == nullptr)
		{
			if (size == 0)
				return nullptr;

			return malloc(size);
		}
		else
		{
			if (size == 0)
			{
				free(ptr);
				return nullptr;
			}
			else
				return realloc(ptr, size);
		}
	}

	void MallocDriver_Win32::Free(void *ptr)
	{
		free(ptr);
	}

	void MallocDriver_Win32::CheckIntegrity()
	{
		_ASSERTE(_CrtCheckMemory());
	}

	IModule *ModuleDriver_Win32::LoadModule(uint32_t moduleNamespace, const char *moduleName, const ModuleInitParameters *initParams)
	{
		// Base module driver does no deduplication
		IMallocDriver *mallocDriver = g_winGlobals.m_drivers.m_mallocDriver;

		char *nameBuf = static_cast<char *>(mallocDriver->Alloc(strlen(moduleName) + strlen("????_.dll") + 1));
		if (!nameBuf)
			return nullptr;

		strcpy(nameBuf, "RKit_");
		strcat(nameBuf, moduleName);
		strcat(nameBuf, ".dll");

		rkit::utils::ExtractFourCC(moduleNamespace, nameBuf[0], nameBuf[1], nameBuf[2], nameBuf[3]);

		void *moduleMemory = mallocDriver->Alloc(sizeof(Module_Win32));
		if (!moduleMemory)
		{
			mallocDriver->Free(nameBuf);
			return nullptr;
		}

		HMODULE hmodule = LoadLibraryA(nameBuf);
		mallocDriver->Free(nameBuf);

		if (hmodule == nullptr)
		{
			mallocDriver->Free(moduleMemory);
			return nullptr;
		}

		FARPROC initFunc = GetProcAddress(hmodule, "InitializeRKitModule");
		if (initFunc == nullptr)
		{
			FreeLibrary(hmodule);
			mallocDriver->Free(moduleMemory);
			return nullptr;
		}

		IModule* module = new (moduleMemory) Module_Win32(hmodule, initFunc, mallocDriver);

		Result initResult = module->Init(initParams);
		if (!utils::ResultIsOK(initResult))
		{
			module->Unload();
			return nullptr;
		}

		return module;
	}

	void ConsoleLogDriver_Win32::LogMessage(LogSeverity severity, const char *msg)
	{
		if (severity == LogSeverity::kInfo)
		{
			fputs(msg, stdout);
			fputc('\n', stdout);
		}
		else if (severity == LogSeverity::kWarning)
		{
			fputs("WARNING: ", stderr);
			fputs(msg, stdout);
			fputc('\n', stderr);
		}
		else if (severity == LogSeverity::kError)
		{
			fputs("ERROR: ", stderr);
			fputs(msg, stdout);
			fputc('\n', stderr);
		}

	}

	void ConsoleLogDriver_Win32::VLogMessage(LogSeverity severity, const char *fmt, va_list arg)
	{
		va_list argCopy;
		va_copy(argCopy, arg);

		if (severity == LogSeverity::kInfo)
		{
			vfprintf(stdout, fmt, argCopy);
			fputc('\n', stdout);
		}
		else if (severity == LogSeverity::kWarning)
		{
			fputs("WARNING: ", stderr);
			vfprintf(stderr, fmt, argCopy);
			fputc('\n', stderr);
		}
		else if (severity == LogSeverity::kError)
		{
			fputs("ERROR: ", stderr);
			vfprintf(stderr, fmt, argCopy);
			fputc('\n', stderr);
		}
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

		FreeLibrary(m_hmodule);
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

		return ResultCode::kOK;
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

static int WinMainCommon(HINSTANCE hInstance)
{
	setlocale(LC_ALL, "C");

	new (rkit::g_winGlobalsBuffer.m_bytes) rkit::Win32Globals();

	rkit::Drivers *drivers = &rkit::g_winGlobals.m_drivers;

	drivers->m_mallocDriver.m_obj = &rkit::g_winGlobals.m_mallocDriver;
	drivers->m_moduleDriver.m_obj = &rkit::g_winGlobals.m_moduleDriver;

#if RKIT_IS_DEBUG
	drivers->m_logDriver.m_obj = &rkit::g_winGlobals.m_consoleLogDriver;
#endif

	rkit::WString16 modulePathStr;
	rkit::WString16 moduleDirStr;

	{
		DWORD requiredSize = 16;

		for (;;)
		{
			rkit::Vector<wchar_t> moduleFileNameChars;

			rkit::Result resizeResult = moduleFileNameChars.Resize(requiredSize);
			if (!rkit::utils::ResultIsOK(resizeResult))
				return rkit::utils::ResultToExitCode(resizeResult);

			rkit::WStringConstructionBuffer cbuf;

			DWORD moduleStrSize = GetModuleFileNameW(nullptr, moduleFileNameChars.GetBuffer(), requiredSize);

			if (moduleStrSize == requiredSize)
			{
				if (requiredSize >= 0x1000000)
					return rkit::utils::ResultToExitCode(rkit::Result(rkit::ResultCode::kOutOfMemory));

				requiredSize *= 2;
				continue;
			}

			rkit::Result setResult = modulePathStr.Set(moduleFileNameChars.ToSpan().SubSpan(0, moduleStrSize));
			if (!rkit::utils::ResultIsOK(setResult))
				return rkit::utils::ResultToExitCode(setResult);

			DWORD dirEnd = moduleStrSize;
			DWORD dirEndScan = dirEnd;
			while (dirEndScan > 0 && moduleFileNameChars[dirEndScan] != '\\')
				dirEndScan--;


			setResult = moduleDirStr.Set(moduleFileNameChars.ToSpan().SubSpan(0, dirEndScan));
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

		systemModule = ::rkit::g_winGlobals.m_moduleDriver.LoadModule(::rkit::IModuleDriver::kDefaultNamespace, "System_Win32", &systemParams);
		if (!systemModule)
			return rkit::utils::ResultToExitCode(rkit::Result(rkit::ResultCode::kModuleLoadFailed));
	}

	rkit::IModule *programLauncherModule = ::rkit::g_winGlobals.m_moduleDriver.LoadModule(::rkit::IModuleDriver::kDefaultNamespace, "ProgramLauncher", nullptr);
	if (!programLauncherModule)
	{
		systemModule->Unload();
		return rkit::utils::ResultToExitCode(rkit::Result(rkit::ResultCode::kModuleLoadFailed));
	}

	rkit::Result result = drivers->m_programDriver->InitProgram();
	if (rkit::utils::ResultIsOK(result))
	{
		for (;;)
		{
			bool isExiting = false;
			result = drivers->m_programDriver->RunFrame(isExiting);

			if (!rkit::utils::ResultIsOK(result) || isExiting)
				break;
		}
	}

	drivers->m_programDriver->ShutdownProgram();

	programLauncherModule->Unload();
	systemModule->Unload();

	rkit::g_winGlobals.~Win32Globals();

	return rkit::utils::ResultToExitCode(result);
}



#ifdef _CONSOLE
int main(int argc, const char **argv)
{
	return WinMainCommon(GetModuleHandleW(nullptr));
}
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	return WinMainCommon(hInstance);
}
#endif

RKIT_IMPLEMENT_PER_MODULE_FUNCTIONS
