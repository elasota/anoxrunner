#include "VulkanRenderPipelineCompiler.h"

#include "rkit/BuildSystem/BuildSystem.h"
#include "rkit/BuildSystem/PackageBuilder.h"

#include "rkit/Data/DataDriver.h"
#include "rkit/Data/RenderDataHandler.h"

#include "rkit/Render/RenderDefs.h"

#include "rkit/Vulkan/GraphicsPipeline.h"

#include "rkit/Core/BufferStream.h"
#include "rkit/Core/FourCC.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/Stream.h"

#include "rkit/ShaderC/ShaderC_DLL.h"

#include <glslang/Include/glslang_c_interface.h>

namespace rkit { namespace buildsystem { namespace vulkan
{
	struct GlslCApi;

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


	class PipelineCompilerBase
	{
	protected:
		static Result LoadDataDriver(data::IDataDriver **outDriver);
		static Result LoadPackage(BuildFileLocation location, const CIPathView &path, bool allowTempStrings, IDependencyNodeCompilerFeedback *feedback, UniquePtr<data::IRenderDataPackage> &outPackage, Vector<Vector<uint8_t>> *binaryContent);

		typedef const render::ShaderDesc *(render::GraphicsPipelineDesc:: *GraphicsPipelineShaderField_t);
		static Result GetGraphicsShaderPipelineShaderFieldForStage(render::vulkan::GraphicPipelineStage stage, GraphicsPipelineShaderField_t &outField);

		static Result FormatGraphicsPipelineStageFilePath(CIPath &str, const CIPathView &inPath, render::vulkan::GraphicPipelineStage stage);
	};

	enum class PipelineType
	{
		Graphics,
	};

	class RenderPipelineStageBuildJob final : public PipelineCompilerBase
	{
	public:
		RenderPipelineStageBuildJob(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, const GlslCApi *glslc, PipelineType pipelineType);

		Result AddIncludePath(String &&str);

		Result RunGraphics(const data::IRenderDataPackage *package, const render::GraphicsPipelineDesc *pipeline, render::vulkan::GraphicPipelineStage stage);
		Result Compile();
		Result WriteToFile(IWriteStream &stream);

	private:
		class IncludeResultBase : public glsl_include_result_t, public NoCopy
		{
		public:
			IncludeResultBase();
			virtual ~IncludeResultBase();

			void SetAllocation(const SimpleObjectAllocation<IncludeResultBase> &allocation);
			SimpleObjectAllocation<IncludeResultBase> GetAllocation() const;

		private:
			void *m_baseAddress = nullptr;
			IMallocDriver *m_alloc = nullptr;
		};

		class StaticIncludeResult : public IncludeResultBase
		{
		public:
			StaticIncludeResult(String &&name, const void *data, size_t size);

		private:
			String m_name;
		};

		class DynamicIncludeResult : public IncludeResultBase
		{
		public:
			DynamicIncludeResult(CIPath &&name, Vector<uint8_t> &&buffer);

		private:
			Vector<uint8_t> m_buffer;
			CIPath m_name;
		};

		static Result WriteString(IWriteStream &stream, const rkit::StringSliceView &str);
		static Result WriteUIntString(IWriteStream &stream, uint64_t value);

		static Result WriteRTFormat(IWriteStream &stream, const render::RenderTargetFormat &format);
		static Result WriteVectorOrScalarNumericType(IWriteStream &stream, const render::VectorOrScalarNumericType &format);
		static Result WriteVectorNumericType(IWriteStream &stream, const render::VectorNumericType &format);
		static Result WriteNumericType(IWriteStream &stream, const render::NumericType numericType);
		static Result WriteVectorOrScalarDimension(IWriteStream &stream, const render::VectorOrScalarDimension vectorOrScalarDimension);
		static Result WriteVectorDimension(IWriteStream &stream, const render::VectorDimension vectorDimension);
		static Result WriteTextureDescriptorType(IWriteStream &stream, const render::DescriptorType descriptorType, const render::ValueType &valueType);
		static Result WriteConfigurableRTFormat(IWriteStream &stream, const render::ConfigurableValueBase<render::RenderTargetFormat> &format);

		Result NormalizePath(String &path) const;
		Result TryInclude(CIPath &&path, bool &outSucceeded, UniquePtr<IncludeResultBase> &outIncludeResult) const;
		Result ProcessInclude(const StringView &headerName, const StringView &includerName, size_t includeDepth, bool isSystem, UniquePtr<IncludeResultBase> &outIncludeResult);
		void FreeIncludeResult(glsl_include_result_t *result);

		static glsl_include_result_t *StaticIncludeCallback(void *ctx, const char *header_name, const char *includer_name, size_t include_depth, bool is_system);
		static glsl_include_result_t *StaticIncludeLocalCallback(void *ctx, const char *header_name, const char *includer_name, size_t include_depth);
		static glsl_include_result_t *StaticIncludeSystemCallback(void *ctx, const char *header_name, const char *includer_name, size_t include_depth);
		static int StaticFreeIncludeResultCallback(void *ctx, glsl_include_result_t *result);

		Vector<String> m_includePaths;	// Must be separator-terminated

		IMallocDriver *m_alloc;
		const GlslCApi *m_glslc;

		IDependencyNode *m_depsNode;
		IDependencyNodeCompilerFeedback *m_feedback;
		PipelineType m_pipelineType;

		BufferStream m_prefixStream;
		UniquePtr<IReadWriteStream> m_mainContentStream;
		BufferStream m_suffixStream;
		String m_shaderSourcePath;
		glslang_stage_t m_glslangStage;
		glslang_resource_t m_glslangResource;

		Vector<unsigned int> m_resultSPV;
	};

	class RenderPipelineStageCompiler final : public IDependencyNodeCompiler, public PipelineCompilerBase
	{
	public:
		explicit RenderPipelineStageCompiler(const GlslCApi *glslc, PipelineType pipelineType, uint32_t stage);

		bool HasAnalysisStage() const override;
		Result RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;
		Result RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;

	private:
		RenderPipelineStageCompiler() = delete;

		PipelineType m_pipelineType;
		uint32_t m_stageUInt;
		const GlslCApi *m_glslc;
	};

	class RenderPipelineCompiler final : public IDependencyNodeCompiler, public PipelineCompilerBase
	{
	public:
		explicit RenderPipelineCompiler(PipelineType stage);

		bool HasAnalysisStage() const override;
		Result RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;
		Result RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;

	private:
		class BinaryInclusionIndexer final : public IStringResolver, public NoCopy
		{
		public:
			BinaryInclusionIndexer(const data::IRenderDataPackage *package, const Span<const Vector<uint8_t>> &pkgBinaryContent, const Vector<Vector<uint8_t>> &extraBinaryContent);

			StringSliceView ResolveGlobalString(size_t index) const override;
			StringSliceView ResolveConfigKey(size_t index) const override;
			StringSliceView ResolveTempString(size_t index) const override;
			Span<const uint8_t> ResolveBinaryContent(size_t index) const override;

		private:
			const data::IRenderDataPackage *m_package;
			Span<const Vector<uint8_t>> m_baseBinaryContent;
			const Vector<Vector<uint8_t>> &m_extraBinaryContent;
		};

		RenderPipelineCompiler() = delete;

		static Result CompileGraphicsPipeline(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, data::IRenderDataPackage *package, const Span<const Vector<uint8_t>> &pkgBinaryContent, const render::GraphicsPipelineNameLookup *nameLookup);

		static Result AddGraphicsAnalysisStage(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, render::vulkan::GraphicPipelineStage stage);

		static Result ClearUnusedValues(data::IRenderDataPackage &package);

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
		CIPath depsPath;
		RKIT_CHECK(depsPath.Set(depsNode->GetIdentifier()));

