#include "rkit/Core/Algorithm.h"
#include "rkit/Core/MallocDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Path.h"
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
			IModule *utilitiesModule = GetDrivers().m_moduleDriver->LoadModule(::rkit::IModuleDriver::kDefaultNamespace, u8"Utilities");

			if (!utilitiesModule)
				RKIT_THROW(ResultCode::kModuleLoadFailed);

			RKIT_RETURN_OK;
		}

		Result TryGetJsonObjectValue(const utils::JsonValue &objValue, const StringView &key, bool &outExists, utils::JsonValue &outValue)
		{
			if (objValue.GetType() != utils::JsonElementType::kObject)
				RKIT_THROW(rkit::ResultCode::kConfigInvalid);

			return objValue.TryGetObjectElement(key, outExists, outValue);
		}

		Result TryGetJsonObjectValueOfType(const utils::JsonValue &objValue, const StringView &key, utils::JsonElementType desiredElementType, bool &outExists, utils::JsonValue &outValue)
		{
			RKIT_CHECK(TryGetJsonObjectValue(objValue, key, outExists, outValue));

			if (outValue.GetType() != desiredElementType)
				RKIT_THROW(rkit::ResultCode::kConfigInvalid);

			RKIT_RETURN_OK;
		}

		Result StringToNamespaceID(const StringView &namespaceStr, uint32_t &outNamespaceID)
		{
			if (namespaceStr.Length() > 4)
				RKIT_THROW(rkit::ResultCode::kConfigInvalid);

			char namespaceChars[4] = { 0, 0, 0, 0 };

			for (size_t i = 0; i < namespaceStr.Length() && i < 4; i++)
				namespaceChars[i] = namespaceStr[i];

			outNamespaceID = utils::ComputeFourCC(namespaceChars[0], namespaceChars[1], namespaceChars[2], namespaceChars[3]);

			RKIT_RETURN_OK;
		}

		Result TryGetJsonObjectValueString(const utils::JsonValue &objValue, const StringView &key, bool &outExists, StringView &outStr)
		{
			utils::JsonValue strValue;
			RKIT_CHECK(TryGetJsonObjectValueOfType(objValue, key, utils::JsonElementType::kString, outExists, strValue));

			if (!outExists)
				RKIT_RETURN_OK;

			RKIT_CHECK(strValue.ToString(outStr));

			outExists = true;

			RKIT_RETURN_OK;
		}

		Result LoadModuleConfig(ModuleConfig &outConfig)
		{
			UniquePtr<ISeekableReadStream> configStream;

			RKIT_CHECK(GetDrivers().m_systemDriver->OpenFileRead(configStream, FileLocation::kProgramDirectory, u8"rkitmoduleconfig.json", false));

			UniquePtr<utils::IJsonDocument> configJsonDoc;
			RKIT_CHECK(GetDrivers().m_utilitiesDriver->CreateJsonDocument(configJsonDoc, GetDrivers().m_mallocDriver, configStream.Get()));

			utils::JsonValue docJV;
			RKIT_CHECK(configJsonDoc->ToJsonValue(docJV));

			if (docJV.GetType() != utils::JsonElementType::kObject)
				RKIT_THROW(rkit::ResultCode::kConfigInvalid);

			bool has = false;
			utils::JsonValue defaultProgramJV;
			RKIT_CHECK(TryGetJsonObjectValueOfType(docJV, u8"DefaultProgram", utils::JsonElementType::kObject, has, defaultProgramJV));

			if (!has)
				RKIT_THROW(rkit::ResultCode::kConfigInvalid);

			utils::JsonValue modulesJV;
			RKIT_CHECK(TryGetJsonObjectValueOfType(docJV, u8"Modules", utils::JsonElementType::kObject, has, modulesJV));

			if (!has)
				RKIT_THROW(rkit::ResultCode::kConfigInvalid);

			utils::JsonValue programsJV;
			RKIT_CHECK(TryGetJsonObjectValueOfType(docJV, u8"Programs", utils::JsonElementType::kObject, has, programsJV));

			if (!has)
				RKIT_THROW(rkit::ResultCode::kConfigInvalid);

			outConfig.m_defaultProgramNamespace = 0;

			{
				StringView defaultProgramNamespaceSV;
				RKIT_CHECK(TryGetJsonObjectValueString(defaultProgramJV, u8"Namespace", has, defaultProgramNamespaceSV));

				if (!has)
					RKIT_THROW(rkit::ResultCode::kConfigInvalid);

				RKIT_CHECK(StringToNamespaceID(defaultProgramNamespaceSV, outConfig.m_defaultProgramNamespace));
			}

			{
				StringView defaultProgramNameSV;
				RKIT_CHECK(TryGetJsonObjectValueString(defaultProgramJV, u8"Name", has, defaultProgramNameSV));

				if (!has)
					RKIT_THROW(rkit::ResultCode::kConfigInvalid);

				String nameStr;
				RKIT_CHECK(nameStr.Set(defaultProgramNameSV));

				outConfig.m_defaultProgramName = std::move(nameStr);
			}

			auto iterateModules = [&outConfig](const StringView &key, const utils::JsonValue &value, bool &shouldContinue) -> Result
			{
				uint32_t namespaceID = 0;

				RKIT_CHECK(StringToNamespaceID(key, namespaceID));

				if (value.GetType() != utils::JsonElementType::kArray)
					RKIT_THROW(rkit::ResultCode::kConfigInvalid);

				size_t arraySize = 0;
				RKIT_CHECK(value.GetArraySize(arraySize));

				for (size_t i = 0; i < arraySize; i++)
				{
					utils::JsonValue namespaceNameJV;

					RKIT_CHECK(value.GetArrayElement(i, namespaceNameJV));

					if (namespaceNameJV.GetType() != utils::JsonElementType::kString)
						RKIT_THROW(rkit::ResultCode::kConfigInvalid);

					StringView namespaceNameSV;
					RKIT_CHECK(namespaceNameJV.ToString(namespaceNameSV));

					ProgramLauncherPrivate::ModuleDef moduleDef;
					RKIT_CHECK(moduleDef.m_name.Set(namespaceNameSV));
					moduleDef.m_namespace = namespaceID;

					RKIT_CHECK(outConfig.m_moduleDefs.Append(std::move(moduleDef)));
				}

				RKIT_RETURN_OK;
			};

			RKIT_CHECK(modulesJV.IterateObjectWithCallable(iterateModules));

			auto iteratePrograms = [&outConfig](const StringView &key, const utils::JsonValue &value, bool &shouldContinue) -> Result
			{
				uint32_t namespaceID = 0;

				RKIT_CHECK(StringToNamespaceID(key, namespaceID));

				if (value.GetType() != utils::JsonElementType::kArray)
					RKIT_THROW(rkit::ResultCode::kConfigInvalid);

				size_t arraySize = 0;
				RKIT_CHECK(value.GetArraySize(arraySize));

				for (size_t i = 0; i < arraySize; i++)
				{
					utils::JsonValue namespaceNameJV;

					RKIT_CHECK(value.GetArrayElement(i, namespaceNameJV));

					if (namespaceNameJV.GetType() != utils::JsonElementType::kString)
						RKIT_THROW(rkit::ResultCode::kConfigInvalid);

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
						RKIT_THROW(rkit::ResultCode::kConfigInvalid);

					ProgramLauncherPrivate::ModuleDef moduleDef;
					RKIT_CHECK(moduleDef.m_name.Set(namespaceNameSV));
					moduleDef.m_namespace = namespaceID;

					RKIT_CHECK(outConfig.m_moduleDefs.Append(std::move(moduleDef)));
				}

				RKIT_RETURN_OK;
			};

			RKIT_CHECK(programsJV.IterateObjectWithCallable(iteratePrograms));

			RKIT_RETURN_OK;
		}
	
		Result LoadProgramModule(const ModuleConfig &moduleConfig)
		{
			// Check command line
			Span<const StringView> cmdLine = GetDrivers().m_systemDriver->GetCommandLine();

			uint32_t programNamespaceID = moduleConfig.m_defaultProgramNamespace;
			StringView programName = moduleConfig.m_defaultProgramName;

			if (cmdLine.Count() >= 2 && cmdLine[1] == u8"-tool")
			{
				if (cmdLine.Count() == 2)
					RKIT_THROW(ResultCode::kInvalidCommandLine);

				bool found = false;
				for (const ProgramLauncherPrivate::ModuleDef &moduleDef : moduleConfig.m_moduleDefs)
				{
					if (moduleDef.m_name == cmdLine[2])
					{
						if (!moduleDef.m_isProgram)
							RKIT_THROW(ResultCode::kInvalidCommandLine);

						programNamespaceID = moduleDef.m_namespace;
						programName = moduleDef.m_name;
						found = true;
						break;
					}
				}

				if (!found)
					RKIT_THROW(ResultCode::kUnknownTool);

				GetDrivers().m_systemDriver->RemoveCommandLineArgs(0, 3);
			}
			else
				GetDrivers().m_systemDriver->RemoveCommandLineArgs(0, 1);

			IModule *programModule = GetDrivers().m_moduleDriver->LoadModule(programNamespaceID, programName.GetChars());

			if (!programModule)
				RKIT_THROW(ResultCode::kModuleLoadFailed);

			RKIT_RETURN_OK;
		}
	}

	class TrackedModuleDriver final : public IModuleDriver
	{
	public:
		friend class TrackedModule;

		explicit TrackedModuleDriver(const SimpleObjectAllocation<IModuleDriver> &moduleDriver);

		void UnloadAllTrackedModules();

		IModule *LoadModuleInternal(uint32_t moduleNamespace, const Utf8Char_t *moduleName, const ModuleInitParameters* initParams, IMallocDriver &mallocDriver) override;

		void Uninstall();

	private:
		SimpleObjectAllocation<IModuleDriver> m_moduleDriver;

		TrackedModule *m_firstModule;
		TrackedModule *m_lastModule;
	};

	class TrackedModule final : public IModule
	{
	public:
		TrackedModule(IModule *module, TrackedModule *prevModule, IMallocDriver *mallocDriver, uint32_t namespaceID, String &&name, TrackedModuleDriver *trackedModuleDriver);

		Result Init(const ModuleInitParameters *initParams) override;
		void Unload() override;

		uint32_t GetNamespace() const;
		StringView GetName() const;
		TrackedModule *GetNext() const;

	private:
		uint32_t m_namespace;
		String m_name;

		TrackedModule *m_prevModule;
		TrackedModule *m_nextModule;

		IMallocDriver *m_mallocDriver;
		TrackedModuleDriver *m_trackedModuleDriver;
		IModule *m_module;
		bool m_initialized;
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

	IModule *TrackedModuleDriver::LoadModuleInternal(uint32_t moduleNamespace, const Utf8Char_t *moduleName, const ModuleInitParameters *initParams, IMallocDriver &mallocDriver)
	{
		StringView moduleNameStringView = StringView::FromCString(moduleName);
		TrackedModule *scanModule = m_firstModule;

		while (scanModule != nullptr)
		{
			if (scanModule->GetNamespace() == moduleNamespace && scanModule->GetName() == moduleNameStringView)
				return scanModule;

			scanModule = scanModule->GetNext();
		}

		String nameStr;
		if (!utils::ResultIsOK(RKIT_TRY_EVAL(nameStr.Set(moduleNameStringView))))
			return nullptr;

		void *moduleMem = mallocDriver.Alloc(sizeof(TrackedModule));
		if (!moduleMem)
			return nullptr;

		IModule *module = m_moduleDriver->LoadModule(moduleNamespace, moduleName, initParams);
		if (!module)
		{
			mallocDriver.Free(moduleMem);
			return nullptr;
		}

		TrackedModule *newModule = new (moduleMem) TrackedModule(module, m_lastModule, &mallocDriver, moduleNamespace, std::move(nameStr), this);
		if (!m_firstModule)
			m_firstModule = newModule;

		m_lastModule = newModule;

		return newModule;
	}

	void TrackedModuleDriver::Uninstall()
	{
		GetMutableDrivers().m_moduleDriver = m_moduleDriver;
	}

	TrackedModule::TrackedModule(IModule *module, TrackedModule *prevModule, IMallocDriver *mallocDriver, uint32_t namespaceID, String &&nameStr, TrackedModuleDriver *trackedModuleDriver)
		: m_module(module)
		, m_prevModule(prevModule)
		, m_nextModule(nullptr)
		, m_mallocDriver(mallocDriver)
		, m_trackedModuleDriver(trackedModuleDriver)
		, m_initialized(false)
		, m_namespace(namespaceID)
		, m_name(std::move(nameStr))
	{
		if (prevModule)
			prevModule->m_nextModule = this;
	}

	Result TrackedModule::Init(const ModuleInitParameters *initParams)
	{
		if (m_initialized)
			RKIT_RETURN_OK;

		RKIT_CHECK(m_module->Init(initParams));

		m_initialized = true;

		RKIT_RETURN_OK;
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


	uint32_t TrackedModule::GetNamespace() const
	{
		return m_namespace;
	}

	StringView TrackedModule::GetName() const
	{
		return m_name;
	}

	TrackedModule *TrackedModule::GetNext() const
	{
		return m_nextModule;
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

		return Result(ResultCode::kOK);
	}

	void ProgramModule::Shutdown()
	{
		SafeDelete(ms_moduleConfig);

		if (ms_trackedModuleDriver)
		{
			// Unload all tracked modules
			ms_trackedModuleDriver->UnloadAllTrackedModules();

			ms_trackedModuleDriver->Uninstall();
			SafeDelete(ms_trackedModuleDriver);
		}
	}

	SimpleObjectAllocation<TrackedModuleDriver> ProgramModule::ms_trackedModuleDriver = {};
	SimpleObjectAllocation<ProgramLauncherPrivate::ModuleConfig> ProgramModule::ms_moduleConfig = {};
}

RKIT_IMPLEMENT_MODULE(RKit, ProgramLauncher, ::rkit::ProgramModule)
