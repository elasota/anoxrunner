#include "VulkanRenderPipelineCompiler.h"

#include "rkit/BuildSystem/BuildSystem.h"

#include "rkit/Data/DataDriver.h"
#include "rkit/Data/RenderDataHandler.h"

#include "rkit/Render/RenderDefs.h"

#include "rkit/Vulkan/GraphicsPipeline.h"

#include "rkit/Core/FourCC.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/Stream.h"

namespace rkit::buildsystem::vulkan
{
	uint32_t CreateNodeTypeIDForStageInt(char c0, char c1, uint8_t pipelineStage)
	{
		char c2 = static_cast<char>('0' + pipelineStage / 10);
		char c3 = static_cast<char>('0' + pipelineStage % 10);

		if (c2 == '0')
			c2 = 'P';

		return utils::ComputeFourCC(c0, c1, c2, c3);
	}

	uint32_t CreateNodeTypeIDForStage(rkit::render::vulkan::GraphicPipelineStage pipelineStage)
	{
		return CreateNodeTypeIDForStageInt('V', 'G', static_cast<uint8_t>(pipelineStage));
	}

	class PipelineCompilerBase : public IDependencyNodeCompiler
	{
	protected:
		static Result LoadDataDriver(data::IDataDriver **outDriver);
		static Result LoadPackage(BuildFileLocation location, const StringView &path, bool allowTempStrings, IDependencyNodeCompilerFeedback *feedback, UniquePtr<data::IRenderDataPackage> &outPackage);
	};

	enum class PipelineType
	{
		Graphics,
	};

	class RenderPipelineStageCompiler final : public PipelineCompilerBase
	{
	public:
		explicit RenderPipelineStageCompiler(PipelineType pipelineType, uint32_t stage);

		bool HasAnalysisStage() const override;
		Result RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;
		Result RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;

		Result CreatePrivateData(UniquePtr<IDependencyNodePrivateData> &outPrivateData) override;

		uint32_t GetVersion() const override;

	private:
		RenderPipelineStageCompiler() = delete;

		static Result CompileGraphicsPipelineStage(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, const render::GraphicsPipelineDesc *pipeline);

		PipelineType m_pipelineType;
		uint32_t m_stageUInt;
	};

	class RenderPipelineCompiler final : public PipelineCompilerBase
	{
	public:
		explicit RenderPipelineCompiler(PipelineType stage);

		bool HasAnalysisStage() const override;
		Result RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;
		Result RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;

		Result CreatePrivateData(UniquePtr<IDependencyNodePrivateData> &outPrivateData) override;

		uint32_t GetVersion() const override;

	private:
		RenderPipelineCompiler() = delete;

		static Result CompileGraphicsPipeline(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, const render::GraphicsPipelineDesc *pipeline);

		static Result AddGraphicsAnalysisStage(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, render::vulkan::GraphicPipelineStage stage);
		static Result FormatGraphicsPipelineStageFilePath(String &str, const StringView &inPath, render::vulkan::GraphicPipelineStage stage);

		PipelineType m_pipelineType;
	};


	RenderPipelineCompiler::RenderPipelineCompiler(PipelineType pipelineType)
		: m_pipelineType(pipelineType)
	{
	}

	bool RenderPipelineCompiler::HasAnalysisStage() const
	{
		return true;
	}

	Result RenderPipelineCompiler::RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		UniquePtr<data::IRenderDataPackage> package;
		RKIT_CHECK(LoadPackage(BuildFileLocation::kIntermediateDir, depsNode->GetIdentifier(), true, feedback, package));

		Vector<uint32_t> stages;

		if (m_pipelineType == PipelineType::Graphics)
		{
			data::IRenderRTTIListBase *list = package->GetIndexable(data::RenderRTTIIndexableStructType::GraphicsPipelineDesc);
			if (list->GetCount() != 1)
			{
				rkit::log::Error("Pipeline package doesn't contain exactly one graphics pipeline");
				return ResultCode::kMalformedFile;
			}

			const render::GraphicsPipelineDesc *pipelineDesc = static_cast<const render::GraphicsPipelineDesc *>(list->GetElementPtr(0));

			if (pipelineDesc->m_vertexShader)
			{
				RKIT_CHECK(AddGraphicsAnalysisStage(depsNode, feedback, render::vulkan::GraphicPipelineStage::Vertex));
			}
			if (pipelineDesc->m_vertexShader)
			{
				RKIT_CHECK(AddGraphicsAnalysisStage(depsNode, feedback, render::vulkan::GraphicPipelineStage::Pixel));
			}
		}
		else
			return ResultCode::kInternalError;

