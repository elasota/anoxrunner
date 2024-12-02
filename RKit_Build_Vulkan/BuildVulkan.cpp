#include "rkit/BuildSystem/BuildSystem.h"

#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"

#include "rkit/Vulkan/GraphicsPipeline.h"

#include "VulkanRenderPipelineCompiler.h"

#include <glslang/Include/glslang_c_interface.h>

namespace rkit::buildsystem
{
	class BuildVulkanDriver final : public rkit::buildsystem::IBuildSystemAddOnDriver
	{
	public:

	private:
		rkit::Result InitDriver() override;
		void ShutdownDriver() override;

		Result RegisterBuildSystemAddOn(IBuildSystemInstance *instance) override;

		uint32_t GetDriverNamespaceID() const override { return rkit::IModuleDriver::kDefaultNamespace; }
		rkit::StringView GetDriverName() const override { return "Build_Vulkan"; }

		bool m_isGlslangInitialized = false;
	};

	typedef rkit::CustomDriverModuleStub<BuildVulkanDriver> BuildVulkanModule;

	rkit::Result BuildVulkanDriver::InitDriver()
	{
		if (!glslang_initialize_process())
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
			glslang_finalize_process();
			m_isGlslangInitialized = false;
		}
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
			RKIT_CHECK(rkit::buildsystem::vulkan::CreateGraphicsPipelineStageCompiler(stage, stageCompiler));

			RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kDefaultNamespace, vulkan::CreateNodeTypeIDForStage(stage), std::move(stageCompiler)));
		}

		return ResultCode::kOK;
	}
}

RKIT_IMPLEMENT_MODULE("RKit", "Build_Vulkan", ::rkit::buildsystem::BuildVulkanModule)
