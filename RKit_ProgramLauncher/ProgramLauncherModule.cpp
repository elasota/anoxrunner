#include "rkit/Core/Algorithm.h"
#include "rkit/Core/MallocDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/ProgramDriver.h"
#include "rkit/Core/Span.h"
#include "rkit/Core/String.h"
#include "rkit/Core/UtilitiesDriver.h"
#include "rkit/Core/Vector.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/SystemDriver.h"

#include "rkit/Utilities/Json.h"
#include "rkit/Core/MemoryStream.h"

#include <new>
#include <cstring>


namespace rkit
{
	class TrackedModule;
	class ProgramDriver;

	namespace ProgramLauncherPrivate
	{
		struct ModuleDef
		{
			uint32_t m_namespace = 0;
			bool m_isProgram = false;
			String m_name;
		};

		struct ModuleConfig
		{
			uint32_t m_defaultProgramNamespace = 0;
			String m_defaultProgramName;

			Vector<ModuleDef> m_moduleDefs;
		};

		Result LoadUtilitiesModule()
		{
			IModule *utilitiesModule = GetDrivers().m_moduleDriver->LoadModule(::rkit::IModuleDriver::kDefaultNamespace, "Utilities");

			if (!utilitiesModule)
				return ResultCode::kModuleLoadFailed;

			return utilitiesModule->Init(nullptr);
		}

		Result TryGetJsonObjectValue(const Utilities::JsonValue &objValue, const StringView &key, bool &outExists, Utilities::JsonValue &outValue)
		{
			if (objValue.GetType() != Utilities::JsonElementType::kObject)
				return ResultCode::kConfigInvalid;

			Result getResult = objValue.GetObjectElement(key, outValue);
			if (getResult.GetResultCode() == ResultCode::kKeyNotFound)
			{
				outExists = false;
				return ResultCode::kOK;
			}
			else if (!getResult.IsOK())
				return getResult;

			outExists = true;
			return ResultCode::kOK;
		}

		Result TryGetJsonObjectValueOfType(const Utilities::JsonValue &objValue, const StringView &key, Utilities::JsonElementType desiredElementType, bool &outExists, Utilities::JsonValue &outValue)
		{
			RKIT_CHECK(TryGetJsonObjectValue(objValue, key, outExists, outValue));

			if (outValue.GetType() != desiredElementType)
				return ResultCode::kConfigInvalid;

			return ResultCode::kOK;
		}

		Result StringToNamespaceID(const StringView &namespaceStr, uint32_t &outNamespaceID)
		{
			if (namespaceStr.Length() > 4)
				return ResultCode::kConfigInvalid;

			uint32_t namespaceID = 0;
			for (size_t i = 0; i < namespaceStr.Length(); i++)
			{
				uint32_t namespaceByte = static_cast<uint8_t>(namespaceStr[i]);
				namespaceID |= static_cast<uint32_t>(namespaceByte << (i * 8));
			}

			outNamespaceID = namespaceID;

			return ResultCode::kOK;
		}

		Result TryGetJsonObjectValueString(const Utilities::JsonValue &objValue, const StringView &key, bool &outExists, StringView &outStr)
		{
			Utilities::JsonValue strValue;
			RKIT_CHECK(TryGetJsonObjectValueOfType(objValue, key, Utilities::JsonElementType::kString, outExists, strValue));

			if (!outExists)
				return ResultCode::kOK;

			RKIT_CHECK(strValue.ToString(outStr));

			outExists = true;

			return ResultCode::kOK;
		}

