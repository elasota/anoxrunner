#include "rkit/BuildSystem/BuildSystem.h"

#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"

#include "rkit/Vulkan/GraphicsPipeline.h"

#include "VulkanRenderPipelineCompiler.h"

#include <glslang/Include/glslang_c_interface.h>
#include "rkit/ShaderC/ShaderC_DLL.h"

namespace rkit { namespace buildsystem
{
	class BuildVulkanDriver final : public rkit::buildsystem::IBuildSystemAddOnDriver
	{
	public:

	private:
		rkit::Result InitDriver(const DriverInitParameters *initParams) override;
		void ShutdownDriver() override;

		Result RegisterBuildSystemAddOn(IBuildSystemInstance *instance) override;

		uint32_t GetDriverNamespaceID() const override { return rkit::IModuleDriver::kDefaultNamespace; }
		rkit::StringView GetDriverName() const override { return "Build_Vulkan"; }

		bool m_isGlslangInitialized = false;

		IModule *m_shaderCModule = nullptr;
		const vulkan::GlslCApi *m_glslc = nullptr;
	};

	typedef rkit::CustomDriverModuleStub<BuildVulkanDriver> BuildVulkanModule;

	rkit::Result BuildVulkanDriver::InitDriver(const DriverInitParameters *initParams)
	{
		vulkan::ShaderCApiGroup apiGroup = {};

		vulkan::ShaderCModuleInitParameters shaderCInitParams = {};
		shaderCInitParams.m_outApiGroup = &apiGroup;

		m_shaderCModule = GetDrivers().m_moduleDriver->LoadModule(kDefaultNamespace, "ShaderC_DLL", &shaderCInitParams);
		if (!m_shaderCModule)
		{
			rkit::log::Error("ShaderC module failed to load");
			return rkit::ResultCode::kOperationFailed;
		}

		m_glslc = apiGroup.m_glslApi;

		if (!m_glslc->glslang_initialize_process())
		{
			rkit::log::Error("glslang failed to init");
			return rkit::ResultCode::kOperationFailed;
		}

		m_isGlslangInitialized = true;

		return rkit::ResultCode::kOK;
	}

	void BuildVulkanDriver::ShutdownDriver()
	{
		if (m_isGlslangInitialized)
		{
			m_glslc->glslang_finalize_process();
			m_isGlslangInitialized = false;
		}

		if (m_shaderCModule)
			m_shaderCModule->Unload();
	}

	Result BuildVulkanDriver::RegisterBuildSystemAddOn(IBuildSystemInstance *instance)
	{
		UniquePtr<buildsystem::IDependencyNodeCompiler> pipelineCompiler;
		RKIT_CHECK(rkit::buildsystem::vulkan::CreatePipelineCompiler(pipelineCompiler));

		RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kDefaultNamespace, kRenderGraphicsPipelineNodeID, std::move(pipelineCompiler)));

		for (size_t i = 0; i < static_cast<size_t>(render::vulkan::GraphicPipelineStage::Count); i++)
		{
			render::vulkan::GraphicPipelineStage stage = static_cast<render::vulkan::GraphicPipelineStage>(i);

			UniquePtr<buildsystem::IDependencyNodeCompiler> stageCompiler;
			RKIT_CHECK(rkit::buildsystem::vulkan::CreateGraphicsPipelineStageCompiler(m_glslc, stage, stageCompiler));

			RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kDefaultNamespace, vulkan::CreateNodeTypeIDForStage(stage), std::move(stageCompiler)));
		}

		return ResultCode::kOK;
	}
} } // rkit::buildsystem

RKIT_IMPLEMENT_MODULE("RKit", "Build_Vulkan", ::rkit::buildsystem::BuildVulkanModule)
