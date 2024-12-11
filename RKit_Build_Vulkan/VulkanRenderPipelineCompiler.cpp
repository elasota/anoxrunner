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
#include "rkit/Core/Stream.h"

#include <glslang/Include/glslang_c_interface.h>

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



	class PipelineCompilerBase
	{
	protected:
		static Result LoadDataDriver(data::IDataDriver **outDriver);
		static Result LoadPackage(BuildFileLocation location, const StringView &path, bool allowTempStrings, IDependencyNodeCompilerFeedback *feedback, UniquePtr<data::IRenderDataPackage> &outPackage, Vector<Vector<uint8_t>> *binaryContent);

		typedef const render::ShaderDesc *(render::GraphicsPipelineDesc:: *GraphicsPipelineShaderField_t);
		static Result GetGraphicsShaderPipelineShaderFieldForStage(render::vulkan::GraphicPipelineStage stage, GraphicsPipelineShaderField_t &outField);

		static Result FormatGraphicsPipelineStageFilePath(String &str, const StringView &inPath, render::vulkan::GraphicPipelineStage stage);
	};

	enum class PipelineType
	{
		Graphics,
	};

	class RenderPipelineStageBuildJob final : public PipelineCompilerBase
	{
	public:
		RenderPipelineStageBuildJob(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, PipelineType pipelineType);

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
			DynamicIncludeResult(String &&name, Vector<uint8_t> &&buffer);

		private:
			Vector<uint8_t> m_buffer;
			String m_name;
		};

		static Result WriteString(IWriteStream &stream, const rkit::StringSliceView &str);
		static Result WriteUIntString(IWriteStream &stream, uint64_t value);

		static Result WriteRTFormat(IWriteStream &stream, const render::RenderTargetFormat &format);
		static Result WriteVectorNumericType(IWriteStream &stream, const render::VectorNumericType &format);
		static Result WriteNumericType(IWriteStream &stream, const render::NumericType numericType);
		static Result WriteVectorDimension(IWriteStream &stream, const render::VectorDimension vectorDimension);
		static Result WriteTextureDescriptorType(IWriteStream &stream, const render::DescriptorType descriptorType, const render::ValueType &valueType);
		static Result WriteConfigurableRTFormat(IWriteStream &stream, const render::ConfigurableValueBase<render::RenderTargetFormat> &format);

		Result NormalizePath(String &path) const;
		Result TryInclude(String &&path, bool &outSucceeded, UniquePtr<IncludeResultBase> &outIncludeResult) const;
		Result ProcessInclude(const StringView &headerName, const StringView &includerName, size_t includeDepth, bool isSystem, UniquePtr<IncludeResultBase> &outIncludeResult);
		void FreeIncludeResult(glsl_include_result_t *result);

		static glsl_include_result_t *StaticIncludeCallback(void *ctx, const char *header_name, const char *includer_name, size_t include_depth, bool is_system);
		static glsl_include_result_t *StaticIncludeLocalCallback(void *ctx, const char *header_name, const char *includer_name, size_t include_depth);
		static glsl_include_result_t *StaticIncludeSystemCallback(void *ctx, const char *header_name, const char *includer_name, size_t include_depth);
		static int StaticFreeIncludeResultCallback(void *ctx, glsl_include_result_t *result);

		Vector<String> m_includePaths;	// Must be separator-terminated

		IMallocDriver *m_alloc;

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
		explicit RenderPipelineStageCompiler(PipelineType pipelineType, uint32_t stage);

		bool HasAnalysisStage() const override;
		Result RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;
		Result RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;

	private:
		RenderPipelineStageCompiler() = delete;

		PipelineType m_pipelineType;
		uint32_t m_stageUInt;
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

		static Result CompileGraphicsPipeline(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, data::IRenderDataPackage *package, const Span<const Vector<uint8_t>> &pkgBinaryContent, render::GraphicsPipelineDesc *pipeline);

		static Result AddGraphicsAnalysisStage(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, render::vulkan::GraphicPipelineStage stage);

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
		RKIT_CHECK(LoadPackage(BuildFileLocation::kIntermediateDir, depsNode->GetIdentifier(), true, feedback, package, nullptr));

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
			return ResultCode::kInternalError;

		return ResultCode::kOK;
	}

	Result RenderPipelineCompiler::RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		Vector<Vector<uint8_t>> binaryContent;

		UniquePtr<data::IRenderDataPackage> package;
		RKIT_CHECK(LoadPackage(BuildFileLocation::kIntermediateDir, depsNode->GetIdentifier(), true, feedback, package, &binaryContent));

		if (m_pipelineType == PipelineType::Graphics)
		{
			data::IRenderRTTIListBase *list = package->GetIndexable(data::RenderRTTIIndexableStructType::GraphicsPipelineDesc);
			if (list->GetCount() != 1)
			{
				rkit::log::Error("Pipeline package doesn't contain exactly one graphics pipeline");
				return ResultCode::kMalformedFile;
			}

			const render::GraphicsPipelineDesc *pipelineDesc = static_cast<const render::GraphicsPipelineDesc *>(list->GetElementPtr(0));

			RKIT_CHECK(CompileGraphicsPipeline(depsNode, feedback, package.Get(), binaryContent.ToSpan(), const_cast<render::GraphicsPipelineDesc *>(pipelineDesc)));
		}
		else
			return ResultCode::kInternalError;

		RKIT_CHECK(feedback->CheckFault());

		return ResultCode::kOK;
	}

	uint32_t RenderPipelineCompiler::GetVersion() const
	{
		return 1;
	}

	Result RenderPipelineCompiler::CompileGraphicsPipeline(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, data::IRenderDataPackage *package, const Span<const Vector<uint8_t>> &pkgBinaryContent, render::GraphicsPipelineDesc *pipeline)
	{
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

				String spvPath;
				RKIT_CHECK(FormatGraphicsPipelineStageFilePath(spvPath, depsNode->GetIdentifier(), stage));

				UniquePtr<ISeekableReadStream> stream;
				RKIT_CHECK(feedback->TryOpenInput(BuildFileLocation::kIntermediateDir, spvPath, stream));

				if (!stream.IsValid())
				{
					rkit::log::ErrorFmt("Failed to open SPV input %s", spvPath.CStr());
					return ResultCode::kOperationFailed;
				}

				FilePos_t size = stream->GetSize();
				if (size > std::numeric_limits<size_t>::max())
					return ResultCode::kOutOfMemory;

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

		rkit::buildsystem::IBuildSystemDriver *bsDriver = static_cast<rkit::buildsystem::IBuildSystemDriver *>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, "BuildSystem"));

		data::IDataDriver *dataDriver = nullptr;
		RKIT_CHECK(LoadDataDriver(&dataDriver));

		UniquePtr<IPackageObjectWriter> objectWriter;
		RKIT_CHECK(bsDriver->CreatePackageObjectWriter(objectWriter));

		UniquePtr<IPackageBuilder> packageBuilder;
		RKIT_CHECK(bsDriver->CreatePackageBuilder(dataDriver->GetRenderDataHandler(), objectWriter.Get(), false, packageBuilder));

		String outPath;
		RKIT_CHECK(outPath.Set(GetCompiledPipelineIntermediateBasePath()));
		RKIT_CHECK(outPath.Append(depsNode->GetIdentifier()));

		UniquePtr<ISeekableReadWriteStream> stream;
		RKIT_CHECK(feedback->OpenOutput(BuildFileLocation::kIntermediateDir, outPath, stream));

		BinaryInclusionIndexer indexer(package, pkgBinaryContent, binaryContentData);
		packageBuilder->BeginSource(&indexer);

		size_t index = 0;
		RKIT_CHECK(packageBuilder->IndexObject(pipeline, dataDriver->GetRenderDataHandler()->GetGraphicsPipelineDescRTTI(), false, index));

		RKIT_CHECK(packageBuilder->WritePackage(*stream));

		return ResultCode::kOK;
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

	RenderPipelineStageBuildJob::RenderPipelineStageBuildJob(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback, PipelineType pipelineType)
		: m_depsNode(depsNode)
		, m_feedback(feedback)
		, m_pipelineType(pipelineType)
		, m_glslangStage(GLSLANG_STAGE_COUNT)
		, m_alloc(GetDrivers().m_mallocDriver)
		, m_glslangResource{}
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
			return ResultCode::kInternalError;
		}

		GraphicsPipelineShaderField_t field;
		RKIT_CHECK(GetGraphicsShaderPipelineShaderFieldForStage(stage, field));

		const render::ShaderDesc *shaderDesc = (pipeline->*field);
		render::TempStringIndex_t sourceFileIndex = shaderDesc->m_source;

		RKIT_CHECK(m_shaderSourcePath.Set(package->GetString(sourceFileIndex.GetIndex())));


		RKIT_CHECK(WriteString(m_prefixStream, "#define float2 vec2\n"));
		RKIT_CHECK(WriteString(m_prefixStream, "#define float3 vec3\n"));
		RKIT_CHECK(WriteString(m_prefixStream, "#define float4 vec4\n"));
		RKIT_CHECK(WriteString(m_prefixStream, "#define float2x2 mat2x2\n"));
		RKIT_CHECK(WriteString(m_prefixStream, "#define float2x3 mat2x3\n"));
		RKIT_CHECK(WriteString(m_prefixStream, "#define float2x4 mat2x4\n"));
		RKIT_CHECK(WriteString(m_prefixStream, "#define float3x2 mat3x2\n"));
		RKIT_CHECK(WriteString(m_prefixStream, "#define float3x3 mat3x3\n"));
		RKIT_CHECK(WriteString(m_prefixStream, "#define float3x4 mat3x4\n"));
		RKIT_CHECK(WriteString(m_prefixStream, "#define float4x2 mat4x2\n"));
		RKIT_CHECK(WriteString(m_prefixStream, "#define float4x3 mat4x3\n"));
		RKIT_CHECK(WriteString(m_prefixStream, "#define float4x4 mat4x4\n"));

		if (pipeline->m_pushConstants)
		{
			return ResultCode::kNotYetImplemented;
		}

		if (stage == render::vulkan::GraphicPipelineStage::Vertex && pipeline->m_inputLayout != nullptr)
		{
			size_t inputIndex = 0;
			for (const render::InputLayoutVertexInputDesc *vertexInput : pipeline->m_inputLayout->m_vertexFeeds)
			{
				RKIT_CHECK(WriteString(m_prefixStream, "layout(location = "));
				RKIT_CHECK(WriteUIntString(m_prefixStream, inputIndex));
				RKIT_CHECK(WriteString(m_prefixStream, ") in "));
				RKIT_CHECK(WriteVectorNumericType(m_prefixStream, *vertexInput->m_numericType));
				RKIT_CHECK(WriteString(m_prefixStream, " _vs_in_F"));
				RKIT_CHECK(WriteUIntString(m_prefixStream, inputIndex));
				RKIT_CHECK(WriteString(m_prefixStream, ";\n"));

				RKIT_CHECK(WriteVectorNumericType(m_prefixStream, *vertexInput->m_numericType));
				RKIT_CHECK(WriteString(m_prefixStream, " VertexInput_Load_"));
				RKIT_CHECK(WriteString(m_prefixStream, package->GetString(vertexInput->m_feedName.GetIndex())));
				RKIT_CHECK(WriteString(m_prefixStream, "_"));
				RKIT_CHECK(WriteString(m_prefixStream, package->GetString(vertexInput->m_memberName.GetIndex())));
				RKIT_CHECK(WriteString(m_prefixStream, "()\n"));
				RKIT_CHECK(WriteString(m_prefixStream, "{\n"));
				RKIT_CHECK(WriteString(m_prefixStream, "    return _vs_in_F"));
				RKIT_CHECK(WriteUIntString(m_prefixStream, inputIndex));
				RKIT_CHECK(WriteString(m_prefixStream, ";\n"));
				RKIT_CHECK(WriteString(m_prefixStream, "}\n"));

				inputIndex++;
			}
		}

		if ((stage == render::vulkan::GraphicPipelineStage::Vertex || stage == render::vulkan::GraphicPipelineStage::Pixel) && pipeline->m_vertexShaderOutput != nullptr)
		{
			return ResultCode::kNotYetImplemented;
		}

		if (stage == render::vulkan::GraphicPipelineStage::Pixel)
		{
			for (size_t rtIndex = 0; rtIndex < pipeline->m_renderTargets.Count(); rtIndex++)
			{
				const render::RenderTargetDesc *rtDesc = pipeline->m_renderTargets[rtIndex];

				RKIT_CHECK(WriteString(m_prefixStream, "layout(location = "));
				RKIT_CHECK(WriteUIntString(m_prefixStream, rtIndex));
				RKIT_CHECK(WriteString(m_prefixStream, ") out "));

				RKIT_CHECK(WriteConfigurableRTFormat(m_prefixStream, rtDesc->m_format));

				RKIT_CHECK(WriteString(m_prefixStream, " _ps_out_RT"));
				RKIT_CHECK(WriteUIntString(m_prefixStream, rtIndex));
				RKIT_CHECK(WriteString(m_prefixStream, ";\n"));
			}

			RKIT_CHECK(WriteString(m_prefixStream, "struct PixelShaderOutput\n"));
			RKIT_CHECK(WriteString(m_prefixStream, "{\n"));

			for (size_t rtIndex = 0; rtIndex < pipeline->m_renderTargets.Count(); rtIndex++)
			{
				const render::RenderTargetDesc *rtDesc = pipeline->m_renderTargets[rtIndex];

				RKIT_CHECK(WriteString(m_prefixStream, "    "));
				RKIT_CHECK(WriteConfigurableRTFormat(m_prefixStream, rtDesc->m_format));
				RKIT_CHECK(WriteString(m_prefixStream, " "));
				RKIT_CHECK(WriteString(m_prefixStream, package->GetString(rtDesc->m_name.GetIndex())));
				RKIT_CHECK(WriteString(m_prefixStream, ";\n"));
			}

			RKIT_CHECK(WriteString(m_prefixStream, "};\n"));
		}

		for (size_t dlSlot = 0; dlSlot < pipeline->m_descriptorLayouts.Count(); dlSlot++)
		{
			const render::DescriptorLayoutDesc *dl = pipeline->m_descriptorLayouts[dlSlot];

			for (size_t descSlot = 0; descSlot < dl->m_descriptors.Count(); descSlot++)
			{
				const render::DescriptorDesc *descriptor = dl->m_descriptors[descSlot];

				if (descriptor->m_visibility != render::StageVisibility::All && descriptor->m_visibility != requiredVisibility)
					continue;

				RKIT_CHECK(WriteString(m_prefixStream, "layout(set = "));
				RKIT_CHECK(WriteUIntString(m_prefixStream, dlSlot));
				RKIT_CHECK(WriteString(m_prefixStream, ", binding = "));
				RKIT_CHECK(WriteUIntString(m_prefixStream, descSlot));

				switch (descriptor->m_descriptorType)
				{
				case render::DescriptorType::StaticConstantBuffer:
				case render::DescriptorType::DynamicConstantBuffer:
					RKIT_CHECK(WriteString(m_prefixStream, ", std140"));
					break;
				default:
					break;
				}

				RKIT_CHECK(WriteString(m_prefixStream, ") "));

				switch (descriptor->m_descriptorType)
				{
				case render::DescriptorType::Sampler:
					RKIT_CHECK(WriteString(m_prefixStream, "sampler "));
					break;

				case render::DescriptorType::StaticConstantBuffer:
				case render::DescriptorType::DynamicConstantBuffer:
					RKIT_CHECK(WriteString(m_prefixStream, "uniform "));
					break;

				case render::DescriptorType::Buffer:
				case render::DescriptorType::ByteAddressBuffer:
					RKIT_CHECK(WriteString(m_prefixStream, "readonly buffer "));
					break;

				case render::DescriptorType::RWBuffer:
				case render::DescriptorType::RWByteAddressBuffer:
					RKIT_CHECK(WriteString(m_prefixStream, "buffer "));
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
					RKIT_CHECK(WriteTextureDescriptorType(m_prefixStream, descriptor->m_descriptorType, descriptor->m_valueType));
					RKIT_CHECK(WriteString(m_prefixStream, " "));
					break;
				}

				RKIT_CHECK(WriteString(m_prefixStream, package->GetString(descriptor->m_name.GetIndex())));
				RKIT_CHECK(WriteString(m_prefixStream, ";\n"));
			}
		}

		RKIT_CHECK(WriteString(m_suffixStream, "void main()\n"));
		RKIT_CHECK(WriteString(m_suffixStream, "{\n"));

		if (stage == render::vulkan::GraphicPipelineStage::Vertex)
		{
			RKIT_CHECK(WriteString(m_suffixStream, "    vec4 vPosition;\n"));
		}

		if (stage == render::vulkan::GraphicPipelineStage::Pixel)
		{
			RKIT_CHECK(WriteString(m_prefixStream, "PixelShaderOutput pOutput;\n"));
		}

		// Call entry point
		RKIT_CHECK(WriteString(m_suffixStream, "    "));
		RKIT_CHECK(WriteString(m_suffixStream, package->GetString(shaderDesc->m_entryPoint.GetIndex())));
		RKIT_CHECK(WriteString(m_suffixStream, "("));

		if (stage == render::vulkan::GraphicPipelineStage::Vertex)
		{
			RKIT_CHECK(WriteString(m_suffixStream, "vPosition"));
		}

		if (stage == render::vulkan::GraphicPipelineStage::Pixel)
		{
			RKIT_CHECK(WriteString(m_suffixStream, "pOutput"));
		}

		RKIT_CHECK(WriteString(m_suffixStream, ");\n"));

		// Suffixes
		if (stage == render::vulkan::GraphicPipelineStage::Vertex)
		{
			RKIT_CHECK(WriteString(m_suffixStream, "    gl_Position = vPosition;\n"));
		}

		if (stage == render::vulkan::GraphicPipelineStage::Pixel)
		{
			for (size_t rtIndex = 0; rtIndex < pipeline->m_renderTargets.Count(); rtIndex++)
			{
				const render::RenderTargetDesc *rtDesc = pipeline->m_renderTargets[rtIndex];

				RKIT_CHECK(WriteString(m_suffixStream, "    _ps_out_RT"));
				RKIT_CHECK(WriteUIntString(m_suffixStream, rtIndex));
				RKIT_CHECK(WriteString(m_suffixStream, " = pOutput."));
				RKIT_CHECK(WriteString(m_suffixStream, package->GetString(rtDesc->m_name.GetIndex())));
				RKIT_CHECK(WriteString(m_suffixStream, ";\n"));
			}
		}

		RKIT_CHECK(WriteString(m_suffixStream, "}\n"));

		if (pipeline->m_renderTargets.Count() > static_cast<size_t>(std::numeric_limits<int>::max()))
			return ResultCode::kIntegerOverflow;

		m_glslangResource.max_draw_buffers = static_cast<int>(pipeline->m_renderTargets.Count());

		return Compile();
	}

	Result RenderPipelineStageBuildJob::Compile()
	{
		String mainShaderContent;
		RKIT_CHECK(mainShaderContent.Append("#extension GL_ARB_shading_language_include : enable\n"
			"#include <GlslShaderPrefix>\n"
			"#include \"./"));
		RKIT_CHECK(mainShaderContent.Append(m_shaderSourcePath));
		RKIT_CHECK(mainShaderContent.Append("\"\n"
			"#include <GlslShaderSuffix>\n"));

		glslang_input_t input = {};
		input.language = GLSLANG_SOURCE_GLSL;
		input.stage = m_glslangStage;
		input.client = GLSLANG_CLIENT_VULKAN;
		input.client_version = GLSLANG_TARGET_VULKAN_1_0;
		input.target_language = GLSLANG_TARGET_SPV;
		input.target_language_version = GLSLANG_TARGET_SPV_1_0;
		input.code = mainShaderContent.CStr();
		input.default_version = 450;
		input.default_profile = GLSLANG_CORE_PROFILE;
		input.messages = GLSLANG_MSG_DEFAULT_BIT;
		input.callbacks.free_include_result = StaticFreeIncludeResultCallback;
		input.callbacks.include_local = StaticIncludeLocalCallback;
		input.callbacks.include_system = StaticIncludeSystemCallback;
		input.callbacks_ctx = this;
		input.resource = &m_glslangResource;

		glslang_shader_t *shader = glslang_shader_create(&input);
		if (!shader)
			return ResultCode::kOperationFailed;

		if (!glslang_shader_preprocess(shader, &input) || !glslang_shader_parse(shader, &input))
		{
			const char *infoLog = glslang_shader_get_info_log(shader);
			const char *infoDebugLog = glslang_shader_get_info_debug_log(shader);

			if (infoLog && infoLog[0])
				rkit::log::Error(infoLog);

			if (infoDebugLog && infoDebugLog[0])
				rkit::log::Error(infoDebugLog);

			glslang_shader_delete(shader);
			return ResultCode::kOperationFailed;
		}

		glslang_program_t *program = glslang_program_create();
		if (!program)
		{
			glslang_shader_delete(shader);
			return ResultCode::kOperationFailed;
		}

		glslang_program_add_shader(program, shader);

		if (!glslang_program_link(program, GLSLANG_MSG_DEFAULT_BIT))
		{
			const char *infoLog = glslang_program_get_info_log(program);
			const char *infoDebugLog = glslang_program_get_info_debug_log(program);

			if (infoLog && infoLog[0])
				rkit::log::Error(infoLog);

			if (infoDebugLog && infoDebugLog[0])
				rkit::log::Error(infoDebugLog);

			glslang_program_delete(program);
			glslang_shader_delete(shader);
			return ResultCode::kOperationFailed;
		}

		glslang_program_SPIRV_generate(program, m_glslangStage);

		size_t spvSize = glslang_program_SPIRV_get_size(program);

		RKIT_CHECK(m_resultSPV.Resize(spvSize));
		if (spvSize > 0)
			glslang_program_SPIRV_get(program, &m_resultSPV[0]);

		const char *spvMessages = glslang_program_SPIRV_get_messages(program);
		if (spvMessages && spvMessages[0])
			rkit::log::LogInfo(spvMessages);

		glslang_program_delete(program);
		glslang_shader_delete(shader);

		return ResultCode::kOK;
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
		Span<const char> span = str.ToSpan();
		if (span.Count() == 0)
			return ResultCode::kOK;

		return stream.WriteAll(span.Ptr(), span.Count());
	}

	Result RenderPipelineStageBuildJob::WriteUIntString(IWriteStream &stream, uint64_t value)
	{
		const uint32_t kMaxDigits = 20;

		char strBuffer[kMaxDigits + 1];
		strBuffer[kMaxDigits] = '\0';

		size_t numDigits = 0;
		do
		{
			numDigits++;
			strBuffer[kMaxDigits - numDigits] = static_cast<char>('0' + (value % 10u));
			value /= 10u;
		} while (value != 0u);

		return WriteString(stream, StringView(strBuffer + (kMaxDigits - numDigits), numDigits));
	}

	Result RenderPipelineStageBuildJob::WriteRTFormat(IWriteStream &stream, const render::RenderTargetFormat &format)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result RenderPipelineStageBuildJob::WriteVectorNumericType(IWriteStream &stream, const render::VectorNumericType &format)
	{
		RKIT_CHECK(WriteNumericType(stream, format.m_numericType));
		RKIT_CHECK(WriteVectorDimension(stream, format.m_cols));

		return ResultCode::kOK;
	}

	Result RenderPipelineStageBuildJob::WriteNumericType(IWriteStream &stream, const render::NumericType numericType)
	{
		switch (numericType)
		{
		case render::NumericType::Bool:
			return WriteString(stream, "bool");
		case render::NumericType::Float16:
			return WriteString(stream, "half");
		case render::NumericType::Float32:
			return WriteString(stream, "float");
		case render::NumericType::Float64:
			return WriteString(stream, "double");
		case render::NumericType::SInt8:
			return WriteString(stream, "sbyte");
		case render::NumericType::SInt16:
			return WriteString(stream, "short");
		case render::NumericType::SInt32:
			return WriteString(stream, "int");
		case render::NumericType::SInt64:
			return WriteString(stream, "long");
		case render::NumericType::UInt8:
			return WriteString(stream, "byte");
		case render::NumericType::UInt16:
			return WriteString(stream, "ushort");
		case render::NumericType::UInt32:
			return WriteString(stream, "uint");
		case render::NumericType::UInt64:
			return WriteString(stream, "ulong");
		default:
			rkit::log::Error("A numeric type could not be written in the desired context");
			return ResultCode::kOperationFailed;
		}
	}

	Result RenderPipelineStageBuildJob::WriteVectorDimension(IWriteStream &stream, const render::VectorDimension vectorDimension)
	{
		switch (vectorDimension)
		{
		case render::VectorDimension::Dimension2:
			return WriteString(stream, "2");
		case render::VectorDimension::Dimension3:
			return WriteString(stream, "3");
		case render::VectorDimension::Dimension4:
			return WriteString(stream, "4");
		default:
			return ResultCode::kInternalError;
		}
	}

	Result RenderPipelineStageBuildJob::WriteConfigurableRTFormat(IWriteStream &stream, const render::ConfigurableValueBase<render::RenderTargetFormat> &format)
	{
		if (format.m_state == render::ConfigurableValueState::Explicit)
			return WriteRTFormat(stream, format.m_u.m_value);
		else
			return WriteString(stream, "float4");
	}

	Result RenderPipelineStageBuildJob::WriteTextureDescriptorType(IWriteStream &stream, const render::DescriptorType descriptorType, const render::ValueType &valueType)
	{
		if (valueType.m_type == render::ValueTypeType::VectorNumeric)
			return WriteTextureDescriptorType(stream, descriptorType, render::ValueType(valueType.m_value.m_vectorNumericType->m_numericType));

		if (valueType.m_type != render::ValueTypeType::Numeric)
		{
			rkit::log::Error("Texture descriptor type was non-numeric");
			return ResultCode::kOperationFailed;
		}

		const render::NumericType numericType = valueType.m_value.m_numericType;

		switch (numericType)
		{
		case render::NumericType::SInt8:
		case render::NumericType::SInt16:
		case render::NumericType::SInt32:
		case render::NumericType::SInt64:
			RKIT_CHECK(WriteString(stream, "i"));
			break;

		case render::NumericType::UInt8:
		case render::NumericType::UInt16:
		case render::NumericType::UInt32:
		case render::NumericType::UInt64:
			RKIT_CHECK(WriteString(stream, "u"));
			break;

		default:
			break;
		}

		RKIT_CHECK(WriteString(stream, "texture"));

		switch (descriptorType)
		{
		case render::DescriptorType::Texture1D:
		case render::DescriptorType::Texture1DArray:
		case render::DescriptorType::RWTexture1D:
		case render::DescriptorType::RWTexture1DArray:
			RKIT_CHECK(WriteString(stream, "1D"));
			break;

		case render::DescriptorType::Texture2D:
		case render::DescriptorType::Texture2DArray:
		case render::DescriptorType::Texture2DMS:
		case render::DescriptorType::Texture2DMSArray:
		case render::DescriptorType::RWTexture2D:
		case render::DescriptorType::RWTexture2DArray:
			RKIT_CHECK(WriteString(stream, "2D"));
			break;

		case render::DescriptorType::Texture3D:
		case render::DescriptorType::RWTexture3D:
			RKIT_CHECK(WriteString(stream, "3D"));
			break;

		case render::DescriptorType::TextureCube:
		case render::DescriptorType::TextureCubeArray:
			RKIT_CHECK(WriteString(stream, "Cube"));
			break;

		default:
			return ResultCode::kInternalError;
		}

		switch (descriptorType)
		{
		case render::DescriptorType::Texture1DArray:
		case render::DescriptorType::RWTexture1DArray:
		case render::DescriptorType::Texture2DArray:
		case render::DescriptorType::TextureCubeArray:
			RKIT_CHECK(WriteString(stream, "Array"));
			break;

		case render::DescriptorType::Texture2DMSArray:
		case render::DescriptorType::RWTexture2DArray:
			RKIT_CHECK(WriteString(stream, "MSArray"));
			break;

		case render::DescriptorType::Texture2DMS:
			RKIT_CHECK(WriteString(stream, "MS"));
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
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
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
					return ResultCode::kOperationFailed;
			}

			if (i == path.Length() || c == '/')
			{
				StringSliceView slice = path.SubString(sliceStart, i - sliceStart);
				sliceStart = i + 1;

				if (slice.Length() == 0 || slice == ".")
					return ResultCode::kOperationFailed;

				if (slice == "..")
				{
					size_t lastSlashPos = 0;
					for (size_t j = 0; j < path.Length(); i++)
					{
						if (path[j] == '/')
							lastSlashPos = j;
					}

					if (lastSlashPos == 0)
						return ResultCode::kOperationFailed;

					RKIT_CHECK(fullPath.Set(fullPath.SubString(0, lastSlashPos + 1)));
				}
				else
				{
					if (fullPath.Length() > 0)
					{
						RKIT_CHECK(fullPath.Append("/"));
					}
					RKIT_CHECK(fullPath.Append(slice));
				}
			}
		}

		if (fullPath.Length() == 0 || !GetDrivers().m_utilitiesDriver->ValidateFilePath(fullPath.ToSpan()))
			return ResultCode::kOperationFailed;

		path = std::move(fullPath);
		return ResultCode::kOK;
	}

	Result RenderPipelineStageBuildJob::TryInclude(String &&path, bool &outSucceeded, UniquePtr<IncludeResultBase> &outIncludeResult) const
	{
		outSucceeded = false;
		outIncludeResult.Reset();

		String fullPath;
		RKIT_CHECK(fullPath.Set(buildsystem::GetShaderSourceBasePath()));
		RKIT_CHECK(fullPath.Append(path));

		UniquePtr<ISeekableReadStream> stream;
		RKIT_CHECK(m_feedback->TryOpenInput(BuildFileLocation::kSourceDir, fullPath, stream));

		if (!stream.IsValid())
			return ResultCode::kOK;

		FilePos_t size = static_cast<size_t>(stream->GetSize());
		if (stream->GetSize() > std::numeric_limits<size_t>::max())
			return ResultCode::kOutOfMemory;

		Vector<uint8_t> contents;
		RKIT_CHECK(contents.Resize(static_cast<size_t>(size)));

		if (size > 0)
		{
			RKIT_CHECK(stream->ReadAll(&contents[0], static_cast<size_t>(size)));
		}

		stream.Reset();

		RKIT_CHECK(New<DynamicIncludeResult>(outIncludeResult, std::move(path), std::move(contents)));

		outSucceeded = true;
		return ResultCode::kOK;
	}

	Result RenderPipelineStageBuildJob::ProcessInclude(const StringView &headerName, const StringView &includerName, size_t includeDepth, bool isSystem, UniquePtr<IncludeResultBase> &outIncludeResult)
	{
		if (isSystem)
		{
			if (headerName == "GlslShaderPrefix")
			{
				const Vector<uint8_t> &prefixVector = m_prefixStream.GetBuffer();

				String headerNameStr;
				RKIT_CHECK(headerNameStr.Set(headerName));

				return New<StaticIncludeResult>(outIncludeResult, std::move(headerNameStr), prefixVector.GetBuffer(), prefixVector.Count());
			}
			if (headerName == "GlslShaderSuffix")
			{
				const Vector<uint8_t> &prefixVector = m_suffixStream.GetBuffer();

				String headerNameStr;
				RKIT_CHECK(headerNameStr.Set(headerName));

				return New<StaticIncludeResult>(outIncludeResult, std::move(headerNameStr), prefixVector.GetBuffer(), prefixVector.Count());
			}

			return ResultCode::kOperationFailed;
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

				if (slice == ".")
				{
					if (numSlices != 0)
						return ResultCode::kOperationFailed;

					isAbsolutePath = true;
				}

				numSlices++;
			}

			scanPos++;
		}

		if (isAbsolutePath)
		{
			StringSliceView headerAbsPath = headerName.SubString(2);

			String normalizedPath;
			RKIT_CHECK(normalizedPath.Set(headerAbsPath));
			RKIT_CHECK(NormalizePath(normalizedPath));

			bool succeeded = false;
			RKIT_CHECK(TryInclude(std::move(normalizedPath), succeeded, outIncludeResult));

			if (succeeded)
				return ResultCode::kOK;
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

				String path;
				RKIT_CHECK(path.Set(includerName.SubString(0, basePathLength)));
				RKIT_CHECK(path.Append(headerName));
				RKIT_CHECK(NormalizePath(path));

				bool succeeded = false;
				RKIT_CHECK(TryInclude(std::move(path), succeeded, outIncludeResult));

				if (succeeded)
					return ResultCode::kOK;
			}

			if (shouldTryIncludePaths)
			{
				String normalizedHeaderName;
				RKIT_CHECK(normalizedHeaderName.Set(headerName));
				RKIT_CHECK(NormalizePath(normalizedHeaderName));

				for (const String &includePath : m_includePaths)
				{
					String fullPath = includePath;
					RKIT_CHECK(fullPath.Append(normalizedHeaderName));

					bool succeeded = false;
					RKIT_CHECK(TryInclude(std::move(fullPath), succeeded, outIncludeResult));

					if (succeeded)
						return ResultCode::kOK;
				}
			}
		}

		return ResultCode::kOperationFailed;
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
		Result result = job->ProcessInclude(StringView(header_name, strlen(header_name)), StringView(includer_name, strlen(includer_name)), include_depth, is_system, includeResult);

		if (!result.IsOK())
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
		header_name = m_name.CStr();
	}

	RenderPipelineStageBuildJob::DynamicIncludeResult::DynamicIncludeResult(String &&name, Vector<uint8_t> &&data)
	{
		m_name = std::move(name);
		m_buffer = std::move(data);

		header_data = reinterpret_cast<const char *>(m_buffer.GetBuffer());
		header_length = m_buffer.Count();
		header_name = m_name.CStr();
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
			RKIT_CHECK(LoadPackage(BuildFileLocation::kIntermediateDir, depsNode->GetIdentifier(), true, feedback, package, nullptr));

			data::IRenderRTTIListBase *list = package->GetIndexable(data::RenderRTTIIndexableStructType::GraphicsPipelineDesc);
			if (list->GetCount() != 1)
			{
				rkit::log::Error("Pipeline package doesn't contain exactly one graphics pipeline");
				return ResultCode::kMalformedFile;
			}

			const render::GraphicsPipelineDesc *pipelineDesc = static_cast<const render::GraphicsPipelineDesc *>(list->GetElementPtr(0));

			render::vulkan::GraphicPipelineStage stage = static_cast<render::vulkan::GraphicPipelineStage>(m_stageUInt);

			RenderPipelineStageBuildJob buildJob(depsNode, feedback, m_pipelineType);
			RKIT_CHECK(buildJob.RunGraphics(package.Get(), pipelineDesc, stage));

			String outPath;
			RKIT_CHECK(FormatGraphicsPipelineStageFilePath(outPath, depsNode->GetIdentifier(), stage));

			UniquePtr<ISeekableReadWriteStream> outStream;
			RKIT_CHECK(feedback->OpenOutput(BuildFileLocation::kIntermediateDir, outPath, outStream));

			RKIT_CHECK(buildJob.WriteToFile(*outStream));

			return ResultCode::kOK;
		}

		return ResultCode::kInternalError;
	}

	uint32_t RenderPipelineStageCompiler::GetVersion() const
	{
		return 1;
	}

	Result PipelineCompilerBase::LoadPackage(BuildFileLocation location, const StringView &path, bool allowTempStrings, IDependencyNodeCompilerFeedback *feedback, UniquePtr<data::IRenderDataPackage> &outPackage, Vector<Vector<uint8_t>> *binaryContent)
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

		RKIT_CHECK(dataHandler->LoadPackage(*packageStream, allowTempStrings, outPackage, binaryContent));

		return ResultCode::kOK;
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
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result PipelineCompilerBase::FormatGraphicsPipelineStageFilePath(String &str, const StringView &inPath, render::vulkan::GraphicPipelineStage stage)
	{
		return str.Format("vk_pl_g_%i/%s", static_cast<int>(stage), inPath.GetChars());
	}

	Result PipelineCompilerBase::LoadDataDriver(data::IDataDriver **outDriver)
	{

		rkit::IModule *dataModule = rkit::GetDrivers().m_moduleDriver->LoadModule(IModuleDriver::kDefaultNamespace, "Data");
		if (!dataModule)
		{
			rkit::log::Error("Couldn't load data module");
			return rkit::ResultCode::kModuleLoadFailed;
		}

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