		Result LoadModuleConfig(ModuleConfig &outConfig)
		{
			UniquePtr<ISeekableReadStream> configStream = GetDrivers().m_systemDriver->OpenFileRead(FileLocation::kGameDirectory, "RKitModuleConfig.json");

			if (!configStream.IsValid())
				return ResultCode::kConfigMissing;

			UniquePtr<Utilities::IJsonDocument> configJsonDoc;
			RKIT_CHECK(GetDrivers().m_utilitiesDriver->CreateJsonDocument(configJsonDoc, GetDrivers().m_mallocDriver, configStream.Get()));

			Utilities::JsonValue docJV;
			RKIT_CHECK(configJsonDoc->ToJsonValue(docJV));

			if (docJV.GetType() != Utilities::JsonElementType::kObject)
				return ResultCode::kConfigInvalid;

			bool has = false;
			Utilities::JsonValue defaultProgramJV;
			RKIT_CHECK(TryGetJsonObjectValueOfType(docJV, "DefaultProgram", Utilities::JsonElementType::kObject, has, defaultProgramJV));

			if (!has)
				return ResultCode::kConfigInvalid;

			Utilities::JsonValue modulesJV;
			RKIT_CHECK(TryGetJsonObjectValueOfType(docJV, "Modules", Utilities::JsonElementType::kObject, has, modulesJV));

			if (!has)
				return ResultCode::kConfigInvalid;

			Utilities::JsonValue programsJV;
			RKIT_CHECK(TryGetJsonObjectValueOfType(docJV, "Programs", Utilities::JsonElementType::kObject, has, programsJV));

			if (!has)
				return ResultCode::kConfigInvalid;

			outConfig.m_defaultProgramNamespace = 0;

			{
				StringView defaultProgramNamespaceSV;
				RKIT_CHECK(TryGetJsonObjectValueString(defaultProgramJV, "Namespace", has, defaultProgramNamespaceSV));

				if (!has)
					return ResultCode::kConfigInvalid;

				RKIT_CHECK(StringToNamespaceID(defaultProgramNamespaceSV, outConfig.m_defaultProgramNamespace));
			}

			{
				StringView defaultProgramNameSV;
				RKIT_CHECK(TryGetJsonObjectValueString(defaultProgramJV, "Name", has, defaultProgramNameSV));

				if (!has)
					return ResultCode::kConfigInvalid;

				String nameStr;
				RKIT_CHECK(nameStr.Set(defaultProgramNameSV));

				outConfig.m_defaultProgramName = std::move(nameStr);
			}

			auto iterateModules = [&outConfig](const StringView &key, const Utilities::JsonValue &value, bool &shouldContinue) -> Result
			{
				uint32_t namespaceID = 0;

				RKIT_CHECK(StringToNamespaceID(key, namespaceID));

				if (value.GetType() != Utilities::JsonElementType::kArray)
					return ResultCode::kConfigInvalid;

				size_t arraySize = 0;
				RKIT_CHECK(value.GetArraySize(arraySize));

				for (size_t i = 0; i < arraySize; i++)
				{
					Utilities::JsonValue namespaceNameJV;

					RKIT_CHECK(value.GetArrayElement(i, namespaceNameJV));

					if (namespaceNameJV.GetType() != Utilities::JsonElementType::kString)
						return ResultCode::kConfigInvalid;

					StringView namespaceNameSV;
					RKIT_CHECK(namespaceNameJV.ToString(namespaceNameSV));

					ProgramLauncherPrivate::ModuleDef moduleDef;
					RKIT_CHECK(moduleDef.m_name.Set(namespaceNameSV));
					moduleDef.m_namespace = namespaceID;

					RKIT_CHECK(outConfig.m_moduleDefs.Append(std::move(moduleDef)));
				}

				return ResultCode::kOK;
			};

			RKIT_CHECK(modulesJV.IterateObjectWithCallable(iterateModules));

			auto iteratePrograms = [&outConfig](const StringView &key, const Utilities::JsonValue &value, bool &shouldContinue) -> Result
			{
				uint32_t namespaceID = 0;

				RKIT_CHECK(StringToNamespaceID(key, namespaceID));

				if (value.GetType() != Utilities::JsonElementType::kArray)
					return ResultCode::kConfigInvalid;

				size_t arraySize = 0;
				RKIT_CHECK(value.GetArraySize(arraySize));

				for (size_t i = 0; i < arraySize; i++)
				{
					Utilities::JsonValue namespaceNameJV;

					RKIT_CHECK(value.GetArrayElement(i, namespaceNameJV));

					if (namespaceNameJV.GetType() != Utilities::JsonElementType::kString)
						return ResultCode::kConfigInvalid;

					StringView namespaceNameSV;
					RKIT_CHECK(namespaceNameJV.ToString(namespaceNameSV));

					bool found = false;
					for (ProgramLauncherPrivate::ModuleDef &moduleDef : outConfig.m_moduleDefs)
					{
						if (moduleDef.m_namespace == namespaceID && moduleDef.m_name == namespaceNameSV)
						{
							found = true;
							moduleDef.m_isProgram = true;
							break;
						}
					}

					if (!found)
						return ResultCode::kConfigInvalid;

					ProgramLauncherPrivate::ModuleDef moduleDef;
					RKIT_CHECK(moduleDef.m_name.Set(namespaceNameSV));
					moduleDef.m_namespace = namespaceID;

					RKIT_CHECK(outConfig.m_moduleDefs.Append(std::move(moduleDef)));
				}

				return ResultCode::kOK;
			};

			RKIT_CHECK(programsJV.IterateObjectWithCallable(iteratePrograms));

			return ResultCode::kOK;
		}
	