		return ResultCode::kOK;
	}

	Result RenderPipelineCompiler::RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		UniquePtr<data::IRenderDataPackage> package;
		RKIT_CHECK(LoadPackage(BuildFileLocation::kIntermediateDir, depsNode->GetIdentifier(), true, feedback, package));

		if (m_pipelineType == PipelineType::Graphics)
		{
			data::IRenderRTTIListBase *list = package->GetIndexable(data::RenderRTTIIndexableStructType::GraphicsPipelineDesc);
			if (list->GetCount() != 1)
			{
				rkit::log::Error("Pipeline package doesn't contain exactly one graphics pipeline");
				return ResultCode::kMalformedFile;
			}

			const render::GraphicsPipelineDesc *pipelineDesc = static_cast<const render::GraphicsPipelineDesc *>(list->GetElementPtr(0));

			RKIT_CHECK(CompileGraphicsPipeline(depsNode, feedback, pipelineDesc));
		}
		else
			return ResultCode::kInternalError;

		RKIT_CHECK(feedback->CheckFault());

		return ResultCode::kOK;
	}

	Result RenderPipelineCompiler::CreatePrivateData(UniquePtr<IDependencyNodePrivateData> &outPrivateData)
	{
		return ResultCode::kOK;
	}

	uint32_t RenderPipelineCompiler::GetVersion() const
	{
		return 1;
	}

	Result RenderPipelineCompiler::CompileGraphicsPipeline(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, const render::GraphicsPipelineDesc *pipeline)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result RenderPipelineCompiler::AddGraphicsAnalysisStage(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, render::vulkan::GraphicPipelineStage stage)
	{
		return feedback->AddNodeDependency(kDefaultNamespace, CreateNodeTypeIDForStage(stage), BuildFileLocation::kIntermediateDir, depsNode->GetIdentifier());
	}

	Result RenderPipelineCompiler::FormatGraphicsPipelineStageFilePath(String &str, const StringView &inPath, render::vulkan::GraphicPipelineStage stage)
	{
		return str.Format("vk_plc_g/%s_%i", inPath.GetChars(), static_cast<int>(stage));
	}

	RenderPipelineStageCompiler::RenderPipelineStageCompiler(PipelineType pipelineType, uint32_t stage)
		: m_pipelineType(pipelineType)
		, m_stageUInt(stage)
	{
	}

	bool RenderPipelineStageCompiler::HasAnalysisStage() const
	{
		return false;
	}

	Result RenderPipelineStageCompiler::RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		return ResultCode::kInternalError;
	}

	Result RenderPipelineStageCompiler::RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		if (m_pipelineType == PipelineType::Graphics)
		{
			UniquePtr<data::IRenderDataPackage> package;
			RKIT_CHECK(LoadPackage(BuildFileLocation::kIntermediateDir, depsNode->GetIdentifier(), true, feedback, package));


			data::IRenderRTTIListBase *list = package->GetIndexable(data::RenderRTTIIndexableStructType::GraphicsPipelineDesc);
			if (list->GetCount() != 1)
			{
				rkit::log::Error("Pipeline package doesn't contain exactly one graphics pipeline");
				return ResultCode::kMalformedFile;
			}

			const render::GraphicsPipelineDesc *pipelineDesc = static_cast<const render::GraphicsPipelineDesc *>(list->GetElementPtr(0));

			RKIT_CHECK(CompileGraphicsPipelineStage(depsNode, feedback, pipelineDesc));

			return ResultCode::kNotYetImplemented;
		}

		return ResultCode::kInternalError;
	}

	Result RenderPipelineStageCompiler::CreatePrivateData(UniquePtr<IDependencyNodePrivateData> &outPrivateData)
	{
		return ResultCode::kOK;
	}

	uint32_t RenderPipelineStageCompiler::GetVersion() const
	{
		return 1;
	}

	Result RenderPipelineStageCompiler::CompileGraphicsPipelineStage(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, const render::GraphicsPipelineDesc *pipeline)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result PipelineCompilerBase::LoadPackage(BuildFileLocation location, const StringView &path, bool allowTempStrings, IDependencyNodeCompilerFeedback *feedback, UniquePtr<data::IRenderDataPackage> &outPackage)
	{
		UniquePtr<ISeekableReadStream> packageStream;
		RKIT_CHECK(feedback->TryOpenInput(location, path, packageStream));

		if (!packageStream.IsValid())
		{
			rkit::log::Error("Failed to open pipeline input");
			return rkit::ResultCode::kFileOpenError;
		}

		data::IDataDriver *dataDriver = nullptr;
		RKIT_CHECK(LoadDataDriver(&dataDriver));

		data::IRenderDataHandler *dataHandler = dataDriver->GetRenderDataHandler();

		return dataHandler->LoadPackage(*packageStream, allowTempStrings, outPackage);
	}

	Result PipelineCompilerBase::LoadDataDriver(data::IDataDriver **outDriver)
	{

		rkit::IModule *dataModule = rkit::GetDrivers().m_moduleDriver->LoadModule(IModuleDriver::kDefaultNamespace, "Data");
		if (!dataModule)
		{
			rkit::log::Error("Couldn't load data module");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		RKIT_CHECK(dataModule->Init(nullptr));

		*outDriver = static_cast<data::IDataDriver *>(rkit::GetDrivers().FindDriver(IModuleDriver::kDefaultNamespace, "Data"));

		return ResultCode::kOK;
	}


	Result CreatePipelineCompiler(UniquePtr<IDependencyNodeCompiler> &outCompiler)
	{
		return New<RenderPipelineCompiler>(outCompiler, PipelineType::Graphics);
	}

	Result CreateGraphicsPipelineStageCompiler(render::vulkan::GraphicPipelineStage stage, UniquePtr<IDependencyNodeCompiler> &outCompiler)
	{
		return New<RenderPipelineStageCompiler>(outCompiler, PipelineType::Graphics, static_cast<uint32_t>(stage));
	}
}