		UniquePtr<data::IRenderDataPackage> package;
		RKIT_CHECK(LoadPackage(BuildFileLocation::kIntermediateDir, depsPath, true, feedback, package, nullptr));

		Vector<uint32_t> stages;

		if (m_pipelineType == PipelineType::Graphics)
		{
			data::IRenderRTTIListBase *list = package->GetIndexable(data::RenderRTTIIndexableStructType::GraphicsPipelineDesc);
			if (list->GetCount() != 1)
			{
				rkit::log::Error(u8"Pipeline package doesn't contain exactly one graphics pipeline");
				RKIT_THROW(ResultCode::kMalformedFile);
			}

			const render::GraphicsPipelineDesc *pipelineDesc = static_cast<const render::GraphicsPipelineDesc *>(list->GetElementPtr(0));

			for (uint32_t stageUInt = 0; stageUInt < static_cast<uint32_t>(render::vulkan::GraphicPipelineStage::Count); stageUInt++)
			{
				render::vulkan::GraphicPipelineStage stage = static_cast<render::vulkan::GraphicPipelineStage>(stageUInt);

				GraphicsPipelineShaderField_t field;
				RKIT_CHECK(GetGraphicsShaderPipelineShaderFieldForStage(stage, field));

				const render::ShaderDesc *shaderPtr = (pipelineDesc->*field);

				if (shaderPtr)
				{
					RKIT_CHECK(AddGraphicsAnalysisStage(depsNode, feedback, stage));
				}
			}
		}
		else
			RKIT_THROW(ResultCode::kInternalError);

		RKIT_RETURN_OK;
	}

	Result RenderPipelineCompiler::RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		CIPath depsPath;
		RKIT_CHECK(depsPath.Set(depsNode->GetIdentifier()));

		Vector<Vector<uint8_t>> binaryContent;

		UniquePtr<data::IRenderDataPackage> package;
		RKIT_CHECK(LoadPackage(BuildFileLocation::kIntermediateDir, depsPath, true, feedback, package, &binaryContent));

		if (m_pipelineType == PipelineType::Graphics)
		{
			data::IRenderRTTIListBase *nameLookupList = package->GetIndexable(data::RenderRTTIIndexableStructType::GraphicsPipelineNameLookup);
			if (nameLookupList->GetCount() != 1)
			{
				rkit::log::Error(u8"Pipeline package doesn't contain exactly one graphics pipeline name lookup");
				RKIT_THROW(ResultCode::kMalformedFile);
			}

			const render::GraphicsPipelineNameLookup *nameLookupDesc = static_cast<const render::GraphicsPipelineNameLookup *>(nameLookupList->GetElementPtr(0));

			RKIT_CHECK(CompileGraphicsPipeline(depsNode, feedback, package.Get(), binaryContent.ToSpan(), nameLookupDesc));
		}
		else
			RKIT_THROW(ResultCode::kInternalError);

		RKIT_CHECK(ClearUnusedValues(*package.Get()));

		RKIT_CHECK(feedback->CheckFault());

		RKIT_RETURN_OK;
	}

	uint32_t RenderPipelineCompiler::GetVersion() const
	{
		return 1;
	}

	Result RenderPipelineCompiler::CompileGraphicsPipeline(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, data::IRenderDataPackage *package, const Span<const Vector<uint8_t>> &pkgBinaryContent, const render::GraphicsPipelineNameLookup *nameLookup)
	{
		render::GraphicsPipelineDesc *pipeline = const_cast<render::GraphicsPipelineDesc *>(nameLookup->m_pipeline);

		size_t numVKStages = static_cast<size_t>(render::vulkan::GraphicPipelineStage::Count);

		Vector<Vector<uint8_t>> binaryContentData;
		Vector<render::BinaryContent> binaryContent;
		Vector<render::ContentKey> contentKeys;
		Vector<const render::ContentKey *> contentKeyPtrs;

		RKIT_CHECK(contentKeys.Resize(numVKStages));
		RKIT_CHECK(contentKeyPtrs.Resize(numVKStages));
		RKIT_CHECK(binaryContent.Resize(numVKStages));
		RKIT_CHECK(binaryContentData.Resize(numVKStages));

		size_t baseContentIndex = package->GetBinaryContentCount();

		for (size_t i = 0; i < numVKStages; i++)
		{
			render::vulkan::GraphicPipelineStage stage = static_cast<render::vulkan::GraphicPipelineStage>(i);

			GraphicsPipelineShaderField_t field;
			RKIT_CHECK(GetGraphicsShaderPipelineShaderFieldForStage(stage, field));

			const render::ShaderDesc *shader = (pipeline->*field);
			if (!shader)
			{
				contentKeyPtrs[i] = nullptr;
			}
			else
			{
				Vector<uint8_t> &stageBinaryData = binaryContentData[i];

				CIPath nodePath;
				RKIT_CHECK(nodePath.Set(depsNode->GetIdentifier()));

				CIPath spvPath;
				RKIT_CHECK(FormatGraphicsPipelineStageFilePath(spvPath, nodePath, stage));

				UniquePtr<ISeekableReadStream> stream;
				RKIT_CHECK(feedback->TryOpenInput(BuildFileLocation::kIntermediateDir, spvPath, stream));

				if (!stream.IsValid())
				{
					rkit::log::ErrorFmt(u8"Failed to open SPV input {}", spvPath.CStr());
					RKIT_THROW(ResultCode::kOperationFailed);
				}

				FilePos_t size = stream->GetSize();
				if (size > std::numeric_limits<size_t>::max())
					RKIT_THROW(ResultCode::kOutOfMemory);

				RKIT_CHECK(stageBinaryData.Resize(static_cast<size_t>(size)));
				if (size > 0)
				{
					RKIT_CHECK(stream->ReadAll(&stageBinaryData[0], static_cast<size_t>(size)));
				}


				binaryContent[i].m_contentIndex = 0;
				contentKeys[i].m_content.m_contentIndex = baseContentIndex + i;
				contentKeyPtrs[i] = &contentKeys[i];
			}
		}

		pipeline->m_compiledContentKeys = contentKeyPtrs.ToSpan();

		rkit::buildsystem::IBuildSystemDriver *bsDriver = static_cast<rkit::buildsystem::IBuildSystemDriver *>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, u8"BuildSystem"));

		data::IDataDriver *dataDriver = nullptr;
		RKIT_CHECK(LoadDataDriver(&dataDriver));

		UniquePtr<IPackageObjectWriter> objectWriter;
		RKIT_CHECK(bsDriver->CreatePackageObjectWriter(objectWriter));

		UniquePtr<IPackageBuilder> packageBuilder;
		RKIT_CHECK(bsDriver->CreatePackageBuilder(dataDriver->GetRenderDataHandler(), objectWriter.Get(), false, packageBuilder));

		CIPath outPath;
		RKIT_CHECK(outPath.Set(GetCompiledPipelineIntermediateBasePath()));

		{
			CIPath pipelinePath;
			RKIT_CHECK(pipelinePath.Set(depsNode->GetIdentifier()));

			RKIT_CHECK(outPath.Append(pipelinePath));
		}

		UniquePtr<ISeekableReadWriteStream> stream;
		RKIT_CHECK(feedback->OpenOutput(BuildFileLocation::kIntermediateDir, outPath, stream));

		BinaryInclusionIndexer indexer(package, pkgBinaryContent, binaryContentData);
		packageBuilder->BeginSource(&indexer);

		size_t index = 0;
		RKIT_CHECK(packageBuilder->IndexObject(nameLookup, dataDriver->GetRenderDataHandler()->GetGraphicsPipelineNameLookupRTTI(), false, index));

