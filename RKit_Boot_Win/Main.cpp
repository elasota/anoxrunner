#include "rkit/Core/Drivers.h"
#include "rkit/Core/MallocDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleAPI_Win32.h"
#include "rkit/Core/ProgramDriver.h"
#include "rkit/Core/SystemModuleParams_Win32.h"

#include <Windows.h>
#include <cstdlib>
#include <new>

namespace rkit
{
	struct MallocDriver_Win32 final : public IMallocDriver
	{
		void *Alloc(size_t size) override;
		void *Realloc(void *ptr, size_t size) override;
		void Free(void *ptr) override;
	};

	struct ModuleDriver_Win32 final : public IModuleDriver
	{
		IModule *LoadModule(uint32_t moduleNamespace, const char *moduleName) override;
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

	static rkit::Drivers g_drivers_Win32;
	static rkit::ModuleDriver_Win32 g_moduleDriver;
	static rkit::MallocDriver_Win32 g_mallocDriver;

	void *MallocDriver_Win32::Alloc(size_t size)
	{
		if (size == 0)
			return nullptr;

		return malloc(size);
	}

	void *MallocDriver_Win32::Realloc(void *ptr, size_t size)
	{
		if (ptr == nullptr && size == 0)
			return nullptr;

		return realloc(ptr, size);
	}

	void MallocDriver_Win32::Free(void *ptr)
	{
		free(ptr);
	}

	IModule *ModuleDriver_Win32::LoadModule(uint32_t moduleNamespace, const char *moduleName)
	{
		IMallocDriver *mallocDriver = g_drivers_Win32.m_mallocDriver;

		char *nameBuf = static_cast<char *>(mallocDriver->Alloc(strlen(moduleName) + strlen("????_.dll") + 1));
		if (!nameBuf)
			return nullptr;

		strcpy(nameBuf, "RKit_");
		strcat(nameBuf, moduleName);
		strcat(nameBuf, ".dll");

		for (int i = 0; i < 4; i++)
			nameBuf[i] = static_cast<char>((moduleNamespace >> (i * 8)) & 0xff);

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

		return new (moduleMemory) Module_Win32(hmodule, initFunc, mallocDriver);
	}

	Module_Win32::Module_Win32(HMODULE hmodule, FARPROC initFunc, IMallocDriver *mallocDriver)
		: m_hmodule(hmodule)
		, m_initFunc(initFunc)
		, m_mallocDriver(mallocDriver)
		, m_isInitialized(false)
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

		m_moduleAPI.m_drivers = &g_drivers_Win32;
		m_moduleAPI.m_initFunction = nullptr;
		m_moduleAPI.m_shutdownFunction = nullptr;

		initProc_t initProc = reinterpret_cast<initProc_t>(m_initFunc);

		initProc(&m_moduleAPI);

		m_isInitialized = true;

		return m_moduleAPI.m_initFunction(initParams);
	}

	void Module_Win32::Unload()
	{
		IMallocDriver *mallocDriver = m_mallocDriver;
		void *thisMem = this;

		this->~Module_Win32();
		mallocDriver->Free(thisMem);
	}
}

static int WinMainCommon(HINSTANCE hInstance)
{
	rkit::Drivers *drivers = &rkit::g_drivers_Win32;

	drivers->m_mallocDriver.m_obj = &rkit::g_mallocDriver;
	drivers->m_moduleDriver.m_obj = &rkit::g_moduleDriver;

	rkit::IModule *systemModule = ::rkit::g_moduleDriver.LoadModule(::rkit::IModuleDriver::kDefaultNamespace, "System_Win32");
	if (!systemModule)
		return rkit::Result(rkit::ResultCode::kModuleLoadFailed).ToExitCode();

	rkit::SystemModuleParams_Win32 systemParams;

	rkit::Result systemInitResult = systemModule->Init(&systemParams);

	if (!systemInitResult.IsOK())
	{
		systemModule->Unload();
		return systemInitResult.ToExitCode();
	}

	rkit::IModule *programLauncherModule = ::rkit::g_moduleDriver.LoadModule(::rkit::IModuleDriver::kDefaultNamespace, "ProgramLauncher");
	if (!programLauncherModule)
	{
		systemModule->Unload();
		return rkit::Result(rkit::ResultCode::kModuleLoadFailed).ToExitCode();
	}

	rkit::Result result = programLauncherModule->Init(nullptr);
	if (result.IsOK())
	{
		result = drivers->m_programDriver->InitProgram();
		if (result.IsOK())
		{
			for (;;)
			{
				bool isExiting = false;
				result = drivers->m_programDriver->RunFrame(isExiting);

				if (!result.IsOK() || isExiting)
					break;
			}
		}

		drivers->m_programDriver->ShutdownProgram();
	}

	programLauncherModule->Unload();
	systemModule->Unload();

	return result.ToExitCode();
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