		Result LoadProgramModule(const ModuleConfig &moduleConfig)
		{
			// Check command line
			Span<const StringView> cmdLine = GetDrivers().m_systemDriver->GetCommandLine();

			uint32_t programNamespaceID = moduleConfig.m_defaultProgramNamespace;
			StringView programName = moduleConfig.m_defaultProgramName;

			if (cmdLine.Count() >= 2 && cmdLine[1] == "-tool")
			{
				if (cmdLine.Count() == 2)
					return ResultCode::kInvalidCommandLine;

				bool found = false;
				for (const ProgramLauncherPrivate::ModuleDef &moduleDef : moduleConfig.m_moduleDefs)
				{
					if (moduleDef.m_name == cmdLine[2])
					{
						if (!moduleDef.m_isProgram)
							return ResultCode::kInvalidCommandLine;

						programNamespaceID = moduleDef.m_namespace;
						programName = moduleDef.m_name;
						found = true;
						break;
					}
				}

				if (!found)
					return ResultCode::kUnknownTool;

				GetDrivers().m_systemDriver->RemoveCommandLineArgs(0, 3);
			}
			else
				GetDrivers().m_systemDriver->RemoveCommandLineArgs(0, 2);

			IModule *programModule = GetDrivers().m_moduleDriver->LoadModule(programNamespaceID, programName.GetChars());

			if (!programModule)
				return ResultCode::kModuleLoadFailed;

			return programModule->Init(nullptr);
		}
	}

	class TrackedModuleDriver final : public IModuleDriver
	{
	public:
		friend class TrackedModule;

		explicit TrackedModuleDriver(const SimpleObjectAllocation<IModuleDriver> &moduleDriver);

		void UnloadAllTrackedModules();

		IModule *LoadModule(uint32_t moduleNamespace, const char *moduleName) override;

		void Uninstall();

	private:
		SimpleObjectAllocation<IModuleDriver> m_moduleDriver;

		TrackedModule *m_firstModule;
		TrackedModule *m_lastModule;
	};

	class TrackedModule final : public IModule
	{
	public:
		TrackedModule(IModule *module, TrackedModule *prevModule, IMallocDriver *mallocDriver, TrackedModuleDriver *trackedModuleDriver);

		Result Init(const ModuleInitParameters *initParams) override;
		void Unload() override;

	private:
		TrackedModule *m_prevModule;
		TrackedModule *m_nextModule;

		IMallocDriver *m_mallocDriver;
		TrackedModuleDriver *m_trackedModuleDriver;
		IModule *m_module;
	};

	class ProgramModule
	{
	public:
		static Result Init(const ModuleInitParameters *);
		static void Shutdown();

		static SimpleObjectAllocation<TrackedModuleDriver> ms_trackedModuleDriver;

	private:
		static SimpleObjectAllocation<ProgramLauncherPrivate::ModuleConfig> ms_moduleConfig;
	};

	TrackedModuleDriver::TrackedModuleDriver(const SimpleObjectAllocation<IModuleDriver> &moduleDriver)
		: m_moduleDriver(moduleDriver)
		, m_firstModule(nullptr)
		, m_lastModule(nullptr)
	{
	}