		RKIT_CHECK(packageBuilder->WritePackage(*stream));

		RKIT_RETURN_OK;
	}

	Result RenderPipelineCompiler::ClearUnusedValues(data::IRenderDataPackage &package)
	{
		const data::IRenderRTTIListBase *graphicsPipelines = package.GetIndexable(data::RenderRTTIIndexableStructType::GraphicsPipelineDesc);
		for (size_t i = 0; i < graphicsPipelines->GetCount(); i++)
		{
			render::GraphicsPipelineDesc *pipelineDesc = const_cast<render::GraphicsPipelineDesc *>(static_cast<const render::GraphicsPipelineDesc *>(graphicsPipelines->GetElementPtr(i)));
			pipelineDesc->m_vertexShaderOutput = nullptr;
		}

		const data::IRenderRTTIListBase *descriptorDescs = package.GetIndexable(data::RenderRTTIIndexableStructType::DescriptorDesc);
		for (size_t i = 0; i < descriptorDescs->GetCount(); i++)
		{
			render::DescriptorDesc *descriptorDesc = const_cast<render::DescriptorDesc *>(static_cast<const render::DescriptorDesc *>(descriptorDescs->GetElementPtr(i)));
			descriptorDesc->m_globallyCoherent = false;
		}

		RKIT_RETURN_OK;
	}

	Result RenderPipelineCompiler::AddGraphicsAnalysisStage(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, render::vulkan::GraphicPipelineStage stage)
	{
		return feedback->AddNodeDependency(kDefaultNamespace, CreateNodeTypeIDForStage(stage), BuildFileLocation::kIntermediateDir, depsNode->GetIdentifier());
	}

	RenderPipelineCompiler::BinaryInclusionIndexer::BinaryInclusionIndexer(const data::IRenderDataPackage *package, const Span<const Vector<uint8_t>> &baseBinaryContent, const Vector<Vector<uint8_t>> &extraBinaryContent)
		: m_package(package)
		, m_baseBinaryContent(baseBinaryContent)
		, m_extraBinaryContent(extraBinaryContent)
	{
	}

	StringSliceView RenderPipelineCompiler::BinaryInclusionIndexer::ResolveGlobalString(size_t index) const
	{
		return m_package->GetString(index);
	}

	StringSliceView RenderPipelineCompiler::BinaryInclusionIndexer::ResolveConfigKey(size_t index) const
	{
		return m_package->GetString(m_package->GetConfigKey(index).m_stringIndex);
	}

	StringSliceView RenderPipelineCompiler::BinaryInclusionIndexer::ResolveTempString(size_t index) const
	{
		return m_package->GetString(index);
	}

	Span<const uint8_t> RenderPipelineCompiler::BinaryInclusionIndexer::ResolveBinaryContent(size_t index) const
	{
		if (index < m_baseBinaryContent.Count())
			return m_baseBinaryContent[index].ToSpan();

		index -= m_baseBinaryContent.Count();
		return m_extraBinaryContent[index].ToSpan();
	}

	RenderPipelineStageBuildJob::RenderPipelineStageBuildJob(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, const GlslCApi *glslc, PipelineType pipelineType)
		: m_depsNode(depsNode)
		, m_feedback(feedback)
		, m_pipelineType(pipelineType)
		, m_glslangStage(GLSLANG_STAGE_COUNT)
		, m_alloc(GetDrivers().m_mallocDriver)
		, m_glslangResource{}
		, m_glslc(glslc)
	{
	}

	Result RenderPipelineStageBuildJob::RunGraphics(const data::IRenderDataPackage *package, const render::GraphicsPipelineDesc *pipeline, render::vulkan::GraphicPipelineStage stage)
	{
		render::StageVisibility requiredVisibility = render::StageVisibility::All;

		switch (stage)
		{
		case render::vulkan::GraphicPipelineStage::Vertex:
			requiredVisibility = render::StageVisibility::Vertex;
			m_glslangStage = GLSLANG_STAGE_VERTEX;
			break;
		case render::vulkan::GraphicPipelineStage::Pixel:
			requiredVisibility = render::StageVisibility::Pixel;
			m_glslangStage = GLSLANG_STAGE_FRAGMENT;
			break;
		default:
			RKIT_THROW(ResultCode::kInternalError);
		}

		GraphicsPipelineShaderField_t field;
		RKIT_CHECK(GetGraphicsShaderPipelineShaderFieldForStage(stage, field));

		const render::ShaderDesc *shaderDesc = (pipeline->*field);
		render::TempStringIndex_t sourceFileIndex = shaderDesc->m_source;

		const render::RenderPassDesc *rpDesc = pipeline->m_executeInPass;

		RKIT_CHECK(m_shaderSourcePath.Set(package->GetString(sourceFileIndex.GetIndex())));

		RKIT_CHECK(WriteString(m_prefixStream, u8"#define float2 vec2\n"));
		RKIT_CHECK(WriteString(m_prefixStream, u8"#define float3 vec3\n"));
		RKIT_CHECK(WriteString(m_prefixStream, u8"#define float4 vec4\n"));
		RKIT_CHECK(WriteString(m_prefixStream, u8"#define float2x2 mat2x2\n"));
		RKIT_CHECK(WriteString(m_prefixStream, u8"#define float2x3 mat2x3\n"));
		RKIT_CHECK(WriteString(m_prefixStream, u8"#define float2x4 mat2x4\n"));
		RKIT_CHECK(WriteString(m_prefixStream, u8"#define float3x2 mat3x2\n"));
		RKIT_CHECK(WriteString(m_prefixStream, u8"#define float3x3 mat3x3\n"));
		RKIT_CHECK(WriteString(m_prefixStream, u8"#define float3x4 mat3x4\n"));
		RKIT_CHECK(WriteString(m_prefixStream, u8"#define float4x2 mat4x2\n"));
		RKIT_CHECK(WriteString(m_prefixStream, u8"#define float4x3 mat4x3\n"));
		RKIT_CHECK(WriteString(m_prefixStream, u8"#define float4x4 mat4x4\n"));

		if (pipeline->m_pipelineLayout->m_pushConstantList)
		{
			RKIT_THROW(ResultCode::kNotYetImplemented);
		}

		if (stage == render::vulkan::GraphicPipelineStage::Vertex && pipeline->m_inputLayout != nullptr)
		{
			size_t inputIndex = 0;
			for (const render::InputLayoutVertexInputDesc *vertexInput : pipeline->m_inputLayout->m_vertexInputs)
			{
				RKIT_CHECK(WriteString(m_prefixStream, u8"layout(location = "));
				RKIT_CHECK(WriteUIntString(m_prefixStream, inputIndex));
				RKIT_CHECK(WriteString(m_prefixStream, u8") in "));
				RKIT_CHECK(WriteVectorOrScalarNumericType(m_prefixStream, *vertexInput->m_numericType));
				RKIT_CHECK(WriteString(m_prefixStream, u8" _vs_in_F"));
				RKIT_CHECK(WriteUIntString(m_prefixStream, inputIndex));
				RKIT_CHECK(WriteString(m_prefixStream, u8";\n"));

				RKIT_CHECK(WriteVectorOrScalarNumericType(m_prefixStream, *vertexInput->m_numericType));
				RKIT_CHECK(WriteString(m_prefixStream, u8" VertexInput_Load_"));
				RKIT_CHECK(WriteString(m_prefixStream, package->GetString(vertexInput->m_inputFeed->m_feedName.GetIndex())));
				RKIT_CHECK(WriteString(m_prefixStream, u8"_"));
				RKIT_CHECK(WriteString(m_prefixStream, package->GetString(vertexInput->m_memberName.GetIndex())));
				RKIT_CHECK(WriteString(m_prefixStream, u8"()\n"));
				RKIT_CHECK(WriteString(m_prefixStream, u8"{\n"));
				RKIT_CHECK(WriteString(m_prefixStream, u8"    return _vs_in_F"));
				RKIT_CHECK(WriteUIntString(m_prefixStream, inputIndex));
				RKIT_CHECK(WriteString(m_prefixStream, u8";\n"));
				RKIT_CHECK(WriteString(m_prefixStream, u8"}\n"));

				inputIndex++;
			}
		}

		if ((stage == render::vulkan::GraphicPipelineStage::Vertex || stage == render::vulkan::GraphicPipelineStage::Pixel) && pipeline->m_vertexShaderOutput != nullptr)
		{
			RKIT_THROW(ResultCode::kNotYetImplemented);
		}

		if (stage == render::vulkan::GraphicPipelineStage::Pixel)
		{
			for (size_t rtIndex = 0; rtIndex < rpDesc->m_renderTargets.Count(); rtIndex++)
			{
				const render::RenderTargetDesc *rtDesc = rpDesc->m_renderTargets[rtIndex];

				RKIT_CHECK(WriteString(m_prefixStream, u8"layout(location = "));
				RKIT_CHECK(WriteUIntString(m_prefixStream, rtIndex));
				RKIT_CHECK(WriteString(m_prefixStream, u8") out "));

				RKIT_CHECK(WriteConfigurableRTFormat(m_prefixStream, rtDesc->m_format));

				RKIT_CHECK(WriteString(m_prefixStream, u8" _ps_out_RT"));
				RKIT_CHECK(WriteUIntString(m_prefixStream, rtIndex));
				RKIT_CHECK(WriteString(m_prefixStream, u8";\n"));
			}

			RKIT_CHECK(WriteString(m_prefixStream, u8"struct PixelShaderOutput\n"));
			RKIT_CHECK(WriteString(m_prefixStream, u8"{\n"));

			for (size_t rtIndex = 0; rtIndex < rpDesc->m_renderTargets.Count(); rtIndex++)
			{
				const render::RenderTargetDesc *rtDesc = rpDesc->m_renderTargets[rtIndex];

				RKIT_CHECK(WriteString(m_prefixStream, u8"    "));
				RKIT_CHECK(WriteConfigurableRTFormat(m_prefixStream, rtDesc->m_format));
				RKIT_CHECK(WriteString(m_prefixStream, u8" "));
				RKIT_CHECK(WriteString(m_prefixStream, package->GetString(rtDesc->m_name.GetIndex())));
				RKIT_CHECK(WriteString(m_prefixStream, u8";\n"));
			}

			RKIT_CHECK(WriteString(m_prefixStream, u8"};\n"));
		}

		ConstSpan<const render::DescriptorLayoutDesc *> descriptorLayouts = pipeline->m_pipelineLayout->m_descriptorLayouts;
		for (size_t dlSlot = 0; dlSlot < descriptorLayouts.Count(); dlSlot++)
		{
			const render::DescriptorLayoutDesc *dl = descriptorLayouts[dlSlot];

			for (size_t descSlot = 0; descSlot < dl->m_descriptors.Count(); descSlot++)
			{
				const render::DescriptorDesc *descriptor = dl->m_descriptors[descSlot];

				if (descriptor->m_visibility != render::StageVisibility::All && descriptor->m_visibility != requiredVisibility)
					continue;

				RKIT_CHECK(WriteString(m_prefixStream, u8"layout(set = "));
				RKIT_CHECK(WriteUIntString(m_prefixStream, dlSlot));
				RKIT_CHECK(WriteString(m_prefixStream, u8", binding = "));
				RKIT_CHECK(WriteUIntString(m_prefixStream, descSlot));

				switch (descriptor->m_descriptorType)
				{
				case render::DescriptorType::StaticConstantBuffer:
				case render::DescriptorType::DynamicConstantBuffer:
					RKIT_CHECK(WriteString(m_prefixStream, u8", std140"));
					break;
				default:
					break;
				}

				RKIT_CHECK(WriteString(m_prefixStream, u8") "));

				switch (descriptor->m_descriptorType)
				{
				case render::DescriptorType::Sampler:
					RKIT_CHECK(WriteString(m_prefixStream, u8"sampler "));
					break;

				case render::DescriptorType::StaticConstantBuffer:
				case render::DescriptorType::DynamicConstantBuffer:
					RKIT_CHECK(WriteString(m_prefixStream, u8"uniform "));
					break;

				case render::DescriptorType::Buffer:
				case render::DescriptorType::ByteAddressBuffer:
					RKIT_CHECK(WriteString(m_prefixStream, u8"readonly buffer "));
					break;

				case render::DescriptorType::RWBuffer:
				case render::DescriptorType::RWByteAddressBuffer:
					RKIT_CHECK(WriteString(m_prefixStream, u8"buffer "));
					break;

				case render::DescriptorType::Texture1D:
				case render::DescriptorType::Texture2D:
				case render::DescriptorType::Texture2DArray:
				case render::DescriptorType::Texture2DMS:
				case render::DescriptorType::Texture2DMSArray:
				case render::DescriptorType::Texture3D:
				case render::DescriptorType::TextureCube:
				case render::DescriptorType::TextureCubeArray:
				case render::DescriptorType::RWTexture1D:
				case render::DescriptorType::RWTexture1DArray:
				case render::DescriptorType::RWTexture2D:
				case render::DescriptorType::RWTexture2DArray:
				case render::DescriptorType::RWTexture3D:
					RKIT_CHECK(WriteString(m_prefixStream, u8"uniform "));
					RKIT_CHECK(WriteTextureDescriptorType(m_prefixStream, descriptor->m_descriptorType, descriptor->m_valueType));
					RKIT_CHECK(WriteString(m_prefixStream, u8" "));
					break;
				}

				RKIT_CHECK(WriteString(m_prefixStream, package->GetString(descriptor->m_name.GetIndex())));
				RKIT_CHECK(WriteString(m_prefixStream, u8";\n"));
			}
		}

		RKIT_CHECK(WriteString(m_suffixStream, u8"void main()\n"));
		RKIT_CHECK(WriteString(m_suffixStream, u8"{\n"));

		if (stage == render::vulkan::GraphicPipelineStage::Vertex)
		{
			RKIT_CHECK(WriteString(m_suffixStream, u8"    vec4 vPosition;\n"));
		}

		if (stage == render::vulkan::GraphicPipelineStage::Pixel)
		{
			RKIT_CHECK(WriteString(m_prefixStream, u8"PixelShaderOutput pOutput;\n"));
		}

		// Call entry point
		RKIT_CHECK(WriteString(m_suffixStream, u8"    "));
		RKIT_CHECK(WriteString(m_suffixStream, package->GetString(shaderDesc->m_entryPoint.GetIndex())));
		RKIT_CHECK(WriteString(m_suffixStream, u8"("));

		if (stage == render::vulkan::GraphicPipelineStage::Vertex)
		{
			RKIT_CHECK(WriteString(m_suffixStream, u8"vPosition"));
		}

		if (stage == render::vulkan::GraphicPipelineStage::Pixel)
		{
			RKIT_CHECK(WriteString(m_suffixStream, u8"pOutput"));
		}

		RKIT_CHECK(WriteString(m_suffixStream, u8");\n"));

		// Suffixes
		if (stage == render::vulkan::GraphicPipelineStage::Vertex)
		{
			RKIT_CHECK(WriteString(m_suffixStream, u8"    gl_Position = vPosition;\n"));
		}

		if (stage == render::vulkan::GraphicPipelineStage::Pixel)
		{
			for (size_t rtIndex = 0; rtIndex < rpDesc->m_renderTargets.Count(); rtIndex++)
			{
				const render::RenderTargetDesc *rtDesc = rpDesc->m_renderTargets[rtIndex];

				RKIT_CHECK(WriteString(m_suffixStream, u8"    _ps_out_RT"));
				RKIT_CHECK(WriteUIntString(m_suffixStream, rtIndex));
				RKIT_CHECK(WriteString(m_suffixStream, u8" = pOutput."));
				RKIT_CHECK(WriteString(m_suffixStream, package->GetString(rtDesc->m_name.GetIndex())));
				RKIT_CHECK(WriteString(m_suffixStream, u8";\n"));
			}
		}

		RKIT_CHECK(WriteString(m_suffixStream, u8"}\n"));

		if (rpDesc->m_renderTargets.Count() > static_cast<size_t>(std::numeric_limits<int>::max()))
			RKIT_THROW(ResultCode::kIntegerOverflow);

		m_glslangResource.max_draw_buffers = static_cast<int>(rpDesc->m_renderTargets.Count());

		return Compile();
	}

	Result RenderPipelineStageBuildJob::Compile()
	{
		String mainShaderContent;
		RKIT_CHECK(mainShaderContent.Append(u8"#extension GL_ARB_shading_language_include : enable\n"
			u8"#include <GlslShaderPrefix>\n"
			u8"#include \"./"));
		RKIT_CHECK(mainShaderContent.Append(m_shaderSourcePath));
		RKIT_CHECK(mainShaderContent.Append(u8"\"\n"
			u8"#include <GlslShaderSuffix>\n"));

		glslang_input_t input = {};
		input.language = GLSLANG_SOURCE_GLSL;
		input.stage = m_glslangStage;
		input.client = GLSLANG_CLIENT_VULKAN;
		input.client_version = GLSLANG_TARGET_VULKAN_1_0;
		input.target_language = GLSLANG_TARGET_SPV;
		input.target_language_version = GLSLANG_TARGET_SPV_1_0;
		input.code = ReinterpretUtf8CharToAnsiChar(mainShaderContent.CStr());
		input.default_version = 450;
		input.default_profile = GLSLANG_CORE_PROFILE;
		input.messages = GLSLANG_MSG_DEFAULT_BIT;
		input.callbacks.free_include_result = StaticFreeIncludeResultCallback;
		input.callbacks.include_local = StaticIncludeLocalCallback;
		input.callbacks.include_system = StaticIncludeSystemCallback;
		input.callbacks_ctx = this;
		input.resource = &m_glslangResource;

		glslang_shader_t *shader = m_glslc->glslang_shader_create(&input);
		if (!shader)
			RKIT_THROW(ResultCode::kOperationFailed);

		if (!m_glslc->glslang_shader_preprocess(shader, &input) || !m_glslc->glslang_shader_parse(shader, &input))
		{
			const char *infoLog = m_glslc->glslang_shader_get_info_log(shader);
			const char *infoDebugLog = m_glslc->glslang_shader_get_info_debug_log(shader);

			if (infoLog && infoLog[0])
				rkit::log::Error(StringView::FromCString(ReinterpretAnsiCharToUtf8Char(infoLog)));

			if (infoDebugLog && infoDebugLog[0])
				rkit::log::Error(StringView::FromCString(ReinterpretAnsiCharToUtf8Char(infoDebugLog)));

			m_glslc->glslang_shader_delete(shader);
			RKIT_THROW(ResultCode::kOperationFailed);
		}

		glslang_program_t *program = m_glslc->glslang_program_create();
		if (!program)
		{
			m_glslc->glslang_shader_delete(shader);
			RKIT_THROW(ResultCode::kOperationFailed);
		}

		m_glslc->glslang_program_add_shader(program, shader);

		if (!m_glslc->glslang_program_link(program, GLSLANG_MSG_DEFAULT_BIT))
		{
			const char *infoLog = m_glslc->glslang_program_get_info_log(program);
			const char *infoDebugLog = m_glslc->glslang_program_get_info_debug_log(program);

			if (infoLog && infoLog[0])
				rkit::log::Error(StringView::FromCString(ReinterpretAnsiCharToUtf8Char(infoLog)));

			if (infoDebugLog && infoDebugLog[0])
				rkit::log::Error(StringView::FromCString(ReinterpretAnsiCharToUtf8Char(infoDebugLog)));

			m_glslc->glslang_program_delete(program);
			m_glslc->glslang_shader_delete(shader);
			RKIT_THROW(ResultCode::kOperationFailed);
		}

		m_glslc->glslang_program_SPIRV_generate(program, m_glslangStage);

		size_t spvSize = m_glslc->glslang_program_SPIRV_get_size(program);

		RKIT_CHECK(m_resultSPV.Resize(spvSize));
		if (spvSize > 0)
			m_glslc->glslang_program_SPIRV_get(program, &m_resultSPV[0]);

		const char *spvMessages = m_glslc->glslang_program_SPIRV_get_messages(program);
		if (spvMessages && spvMessages[0])
			rkit::log::LogInfo(StringView::FromCString(ReinterpretAnsiCharToUtf8Char(spvMessages)));

		m_glslc->glslang_program_delete(program);
		m_glslc->glslang_shader_delete(shader);

		RKIT_RETURN_OK;
	}

	Result RenderPipelineStageBuildJob::WriteToFile(IWriteStream &stream)
	{
		Vector<uint8_t> leDWords;
		RKIT_CHECK(leDWords.Resize(m_resultSPV.Count() * 4));

		uint8_t *bytes = leDWords.GetBuffer();

		for (uint32_t dword : m_resultSPV)
		{
			bytes[0] = static_cast<uint8_t>((dword >> 0) & 0xffu);
			bytes[1] = static_cast<uint8_t>((dword >> 8) & 0xffu);
			bytes[2] = static_cast<uint8_t>((dword >> 16) & 0xffu);
			bytes[3] = static_cast<uint8_t>((dword >> 24) & 0xffu);

			bytes += 4;
		}

		return stream.WriteAll(leDWords.GetBuffer(), leDWords.Count());
	}

	Result RenderPipelineStageBuildJob::WriteString(IWriteStream &stream, const rkit::StringSliceView &str)
	{
		Span<const Utf8Char_t> span = str.ToSpan();
		if (span.Count() == 0)
			RKIT_RETURN_OK;

		return stream.WriteAll(span.Ptr(), span.Count());
	}

	Result RenderPipelineStageBuildJob::WriteUIntString(IWriteStream &stream, uint64_t value)
	{
		const uint32_t kMaxDigits = 20;

		Utf8Char_t strBuffer[kMaxDigits + 1];
		strBuffer[kMaxDigits] = '\0';

		size_t numDigits = 0;
		do
		{
			numDigits++;
			strBuffer[kMaxDigits - numDigits] = static_cast<Utf8Char_t>('0' + (value % 10u));
			value /= 10u;
		} while (value != 0u);

		return WriteString(stream, StringView(strBuffer + (kMaxDigits - numDigits), numDigits));
	}

	Result RenderPipelineStageBuildJob::WriteRTFormat(IWriteStream &stream, const render::RenderTargetFormat &format)
	{
		RKIT_THROW(ResultCode::kNotYetImplemented);
	}

	Result RenderPipelineStageBuildJob::WriteVectorOrScalarNumericType(IWriteStream &stream, const render::VectorOrScalarNumericType &format)
	{
		RKIT_CHECK(WriteNumericType(stream, format.m_numericType));
		RKIT_CHECK(WriteVectorOrScalarDimension(stream, format.m_cols));

		RKIT_RETURN_OK;
	}

	Result RenderPipelineStageBuildJob::WriteVectorNumericType(IWriteStream &stream, const render::VectorNumericType &format)
	{
		RKIT_CHECK(WriteNumericType(stream, format.m_numericType));
		RKIT_CHECK(WriteVectorDimension(stream, format.m_cols));

		RKIT_RETURN_OK;
	}

	Result RenderPipelineStageBuildJob::WriteNumericType(IWriteStream &stream, const render::NumericType numericType)
	{
		switch (numericType)
		{
		case render::NumericType::Bool:
			return WriteString(stream, u8"bool");
		case render::NumericType::Float16:
			return WriteString(stream, u8"half");
		case render::NumericType::Float32:
			return WriteString(stream, u8"float");
		case render::NumericType::Float64:
			return WriteString(stream, u8"double");
		case render::NumericType::SInt8:
			return WriteString(stream, u8"sbyte");
		case render::NumericType::SInt16:
			return WriteString(stream, u8"short");
		case render::NumericType::SInt32:
			return WriteString(stream, u8"int");
		case render::NumericType::SInt64:
			return WriteString(stream, u8"long");
		case render::NumericType::UInt8:
			return WriteString(stream, u8"byte");
		case render::NumericType::UInt16:
			return WriteString(stream, u8"ushort");
		case render::NumericType::UInt32:
			return WriteString(stream, u8"uint");
		case render::NumericType::UInt64:
			return WriteString(stream, u8"ulong");
		default:
			rkit::log::Error(u8"A numeric type could not be written in the desired context");
			RKIT_THROW(ResultCode::kOperationFailed);
		}
	}

	Result RenderPipelineStageBuildJob::WriteVectorOrScalarDimension(IWriteStream &stream, const render::VectorOrScalarDimension vectorOrScalarDimension)
	{
		switch (vectorOrScalarDimension)
		{
		case render::VectorOrScalarDimension::Scalar:
			RKIT_RETURN_OK;
		case render::VectorOrScalarDimension::Dimension2:
			return WriteString(stream, u8"2");
		case render::VectorOrScalarDimension::Dimension3:
			return WriteString(stream, u8"3");
		case render::VectorOrScalarDimension::Dimension4:
			return WriteString(stream, u8"4");
		default:
			RKIT_THROW(ResultCode::kInternalError);
		}
	}

	Result RenderPipelineStageBuildJob::WriteVectorDimension(IWriteStream &stream, const render::VectorDimension vectorDimension)
	{
		switch (vectorDimension)
		{
		case render::VectorDimension::Dimension2:
			return WriteString(stream, u8"2");
		case render::VectorDimension::Dimension3:
			return WriteString(stream, u8"3");
		case render::VectorDimension::Dimension4:
			return WriteString(stream, u8"4");
		default:
			RKIT_THROW(ResultCode::kInternalError);
		}
	}

	Result RenderPipelineStageBuildJob::WriteConfigurableRTFormat(IWriteStream &stream, const render::ConfigurableValueBase<render::RenderTargetFormat> &format)
	{
		if (format.m_state == render::ConfigurableValueState::Explicit)
			return WriteRTFormat(stream, format.m_u.m_value);
		else
			return WriteString(stream, u8"float4");
	}

	Result RenderPipelineStageBuildJob::WriteTextureDescriptorType(IWriteStream &stream, const render::DescriptorType descriptorType, const render::ValueType &valueType)
	{
		if (valueType.m_type == render::ValueTypeType::VectorNumeric)
			return WriteTextureDescriptorType(stream, descriptorType, render::ValueType(valueType.m_value.m_vectorNumericType->m_numericType));

		if (valueType.m_type != render::ValueTypeType::Numeric)
		{
			rkit::log::Error(u8"Texture descriptor type was non-numeric");
			RKIT_THROW(ResultCode::kOperationFailed);
		}

		const render::NumericType numericType = valueType.m_value.m_numericType;

		switch (numericType)
		{
		case render::NumericType::SInt8:
		case render::NumericType::SInt16:
		case render::NumericType::SInt32:
		case render::NumericType::SInt64:
			RKIT_CHECK(WriteString(stream, u8"i"));
			break;

		case render::NumericType::UInt8:
		case render::NumericType::UInt16:
		case render::NumericType::UInt32:
		case render::NumericType::UInt64:
			RKIT_CHECK(WriteString(stream, u8"u"));
			break;

		default:
			break;
		}

		RKIT_CHECK(WriteString(stream, u8"texture"));

		switch (descriptorType)
		{
		case render::DescriptorType::Texture1D:
		case render::DescriptorType::Texture1DArray:
		case render::DescriptorType::RWTexture1D:
		case render::DescriptorType::RWTexture1DArray:
			RKIT_CHECK(WriteString(stream, u8"1D"));
			break;

		case render::DescriptorType::Texture2D:
		case render::DescriptorType::Texture2DArray:
		case render::DescriptorType::Texture2DMS:
		case render::DescriptorType::Texture2DMSArray:
		case render::DescriptorType::RWTexture2D:
		case render::DescriptorType::RWTexture2DArray:
			RKIT_CHECK(WriteString(stream, u8"2D"));
			break;

		case render::DescriptorType::Texture3D:
		case render::DescriptorType::RWTexture3D:
			RKIT_CHECK(WriteString(stream, u8"3D"));
			break;

		case render::DescriptorType::TextureCube:
		case render::DescriptorType::TextureCubeArray:
			RKIT_CHECK(WriteString(stream, u8"Cube"));
			break;

		default:
			RKIT_THROW(ResultCode::kInternalError);
		}

		switch (descriptorType)
		{
		case render::DescriptorType::Texture1DArray:
		case render::DescriptorType::RWTexture1DArray:
		case render::DescriptorType::Texture2DArray:
		case render::DescriptorType::TextureCubeArray:
			RKIT_CHECK(WriteString(stream, u8"Array"));
			break;

		case render::DescriptorType::Texture2DMSArray:
		case render::DescriptorType::RWTexture2DArray:
			RKIT_CHECK(WriteString(stream, u8"MSArray"));
			break;

		case render::DescriptorType::Texture2DMS:
			RKIT_CHECK(WriteString(stream, u8"MS"));
			break;

		case render::DescriptorType::Texture1D:
		case render::DescriptorType::RWTexture1D:
		case render::DescriptorType::Texture2D:
		case render::DescriptorType::RWTexture2D:
		case render::DescriptorType::Texture3D:
		case render::DescriptorType::RWTexture3D:
		case render::DescriptorType::TextureCube:
			break;

		default:
			RKIT_THROW(ResultCode::kInternalError);
		}

		RKIT_RETURN_OK;
	}


	Result RenderPipelineStageBuildJob::NormalizePath(String &path) const
	{
		String fullPath;

		size_t sliceStart = 0;
		for (size_t i = 0; i <= path.Length(); i++)
		{
			char c = '\0';

			if (i != path.Length())
			{
				c = path[i];
				if (c != '/' && c != '.' && c != '_' && !(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9'))
					RKIT_THROW(ResultCode::kOperationFailed);
			}

			if (i == path.Length() || c == '/')
			{
				StringSliceView slice = path.SubString(sliceStart, i - sliceStart);
				sliceStart = i + 1;

				if (slice.Length() == 0 || slice == u8".")
					RKIT_THROW(ResultCode::kOperationFailed);

				if (slice == u8"..")
				{
					size_t lastSlashPos = 0;
					for (size_t j = 0; j < path.Length(); i++)
					{
						if (path[j] == '/')
							lastSlashPos = j;
					}

					if (lastSlashPos == 0)
						RKIT_THROW(ResultCode::kOperationFailed);

					RKIT_CHECK(fullPath.Set(fullPath.SubString(0, lastSlashPos + 1)));
				}
				else
				{
					if (fullPath.Length() > 0)
					{
						RKIT_CHECK(fullPath.Append(u8"/"));
					}
					RKIT_CHECK(fullPath.Append(slice));
				}
			}
		}

		if (fullPath.Length() == 0 || !GetDrivers().m_utilitiesDriver->ValidateFilePath(fullPath.ToSpan(), false))
			RKIT_THROW(ResultCode::kOperationFailed);

		path = std::move(fullPath);
		RKIT_RETURN_OK;
	}

	Result RenderPipelineStageBuildJob::TryInclude(CIPath &&path, bool &outSucceeded, UniquePtr<IncludeResultBase> &outIncludeResult) const
	{
		outSucceeded = false;
		outIncludeResult.Reset();

		CIPath fullPath;
		RKIT_CHECK(fullPath.Set(buildsystem::GetShaderSourceBasePath()));
		RKIT_CHECK(fullPath.Append(path));

		UniquePtr<ISeekableReadStream> stream;
		RKIT_CHECK(m_feedback->TryOpenInput(BuildFileLocation::kSourceDir, fullPath, stream));

		if (!stream.IsValid())
			RKIT_RETURN_OK;

		FilePos_t size = static_cast<size_t>(stream->GetSize());
		if (stream->GetSize() > std::numeric_limits<size_t>::max())
			RKIT_THROW(ResultCode::kOutOfMemory);

		Vector<uint8_t> contents;
		RKIT_CHECK(contents.Resize(static_cast<size_t>(size)));

		if (size > 0)
		{
			RKIT_CHECK(stream->ReadAll(&contents[0], static_cast<size_t>(size)));
		}

		stream.Reset();

		RKIT_CHECK(New<DynamicIncludeResult>(outIncludeResult, std::move(path), std::move(contents)));

		outSucceeded = true;
		RKIT_RETURN_OK;
	}

	Result RenderPipelineStageBuildJob::ProcessInclude(const StringView &headerName, const StringView &includerName, size_t includeDepth, bool isSystem, UniquePtr<IncludeResultBase> &outIncludeResult)
	{
		if (isSystem)
		{
			if (headerName == u8"GlslShaderPrefix")
			{
				const Vector<uint8_t> &prefixVector = m_prefixStream.GetBuffer();

				String headerNameStr;
				RKIT_CHECK(headerNameStr.Set(headerName));

				return New<StaticIncludeResult>(outIncludeResult, std::move(headerNameStr), prefixVector.GetBuffer(), prefixVector.Count());
			}
			if (headerName == u8"GlslShaderSuffix")
			{
				const Vector<uint8_t> &prefixVector = m_suffixStream.GetBuffer();

				String headerNameStr;
				RKIT_CHECK(headerNameStr.Set(headerName));

				return New<StaticIncludeResult>(outIncludeResult, std::move(headerNameStr), prefixVector.GetBuffer(), prefixVector.Count());
			}

			RKIT_THROW(ResultCode::kOperationFailed);
		}

		bool isAbsolutePath = false;
		bool shouldTryIncludePaths = true;
		bool shouldTryRelativePath = true;

		size_t sliceStart = 0;
		size_t scanPos = 0;
		size_t numSlices = 0;
		size_t numBacksteps = 0;

		while (scanPos <= headerName.Length())
		{
			if (scanPos == headerName.Length() || headerName[scanPos] == '/')
			{
				StringSliceView slice = StringSliceView(headerName.SubString(sliceStart, scanPos - sliceStart));
				sliceStart = scanPos + 1;

				if (slice == u8".")
				{
					if (numSlices != 0)
						RKIT_THROW(ResultCode::kOperationFailed);

					isAbsolutePath = true;
				}

				numSlices++;
			}

			scanPos++;
		}

		if (isAbsolutePath)
		{
			StringSliceView headerAbsPathStr = headerName.SubString(2);

			// FIXME: Use Path stuff
			String normalizedPathStr;
			RKIT_CHECK(normalizedPathStr.Set(headerAbsPathStr));
			RKIT_CHECK(NormalizePath(normalizedPathStr));

			CIPath normalizedPath;
			RKIT_CHECK(normalizedPath.Set(normalizedPathStr));

			bool succeeded = false;
			RKIT_CHECK(TryInclude(std::move(normalizedPath), succeeded, outIncludeResult));

			if (succeeded)
				RKIT_RETURN_OK;
		}
		else
		{
			if (shouldTryRelativePath)
			{
				size_t basePathLength = 0;
				for (size_t i = 0; i < includerName.Length(); i++)
				{
					if (includerName[i] == '/')
						basePathLength = i + 1;
				}

				String pathStr;
				RKIT_CHECK(pathStr.Set(includerName.SubString(0, basePathLength)));
				RKIT_CHECK(pathStr.Append(headerName));
				RKIT_CHECK(NormalizePath(pathStr));

				CIPath path;
				RKIT_CHECK(path.Set(pathStr));

				bool succeeded = false;
				RKIT_CHECK(TryInclude(std::move(path), succeeded, outIncludeResult));

				if (succeeded)
					RKIT_RETURN_OK;
			}

			if (shouldTryIncludePaths)
			{
				String normalizedHeaderName;
				RKIT_CHECK(normalizedHeaderName.Set(headerName));
				RKIT_CHECK(NormalizePath(normalizedHeaderName));

				for (const String &includePath : m_includePaths)
				{
					String fullPathStr = includePath;
					RKIT_CHECK(fullPathStr.Append(normalizedHeaderName));

					CIPath fullPath;
					RKIT_CHECK(fullPath.Set(fullPathStr));

					bool succeeded = false;
					RKIT_CHECK(TryInclude(std::move(fullPath), succeeded, outIncludeResult));

					if (succeeded)
						RKIT_RETURN_OK;
				}
			}
		}

		RKIT_THROW(ResultCode::kOperationFailed);
	}

	void RenderPipelineStageBuildJob::FreeIncludeResult(glsl_include_result_t *result)
	{
		if (result)
		{
			IncludeResultBase *includeResult = static_cast<IncludeResultBase *>(result);

			SimpleObjectAllocation<IncludeResultBase> allocation = includeResult->GetAllocation();
			Delete(allocation);
		}
	}

	glsl_include_result_t *RenderPipelineStageBuildJob::StaticIncludeCallback(void *ctx, const char *header_name, const char *includer_name, size_t include_depth, bool is_system)
	{
		RenderPipelineStageBuildJob *job = static_cast<RenderPipelineStageBuildJob *>(ctx);

		UniquePtr<IncludeResultBase> includeResult;
		PackedResultAndExtCode result = RKIT_TRY_EVAL(job->ProcessInclude(StringView::FromCString(ReinterpretAnsiCharToUtf8Char(header_name)), StringView::FromCString(ReinterpretAnsiCharToUtf8Char(includer_name)), include_depth, is_system, includeResult));

		if (!utils::ResultIsOK(result))
			return nullptr;

		SimpleObjectAllocation<IncludeResultBase> allocation = includeResult.Detach();
		allocation.m_obj->SetAllocation(allocation);

		return allocation.m_obj;
	}

	glsl_include_result_t *RenderPipelineStageBuildJob::StaticIncludeLocalCallback(void *ctx, const char *header_name, const char *includer_name, size_t include_depth)
	{
		return StaticIncludeCallback(ctx, header_name, includer_name, include_depth, false);
	}

	glsl_include_result_t *RenderPipelineStageBuildJob::StaticIncludeSystemCallback(void *ctx, const char *header_name, const char *includer_name, size_t include_depth)
	{
		return StaticIncludeCallback(ctx, header_name, includer_name, include_depth, true);
	}

	int RenderPipelineStageBuildJob::StaticFreeIncludeResultCallback(void *ctx, glsl_include_result_t *result)
	{
		RenderPipelineStageBuildJob *job = static_cast<RenderPipelineStageBuildJob *>(ctx);
		job->FreeIncludeResult(result);

		return 1;
	}


	RenderPipelineStageBuildJob::IncludeResultBase::IncludeResultBase()
	{
		glsl_include_result_t *includeResult = this;
		includeResult->header_data = nullptr;
		includeResult->header_length = 0;
		includeResult->header_name = nullptr;
	}

	RenderPipelineStageBuildJob::IncludeResultBase::~IncludeResultBase()
	{
	}

	void RenderPipelineStageBuildJob::IncludeResultBase::SetAllocation(const SimpleObjectAllocation<IncludeResultBase> &allocation)
	{
		m_alloc = allocation.m_alloc;
		m_baseAddress = allocation.m_mem;
	}

	SimpleObjectAllocation<RenderPipelineStageBuildJob::IncludeResultBase> RenderPipelineStageBuildJob::IncludeResultBase::GetAllocation() const
	{
		SimpleObjectAllocation<IncludeResultBase> result;
		result.m_alloc = m_alloc;
		result.m_mem = m_baseAddress;
		result.m_obj = const_cast<IncludeResultBase*>(this);
		return result;
	}

	RenderPipelineStageBuildJob::StaticIncludeResult::StaticIncludeResult(String &&name, const void *data, size_t size)
	{
		m_name = std::move(name);

		header_data = static_cast<const char *>(data);
		header_length = size;
		header_name = ReinterpretUtf8CharToAnsiChar(m_name.CStr());
	}

	RenderPipelineStageBuildJob::DynamicIncludeResult::DynamicIncludeResult(CIPath &&name, Vector<uint8_t> &&data)
	{
		m_name = std::move(name);
		m_buffer = std::move(data);

		header_data = reinterpret_cast<const char *>(m_buffer.GetBuffer());
		header_length = m_buffer.Count();
		header_name = ReinterpretUtf8CharToAnsiChar(m_name.CStr());
	}

	RenderPipelineStageCompiler::RenderPipelineStageCompiler(const GlslCApi *glslc, PipelineType pipelineType, uint32_t stage)
		: m_pipelineType(pipelineType)
		, m_stageUInt(stage)
		, m_glslc(glslc)
	{
	}

	bool RenderPipelineStageCompiler::HasAnalysisStage() const
	{
		return false;
	}

	Result RenderPipelineStageCompiler::RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		RKIT_THROW(ResultCode::kInternalError);
	}

	Result RenderPipelineStageCompiler::RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		if (m_pipelineType == PipelineType::Graphics)
		{
			CIPath nodePath;
			RKIT_CHECK(nodePath.Set(depsNode->GetIdentifier()));

			UniquePtr<data::IRenderDataPackage> package;
			RKIT_CHECK(LoadPackage(BuildFileLocation::kIntermediateDir, nodePath, true, feedback, package, nullptr));

			data::IRenderRTTIListBase *list = package->GetIndexable(data::RenderRTTIIndexableStructType::GraphicsPipelineDesc);
			if (list->GetCount() != 1)
			{
				rkit::log::Error(u8"Pipeline package doesn't contain exactly one graphics pipeline");
				RKIT_THROW(ResultCode::kMalformedFile);
			}

			const render::GraphicsPipelineDesc *pipelineDesc = static_cast<const render::GraphicsPipelineDesc *>(list->GetElementPtr(0));

			render::vulkan::GraphicPipelineStage stage = static_cast<render::vulkan::GraphicPipelineStage>(m_stageUInt);

			RenderPipelineStageBuildJob buildJob(depsNode, feedback, m_glslc, m_pipelineType);
			RKIT_CHECK(buildJob.RunGraphics(package.Get(), pipelineDesc, stage));

			CIPath outPath;
			RKIT_CHECK(FormatGraphicsPipelineStageFilePath(outPath, nodePath, stage));

			UniquePtr<ISeekableReadWriteStream> outStream;
			RKIT_CHECK(feedback->OpenOutput(BuildFileLocation::kIntermediateDir, outPath, outStream));

			RKIT_CHECK(buildJob.WriteToFile(*outStream));

			RKIT_RETURN_OK;
		}

		RKIT_THROW(ResultCode::kInternalError);
	}

	uint32_t RenderPipelineStageCompiler::GetVersion() const
	{
		return 1;
	}

	Result PipelineCompilerBase::LoadPackage(BuildFileLocation location, const CIPathView &path, bool allowTempStrings, IDependencyNodeCompilerFeedback *feedback, UniquePtr<data::IRenderDataPackage> &outPackage, Vector<Vector<uint8_t>> *binaryContent)
	{
		UniquePtr<ISeekableReadStream> packageStream;
		RKIT_CHECK(feedback->TryOpenInput(location, path, packageStream));

		if (!packageStream.IsValid())
		{
			rkit::log::Error(u8"Failed to open pipeline input");
			RKIT_THROW(ResultCode::kFileOpenError);
		}

		data::IDataDriver *dataDriver = nullptr;
		RKIT_CHECK(LoadDataDriver(&dataDriver));

		data::IRenderDataHandler *dataHandler = dataDriver->GetRenderDataHandler();

		RKIT_CHECK(dataHandler->LoadPackage(*packageStream, allowTempStrings, nullptr, outPackage, binaryContent));

		RKIT_RETURN_OK;
	}

	Result PipelineCompilerBase::GetGraphicsShaderPipelineShaderFieldForStage(render::vulkan::GraphicPipelineStage stage, GraphicsPipelineShaderField_t &outField)
	{
		switch (stage)
		{
		case render::vulkan::GraphicPipelineStage::Vertex:
			outField = &render::GraphicsPipelineDesc::m_vertexShader;
			break;
		case render::vulkan::GraphicPipelineStage::Pixel:
			outField = &render::GraphicsPipelineDesc::m_pixelShader;
			break;

		default:
			RKIT_THROW(ResultCode::kInternalError);
		}

		RKIT_RETURN_OK;
	}

	Result PipelineCompilerBase::FormatGraphicsPipelineStageFilePath(CIPath &path, const CIPathView &inPath, render::vulkan::GraphicPipelineStage stage)
	{
		String str;
		RKIT_CHECK(str.Format(u8"vk_pl_g_{}/{}", static_cast<int>(stage), inPath.GetChars()));

		RKIT_CHECK(path.Set(str));

		RKIT_RETURN_OK;
	}

	Result PipelineCompilerBase::LoadDataDriver(data::IDataDriver **outDriver)
	{

		rkit::IModule *dataModule = rkit::GetDrivers().m_moduleDriver->LoadModule(IModuleDriver::kDefaultNamespace, u8"Data");
		if (!dataModule)
		{
			rkit::log::Error(u8"Couldn't load data module");
			RKIT_THROW(ResultCode::kModuleLoadFailed);
		}

		*outDriver = static_cast<data::IDataDriver *>(rkit::GetDrivers().FindDriver(IModuleDriver::kDefaultNamespace, u8"Data"));

		RKIT_RETURN_OK;
	}


	Result CreatePipelineCompiler(UniquePtr<IDependencyNodeCompiler> &outCompiler)
	{
		return New<RenderPipelineCompiler>(outCompiler, PipelineType::Graphics);
	}

	Result CreateGraphicsPipelineStageCompiler(const GlslCApi *glslc, render::vulkan::GraphicPipelineStage stage, UniquePtr<IDependencyNodeCompiler> &outCompiler)
	{
		return New<RenderPipelineStageCompiler>(outCompiler, glslc, PipelineType::Graphics, static_cast<uint32_t>(stage));
	}
} } } // rkit::buildsystem::vulkan
