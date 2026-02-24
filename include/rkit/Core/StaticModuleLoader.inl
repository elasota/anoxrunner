#pragma once

#if RKIT_MODULE_LINKER_TYPE == RKIT_MODULE_LINKER_TYPE_STATIC

#include "Module.h"
#include "ModuleGlue.h"
#include "ModuleDriver.h"
#include "Span.h"

namespace rkit::moduleloader
{
	class StaticModule final : public IModule
	{
	public:
		explicit StaticModule(const ModuleInfo &mdl, IMallocDriver *alloc);

		Result Init(const ModuleInitParameters *initParams) override;
		void Unload() override;

	private:
		const ModuleInfo &m_module;
		IMallocDriver *m_alloc;
		bool m_isInitialized;
	};

	class StaticModuleDriver final : public IModuleDriver
	{
	protected:
		IModule *LoadModuleInternal(uint32_t moduleNamespace, const Utf8Char_t *moduleName, const ModuleInitParameters *initParams, IMallocDriver &mallocDriver) override;
	};
}

namespace rkit::moduleloader
{
	StaticModule::StaticModule(const ModuleInfo &mdl, IMallocDriver *alloc)
		: m_module(mdl)
		, m_alloc(alloc)
		, m_isInitialized(false)
	{
	}

	Result StaticModule::Init(const ModuleInitParameters *initParams)
	{
		m_isInitialized = true;
		return m_module.m_initFunction(initParams);
	}

	void StaticModule::Unload()
	{
		if (m_isInitialized)
			m_module.m_shutdownFunction();

		IMallocDriver *alloc = m_alloc;
		void *thisMem = this;

		this->~StaticModule();
		alloc->Free(thisMem);
	}

	IModule *StaticModuleDriver::LoadModuleInternal(uint32_t moduleNamespace, const Utf8Char_t *moduleName, const ModuleInitParameters *initParams, IMallocDriver &mallocDriver)
	{
		ConstSpan<const ModuleInfo *> modules(g_moduleList.m_modules, g_moduleList.m_numModules);

		const ModuleInfo *foundModule = nullptr;
		for (const ModuleInfo *moduleInfo : modules)
		{
			if (moduleInfo->m_namespace != moduleNamespace)
				continue;

			const size_t nameLen = moduleInfo->m_nameLength;
			const Utf8Char_t *nameChars = moduleInfo->m_name;

			bool matched = true;

			for (size_t i = 0; i < nameLen; i++)
			{
				if (nameChars[i] != moduleName[i])
				{
					matched = false;
					break;
				}
			}

			if (matched && (moduleName[nameLen] == static_cast<Utf8Char_t>(0)))
			{
				foundModule = moduleInfo;
				break;
			}
		}

		if (!foundModule)
			return nullptr;

		void *moduleMemory = mallocDriver.Alloc(sizeof(StaticModule));
		if (!moduleMemory)
			return nullptr;

		IModule *module = new (moduleMemory) StaticModule(*foundModule, &mallocDriver);

		PackedResultAndExtCode initResult = RKIT_TRY_EVAL(module->Init(initParams));
		if (!utils::ResultIsOK(initResult))
		{
			module->Unload();
			return nullptr;
		}

		return module;
	}
}

#endif