	void TrackedModuleDriver::UnloadAllTrackedModules()
	{
		while (m_lastModule)
			m_lastModule->Unload();
	}

	IModule *TrackedModuleDriver::LoadModule(uint32_t moduleNamespace, const char *moduleName)
	{
		IMallocDriver *mallocDriver = GetDrivers().m_mallocDriver;

		void *moduleMem = mallocDriver->Alloc(sizeof(TrackedModule));
		if (!moduleMem)
			return nullptr;

		IModule *module = m_moduleDriver->LoadModule(moduleNamespace, moduleName);
		if (!module)
		{
			mallocDriver->Free(moduleMem);
			return nullptr;
		}

		TrackedModule *newModule = new (moduleMem) TrackedModule(module, m_lastModule, mallocDriver, this);
		if (!m_firstModule)
			m_firstModule = newModule;
		m_lastModule = newModule;

		return newModule;
	}

	void TrackedModuleDriver::Uninstall()
	{
		GetMutableDrivers().m_moduleDriver = m_moduleDriver;
	}

	TrackedModule::TrackedModule(IModule *module, TrackedModule *prevModule, IMallocDriver *mallocDriver, TrackedModuleDriver *trackedModuleDriver)
		: m_module(module)
		, m_prevModule(prevModule)
		, m_nextModule(nullptr)
		, m_mallocDriver(mallocDriver)
		, m_trackedModuleDriver(trackedModuleDriver)
	{
		if (prevModule)
			prevModule->m_nextModule = this;
	}

	Result TrackedModule::Init(const ModuleInitParameters *initParams)
	{
		return m_module->Init(initParams);
	}

	void TrackedModule::Unload()
	{
		m_module->Unload();

		if (m_trackedModuleDriver->m_firstModule == this)
			m_trackedModuleDriver->m_firstModule = this->m_nextModule;
		if (m_trackedModuleDriver->m_lastModule == this)
			m_trackedModuleDriver->m_lastModule = this->m_prevModule;

		if (m_nextModule)
			m_nextModule->m_prevModule = m_prevModule;
		if (m_prevModule)
			m_prevModule->m_nextModule = m_nextModule;


		IMallocDriver *mallocDriver = m_mallocDriver;
		void *thisMem = this;

		this->~TrackedModule();

		mallocDriver->Free(thisMem);
	}

	Result ProgramModule::Init(const ModuleInitParameters *)
	{
		UniquePtr<ProgramLauncherPrivate::ModuleConfig> moduleConfig;
		RKIT_CHECK(New<ProgramLauncherPrivate::ModuleConfig>(moduleConfig));

		UniquePtr<TrackedModuleDriver> driver;
		RKIT_CHECK(New<TrackedModuleDriver>(driver, GetDrivers().m_moduleDriver));
		ms_trackedModuleDriver = driver.Detach();

		GetMutableDrivers().m_moduleDriver = ms_trackedModuleDriver;

		RKIT_CHECK(ProgramLauncherPrivate::LoadUtilitiesModule());

		RKIT_CHECK(ProgramLauncherPrivate::LoadModuleConfig(*moduleConfig.Get()));

		ms_moduleConfig = moduleConfig.Detach();

		RKIT_CHECK(ProgramLauncherPrivate::LoadProgramModule(*ms_moduleConfig));

		return Result();
	}

	void ProgramModule::Shutdown()
	{
		if (ms_trackedModuleDriver)
		{
			// Unload all tracked modules
			ms_trackedModuleDriver->UnloadAllTrackedModules();

			ms_trackedModuleDriver->Uninstall();
			SafeDelete(ms_trackedModuleDriver);
		}

		if (ms_moduleConfig)
		{
			SafeDelete(ms_trackedModuleDriver);
		}
	}

	SimpleObjectAllocation<TrackedModuleDriver> ProgramModule::ms_trackedModuleDriver = {};
	SimpleObjectAllocation<ProgramLauncherPrivate::ModuleConfig> ProgramModule::ms_moduleConfig = {};
}

RKIT_IMPLEMENT_MODULE("RKit", "ProgramLauncher", ::rkit::ProgramModule)
