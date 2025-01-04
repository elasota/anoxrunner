#include "VulkanPipelineLibraryLoader.h"

#include "rkit/Utilities/Sha2.h"
#include "rkit/Utilities/ShadowFile.h"

#include "rkit/Render/RenderDefs.h"

#include "rkit/Data/RenderDataHandler.h"

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/Endian.h"
#include "rkit/Core/FourCC.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/MutexLock.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/String.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"

#include "rkit/Vulkan/GraphicsPipeline.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDevice.h"
#include "VulkanPhysDevice.h"

namespace rkit::render::vulkan
{
	struct PipelineCacheHeader
	{
		enum class Phase : uint16_t
		{
			kEmpty,
			kIndividual,
			kMerged,
		};

		static const uint32_t kExpectedIdentifier = RKIT_FOURCC('R', 'V', 'P', 'C');
		static const uint16_t kExpectedVersion = 1;

		uint32_t m_identifier = kExpectedIdentifier;
		uint16_t m_version = kExpectedVersion;
		Phase m_phase = Phase::kEmpty;

		utils::Sha256DigestBytes m_packageUUID;
	};

	class PipelineLibraryLoader final : public PipelineLibraryLoaderBase
	{
	public:
		PipelineLibraryLoader(VulkanDeviceBase &device, UniquePtr<IPipelineLibraryConfigValidator> &&validator,
			UniquePtr<data::IRenderDataPackage> &&package, UniquePtr<ISeekableReadStream> &&packageStream, FilePos_t packageBinaryContentStart);
		~PipelineLibraryLoader();

		Result Initialize();

		Result LoadObjectsFromPackage() override;

		void SetMergedLibraryStream(UniquePtr<ISeekableReadStream> &&cacheReadStream, ISeekableReadWriteStream *cacheWriteStream) override;
		Result OpenMergedLibrary() override;
		Result LoadGraphicsPipelineFromMergedLibrary(size_t pipelineIndex, size_t permutationIndex) override;
		void CloseMergedLibrary(bool unloadPipelines, bool unloadMergedCache) override;

		Result CompileUnmergedGraphicsPipeline(size_t pipelineIndex, size_t permutationIndex) override;
		Result AddMergedPipeline(size_t pipelineIndex, size_t permutationIndex) override;
		Result SaveMergedPipeline() override;

	private:
		class ConfigHashComputer final : public IWriteStream
		{
		public:
			explicit ConfigHashComputer(const utils::ISha256Calculator *calculator);

			Result WritePartial(const void *data, size_t count, size_t &outCountWritten) override;
			Result Flush() override;

			utils::Sha256DigestBytes FinishSHA();

		private:
			const utils::ISha256Calculator *m_calculator;
			utils::Sha256StreamingState m_streamingState;
		};

		struct CompiledPipelineKey
		{
			size_t m_pipelineIndex = 0;
			size_t m_permutationIndex = 0;

			bool operator==(const CompiledPipelineKey &other) const;
			bool operator!=(const CompiledPipelineKey &other) const;
		};

		struct CompiledPipelineKeyHasher
		{
			static HashValue_t ComputeHash(HashValue_t baseHash, const CompiledPipelineKey &value);
		};

		struct CompiledPipelineFileLocator
		{
			FilePos_t m_filePos = 0;
			size_t m_size = 0;
		};

		struct BinaryContentData
		{
			FilePos_t m_filePos = 0;
			VkShaderModule m_shaderModule = VK_NULL_HANDLE;
		};

		struct CompiledPipeline
		{
			VkPipeline m_pipeline = VK_NULL_HANDLE;
		};

		static Result ResolveVertexInputAttributeFormat(VkFormat &outVkFormat, const VectorOrScalarNumericType &vectorType);
		static Result ResolveRenderTargetFormat(VkFormat &outVkFormat, RenderTargetFormat rtFormat);
		static Result ResolveDepthStencilFormat(VkFormat &outVkFormat, DepthStencilFormat dsFormat);
		static Result ResolveDepthStencilFormatHasStencil(bool &outHaveStencil, DepthStencilFormat dsFormat);

		static Result ResolveColorBlendFactor(VkBlendFactor &outBlendFactor, ColorBlendFactor cbf);
		static Result ResolveAlphaBlendFactor(VkBlendFactor &outBlendFactor, AlphaBlendFactor abf);
		static Result ResolveBlendOp(VkBlendOp &outBlendOp, BlendOp bo);

		static Result ResolveVertexInputAttributeFormat_Scalar(VkFormat &outVkFormat, NumericType numericType);
		static Result ResolveVertexInputAttributeFormat_V2(VkFormat &outVkFormat, NumericType numericType);
		static Result ResolveVertexInputAttributeFormat_V3(VkFormat &outVkFormat, NumericType numericType);
		static Result ResolveVertexInputAttributeFormat_V4(VkFormat &outVkFormat, NumericType numericType);

		static Result ResolveStencilOps(VkStencilOpState &outOpState, const StencilOpDesc &stencilOpDesc, const DepthStencilOperationDesc &depthStencilDesc);
		static Result ResolveStencilOp(VkStencilOp &outOp, StencilOp stencilOp);
		static Result ResolveCompareOp(VkCompareOp &outOp, ComparisonFunction compareFunc);

		static Result ResolveAnyStageFlags(VkPipelineStageFlags &outStageFlags, StageVisibility visibility);
		static Result ResolveGraphicsStageFlags(VkPipelineStageFlags &outStageFlags, StageVisibility visibility);
		static Result ResolveDescriptorType(VkDescriptorType &outDescType, DescriptorType descType);

		static Result ResolveFilter(VkFilter &outFilter, Filter filter);
		static Result ResolveSamplerMipMapMode(VkSamplerMipmapMode &outMipmapMode, MipMapMode mipmapMode);
		static Result ResolveSamplerAddressMode(VkSamplerAddressMode &outAddressMode, AddressMode addressMode);
		static Result ResolveAnisotropy(float &outMaxAnisotropy, AnisotropicFiltering anisoFiltering);

		static Result ResolvePushConstantSize(uint32_t &size, const ValueType &valueType);
		static Result ResolvePushConstantSize(uint32_t &size, const CompoundNumericType &valueType);
		static Result ResolvePushConstantSize(uint32_t &size, const VectorNumericType &valueType);
		static Result ResolvePushConstantSize(uint32_t &size, NumericType numericType);

		Result FindSampler(VkSampler &outSampler, const SamplerDesc *sampler) const;
		Result FindDescriptorSetLayout(VkDescriptorSetLayout &outDSL, const DescriptorLayoutDesc *descLayout) const;
		Result FindPipelineLayout(VkPipelineLayout &outPipelineLayout, const PipelineLayoutDesc *pipelineLayout) const;
		Result FindRenderPass(VkRenderPass &outRenderPass, const RenderPassDesc *renderPass) const;

		Result CreateRenderPass(VkRenderPass &outRenderPass, const RenderPassDesc &rpDesc);

		Result CheckedCreateGraphicsPipeline(VkPipelineCache &pipelineCache, CompiledPipeline &pipeline, size_t pipelineIndex, size_t permutationIndex);
		Result CheckedCompileUnmergedGraphicsPipeline(VkPipelineCache &pipelineCache, CompiledPipeline &pipeline, size_t pipelineIndex, size_t permutationIndex);
		Result LoadShaderModule(size_t binaryContentIndex);

		static Result GetUnmergedCachePath(String &path, const utils::Sha256DigestBytes &configHash, size_t pipelineIndex, size_t permutationIndex);

		template<class T, class TDefaultResolver>
		static T ResolveConfigurable(const ConfigurableValueWithDefault<T, TDefaultResolver> &configurable);

		UniquePtr<IMutex> m_packageStreamMutex;
		UniquePtr<IMutex> m_cacheStreamMutex;
		UniquePtr<IMutex> m_shaderModuleMutex;

		// Protect under CacheStreamMutex
		HashMap<CompiledPipelineKey, CompiledPipelineFileLocator> m_individualPipelineLocators;

		VulkanDeviceBase &m_device;

		UniquePtr<data::IRenderDataPackage> m_package;
		UniquePtr<ISeekableReadStream> m_packageStream;

		FilePos_t m_packageBinaryContentStart;

		UniquePtr<ISeekableReadStream> m_cacheReadStream;
		ISeekableReadWriteStream *m_cacheWriteStream = nullptr;

		UniquePtr<IPipelineLibraryConfigValidator> m_validator;

		Vector<size_t> m_graphicPipelinePermutationStarts;
		Vector<VkPipeline> m_allPipelines;
		Vector<VkRenderPass> m_renderPasses;
		Vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
		Vector<VkPipelineLayout> m_pipelineLayouts;
		Vector<VkSampler> m_samplers;

		Vector<BinaryContentData> m_binaryContents;

		utils::Sha256DigestBytes m_configHashBytes;

		Vector<VkPipelineCache> m_individualCaches;
		VkPipelineCache m_mergedCache = VK_NULL_HANDLE;
	};

	PipelineLibraryLoader::ConfigHashComputer::ConfigHashComputer(const utils::ISha256Calculator *calculator)
		: m_calculator(calculator)
		, m_streamingState(calculator->CreateStreamingState())
	{
	}

	Result PipelineLibraryLoader::ConfigHashComputer::WritePartial(const void *data, size_t count, size_t &outCountWritten)
	{
		m_calculator->AppendStreamingState(m_streamingState, data, count);
		outCountWritten = count;
		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ConfigHashComputer::Flush()
	{
		return ResultCode::kOK;
	}

	utils::Sha256DigestBytes PipelineLibraryLoader::ConfigHashComputer::FinishSHA()
	{
		m_calculator->FinalizeStreamingState(m_streamingState);
		return m_calculator->FlushToBytes(m_streamingState.m_state);
	}


	bool PipelineLibraryLoader::CompiledPipelineKey::operator==(const CompiledPipelineKey &other) const
	{
		return m_permutationIndex == other.m_permutationIndex && m_pipelineIndex == other.m_pipelineIndex;
	}

	bool PipelineLibraryLoader::CompiledPipelineKey::operator!=(const CompiledPipelineKey &other) const
	{
		return !((*this) == other);
	}

	HashValue_t PipelineLibraryLoader::CompiledPipelineKeyHasher::ComputeHash(HashValue_t hash, const CompiledPipelineKey &value)
	{
		hash = Hasher<size_t>::ComputeHash(hash, value.m_pipelineIndex);
		hash = Hasher<size_t>::ComputeHash(hash, value.m_permutationIndex);

		return hash;
	}

	PipelineLibraryLoader::PipelineLibraryLoader(VulkanDeviceBase &device, UniquePtr<IPipelineLibraryConfigValidator> &&validator,
		UniquePtr<data::IRenderDataPackage> &&package, UniquePtr<ISeekableReadStream> &&packageStream, FilePos_t packageBinaryContentStart)
		: m_device(device)
		, m_validator(std::move(validator))
		, m_package(std::move(package))
		, m_packageStream(std::move(packageStream))
		, m_packageBinaryContentStart(packageBinaryContentStart)
		, m_cacheWriteStream(nullptr)
	{
	}

	PipelineLibraryLoader::~PipelineLibraryLoader()
	{
		m_cacheReadStream.Reset();
		m_cacheWriteStream = nullptr;

		m_package.Reset();
		m_packageStream.Reset();

		m_validator.Reset();

		const VulkanDeviceAPI &vkd = m_device.GetDeviceAPI();
		const VkAllocationCallbacks *allocCallbacks = m_device.GetAllocCallbacks();
		VkDevice device = m_device.GetDevice();

		for (const VkPipeline &pipeline : m_allPipelines)
		{
			if (pipeline != VK_NULL_HANDLE)
				vkd.vkDestroyPipeline(device, pipeline, allocCallbacks);
		}

		for (const BinaryContentData &bcData : m_binaryContents)
		{
			if (bcData.m_shaderModule != VK_NULL_HANDLE)
				vkd.vkDestroyShaderModule(device, bcData.m_shaderModule, allocCallbacks);
		}

		for (const VkPipelineLayout &pl : m_pipelineLayouts)
		{
			if (pl != VK_NULL_HANDLE)
				vkd.vkDestroyPipelineLayout(device, pl, allocCallbacks);
		}

		for (const VkDescriptorSetLayout &dsLayout : m_descriptorSetLayouts)
		{
			if (dsLayout != VK_NULL_HANDLE)
				vkd.vkDestroyDescriptorSetLayout(device, dsLayout, allocCallbacks);
		}

		for (const VkSampler &s : m_samplers)
		{
			if (s != VK_NULL_HANDLE)
				vkd.vkDestroySampler(device, s, allocCallbacks);
		}

		for (const VkRenderPass &rp : m_renderPasses)
		{
			if (rp != VK_NULL_HANDLE)
				vkd.vkDestroyRenderPass(device, rp, allocCallbacks);
		}

		for (const VkPipelineCache &plc : m_individualCaches)
		{
			if (plc != VK_NULL_HANDLE)
				vkd.vkDestroyPipelineCache(device, plc, allocCallbacks);
		}

		if (m_mergedCache != VK_NULL_HANDLE)
			vkd.vkDestroyPipelineCache(device, m_mergedCache, allocCallbacks);
	}

	Result PipelineLibraryLoader::Initialize()
	{
		ISystemDriver *sysDriver = GetDrivers().m_systemDriver;
		IUtilitiesDriver *utilsDriver = GetDrivers().m_utilitiesDriver;

		RKIT_CHECK(sysDriver->CreateMutex(m_packageStreamMutex));
		RKIT_CHECK(sysDriver->CreateMutex(m_cacheStreamMutex));
		RKIT_CHECK(sysDriver->CreateMutex(m_shaderModuleMutex));

		ConfigHashComputer hashComputer(utilsDriver->GetSha256Calculator());
		RKIT_CHECK(m_validator->WriteConfig(hashComputer));

		m_configHashBytes = hashComputer.FinishSHA();

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::LoadObjectsFromPackage()
	{
		const VulkanDeviceAPI &vkd = m_device.GetDeviceAPI();

		size_t totalPermutations = 0;

		data::IRenderRTTIListBase *graphicsPipelines = m_package->GetIndexable(rkit::data::RenderRTTIIndexableStructType::GraphicsPipelineDesc);
		const size_t numGraphicsPipelines = graphicsPipelines->GetCount();

		const size_t numAllPipelines = numGraphicsPipelines;

		RKIT_CHECK(m_graphicPipelinePermutationStarts.Resize(numAllPipelines));

		for (size_t i = 0; i < numGraphicsPipelines; i++)
		{
			const GraphicsPipelineDesc *pipelineDesc = static_cast<const GraphicsPipelineDesc *>(graphicsPipelines->GetElementPtr(i));

			size_t treeWidth = 0;
			if (pipelineDesc->m_permutationTree == nullptr)
				treeWidth = 1;
			else
			{
				ConstSpan<const ShaderPermutationTreeBranch *> branches = pipelineDesc->m_permutationTree->m_branches;

				size_t branchWidth = 0;
				for (const ShaderPermutationTreeBranch *branch : branches)
				{
					if (branch->m_subTree == nullptr)
						branchWidth = 1;
					else
						branchWidth = branch->m_subTree->m_width;

					RKIT_CHECK(SafeAdd(treeWidth, treeWidth, branchWidth));
				}
			}

			m_graphicPipelinePermutationStarts[i] = totalPermutations;

			RKIT_CHECK(SafeAdd(totalPermutations, totalPermutations, treeWidth));
		}

		RKIT_CHECK(m_allPipelines.Resize(totalPermutations));

		for (VkPipeline &pipeline : m_allPipelines)
			pipeline = VK_NULL_HANDLE;

		size_t numBinaryContents = m_package->GetBinaryContentCount();
		RKIT_CHECK(m_binaryContents.Resize(numBinaryContents));

		FilePos_t binaryContentStart = m_packageBinaryContentStart;
		for (size_t i = 0; i < numBinaryContents; i++)
		{
			const size_t binaryContentSize = m_package->GetBinaryContentSize(i);

			if (binaryContentSize > std::numeric_limits<FilePos_t>::max())
				return ResultCode::kIntegerOverflow;

			m_binaryContents[i].m_filePos = binaryContentStart;

			RKIT_CHECK(SafeAdd(binaryContentStart, binaryContentStart, static_cast<FilePos_t>(binaryContentSize)));
		}

		// Samplers
		{
			data::IRenderRTTIListBase *samplers = m_package->GetIndexable(rkit::data::RenderRTTIIndexableStructType::SamplerDesc);
			const size_t numSamplers = samplers->GetCount();

			RKIT_CHECK(m_samplers.Resize(numSamplers));

			for (VkSampler &s : m_samplers)
				s = VK_NULL_HANDLE;

			for (size_t i = 0; i < numSamplers; i++)
			{
				const render::SamplerDesc *sampler = static_cast<const render::SamplerDesc *>(samplers->GetElementPtr(i));

				VkSamplerCreateInfo samplerCreateInfo = {};
				samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
				RKIT_CHECK(ResolveFilter(samplerCreateInfo.magFilter, ResolveConfigurable(sampler->m_magFilter)));
				RKIT_CHECK(ResolveFilter(samplerCreateInfo.minFilter, ResolveConfigurable(sampler->m_minFilter)));
				RKIT_CHECK(ResolveSamplerMipMapMode(samplerCreateInfo.mipmapMode, ResolveConfigurable(sampler->m_mipMapMode)));
				RKIT_CHECK(ResolveSamplerAddressMode(samplerCreateInfo.addressModeU, ResolveConfigurable(sampler->m_addressModeU)));
				RKIT_CHECK(ResolveSamplerAddressMode(samplerCreateInfo.addressModeV, ResolveConfigurable(sampler->m_addressModeV)));
				RKIT_CHECK(ResolveSamplerAddressMode(samplerCreateInfo.addressModeW, ResolveConfigurable(sampler->m_addressModeW)));
				samplerCreateInfo.mipLodBias = ResolveConfigurable(sampler->m_mipLodBias);
				samplerCreateInfo.minLod = ResolveConfigurable(sampler->m_minLod);

				if (sampler->m_maxLod.m_state == ConfigurableValueState::Default)
					samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
				else
					samplerCreateInfo.maxLod = ResolveConfigurable(sampler->m_maxLod);

				ComparisonFunction cmpFunction = ResolveConfigurable(sampler->m_compareFunction);

				if (cmpFunction != ComparisonFunction::Disabled)
				{
					samplerCreateInfo.compareEnable = VK_TRUE;
					RKIT_CHECK(ResolveCompareOp(samplerCreateInfo.compareOp, cmpFunction));
				}

				AnisotropicFiltering anisoFiltering = ResolveConfigurable(sampler->m_anisotropy);
				if (anisoFiltering != AnisotropicFiltering::Disabled)
				{
					samplerCreateInfo.anisotropyEnable = VK_TRUE;
					RKIT_CHECK(ResolveAnisotropy(samplerCreateInfo.maxAnisotropy, anisoFiltering));
				}

				RKIT_VK_CHECK(vkd.vkCreateSampler(m_device.GetDevice(), &samplerCreateInfo, m_device.GetAllocCallbacks(), &m_samplers[i]));
			}
		}

		// Descriptor set layouts
		{
			data::IRenderRTTIListBase *descriptorLayouts = m_package->GetIndexable(rkit::data::RenderRTTIIndexableStructType::DescriptorLayoutDesc);
			const size_t numDescriptorLayouts = descriptorLayouts->GetCount();

			RKIT_CHECK(m_descriptorSetLayouts.Resize(numDescriptorLayouts));

			for (VkDescriptorSetLayout &dsl : m_descriptorSetLayouts)
				dsl = VK_NULL_HANDLE;

			for (size_t dli = 0; dli < numDescriptorLayouts; dli++)
			{
				const render::DescriptorLayoutDesc *dslDesc = static_cast<const render::DescriptorLayoutDesc *>(descriptorLayouts->GetElementPtr(dli));

				Vector<VkDescriptorSetLayoutBinding> bindings;
				RKIT_CHECK(bindings.Resize(dslDesc->m_descriptors.Count()));

				Vector<Vector<VkSampler>> immutableSamplers;
				RKIT_CHECK(immutableSamplers.Resize(dslDesc->m_descriptors.Count()));

				for (size_t di = 0; di < dslDesc->m_descriptors.Count(); di++)
				{
					Vector<VkSampler> &bindingImmutableSamplers = immutableSamplers[di];
					VkDescriptorSetLayoutBinding &vkBinding = bindings[di];
					const DescriptorDesc *descriptorDesc = dslDesc->m_descriptors[di];

					vkBinding = {};
					RKIT_CHECK(ResolveGraphicsStageFlags(vkBinding.stageFlags, descriptorDesc->m_visibility));
					RKIT_CHECK(ResolveDescriptorType(vkBinding.descriptorType, descriptorDesc->m_descriptorType));

					if (descriptorDesc->m_arraySize == 0)
					{
						rkit::log::Error("Pipeline contains unbounded descriptors, but device doesn't support unbounded descriptors");
						return ResultCode::kOperationFailed;
					}

					if (descriptorDesc->m_staticSamplerDesc)
					{
						if (vkBinding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
							vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						else
						{
							if (vkBinding.descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER)
							{
								rkit::log::Error("Pipeline contains a static sampler assigned to a descriptor that isn't a sampler or sampled texture");
								return ResultCode::kOperationFailed;
							}
						}

						VkSampler sampler = VK_NULL_HANDLE;
						RKIT_CHECK(FindSampler(sampler, descriptorDesc->m_staticSamplerDesc));

						RKIT_CHECK(bindingImmutableSamplers.Resize(descriptorDesc->m_arraySize));

						for (VkSampler &samplerRef : bindingImmutableSamplers)
							samplerRef = sampler;
					}

					vkBinding.descriptorCount = descriptorDesc->m_arraySize;
					vkBinding.pImmutableSamplers = bindingImmutableSamplers.GetBuffer();
					vkBinding.binding = static_cast<uint32_t>(di);

					// possibly unused: m_valueType;
				}

				VkDescriptorSetLayoutCreateInfo dslCreateInfo = {};
				dslCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				dslCreateInfo.bindingCount = static_cast<uint32_t>(bindings.Count());
				dslCreateInfo.pBindings = bindings.GetBuffer();

				RKIT_VK_CHECK(vkd.vkCreateDescriptorSetLayout(m_device.GetDevice(), &dslCreateInfo, m_device.GetAllocCallbacks(), &m_descriptorSetLayouts[dli]));
			}
		}

		// Pipeline layouts
		{
			data::IRenderRTTIListBase *pipelineLayouts = m_package->GetIndexable(rkit::data::RenderRTTIIndexableStructType::PipelineLayoutDesc);
			const size_t numPipelineLayouts = pipelineLayouts->GetCount();

			RKIT_CHECK(m_pipelineLayouts.Resize(numPipelineLayouts));

			for (VkPipelineLayout &pl : m_pipelineLayouts)
				pl = VK_NULL_HANDLE;

			for (size_t pi = 0; pi < numPipelineLayouts; pi++)
			{
				const render::PipelineLayoutDesc *plDesc = static_cast<const render::PipelineLayoutDesc *>(pipelineLayouts->GetElementPtr(pi));

				VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
				pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

				Vector<VkDescriptorSetLayout> setLayouts;
				RKIT_CHECK(setLayouts.Resize(plDesc->m_descriptorLayouts.Count()));

				for (size_t dli = 0; dli < plDesc->m_descriptorLayouts.Count(); dli++)
				{
					RKIT_CHECK(FindDescriptorSetLayout(setLayouts[dli], plDesc->m_descriptorLayouts[dli]));
				}

				pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.Count());
				pipelineLayoutCreateInfo.pSetLayouts = setLayouts.GetBuffer();

				Vector<VkPushConstantRange> pcRanges;

				if (plDesc->m_pushConstantList)
				{
					for (const PushConstantDesc *pcDesc : plDesc->m_pushConstantList->m_pushConstants)
					{
						VkPushConstantRange pcRange = {};
						RKIT_CHECK(ResolveAnyStageFlags(pcRange.stageFlags, pcDesc->m_stageVisibility));
						pcRange.offset = 0;
						RKIT_CHECK(ResolvePushConstantSize(pcRange.size, pcDesc->m_type));

						if (pcRange.size == 0)
							continue;

						if (pcRanges.Count() != 0)
						{
							VkPushConstantRange &prevRange = pcRanges[pcRanges.Count() - 1];
							if (pcRange.stageFlags == prevRange.stageFlags)
								prevRange.size += pcRange.size;
							else
							{
								pcRange.offset += prevRange.offset;
								RKIT_CHECK(pcRanges.Append(pcRange));
							}
						}
						else
						{
							RKIT_CHECK(pcRanges.Append(pcRange));
						}
					}
				}

				pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(pcRanges.Count());
				pipelineLayoutCreateInfo.pPushConstantRanges = pcRanges.GetBuffer();

				RKIT_VK_CHECK(vkd.vkCreatePipelineLayout(m_device.GetDevice(), &pipelineLayoutCreateInfo, m_device.GetAllocCallbacks(), &m_pipelineLayouts[pi]));
			}
		}

		// Render passes
		{
			data::IRenderRTTIListBase *renderPasses = m_package->GetIndexable(rkit::data::RenderRTTIIndexableStructType::RenderPassDesc);
			const size_t numRenderPasses = renderPasses->GetCount();

			RKIT_CHECK(m_renderPasses.Resize(numRenderPasses));

			for (VkRenderPass &pl : m_renderPasses)
				pl = VK_NULL_HANDLE;

			for (size_t rpi = 0; rpi < numRenderPasses; rpi++)
			{
				RKIT_CHECK(CreateRenderPass(m_renderPasses[rpi], *static_cast<const render::RenderPassDesc *>(renderPasses->GetElementPtr(rpi))));
			}
		}

		return ResultCode::kOK;
	}

	void PipelineLibraryLoader::SetMergedLibraryStream(UniquePtr<ISeekableReadStream> &&cacheReadStream, ISeekableReadWriteStream *cacheWriteStream)
	{
		m_cacheReadStream = std::move(cacheReadStream);
		m_cacheWriteStream = cacheWriteStream;
	}

	Result PipelineLibraryLoader::OpenMergedLibrary()
	{
		if (!m_cacheReadStream.IsValid())
		{
			return Result::SoftFault(ResultCode::kOperationFailed);
		}

		PipelineCacheHeader header = {};

		RKIT_CHECK(m_cacheReadStream->SeekStart(0));
		RKIT_CHECK(m_cacheReadStream->ReadAll(&header, sizeof(header)));

		if (header.m_identifier != PipelineCacheHeader::kExpectedIdentifier
			|| header.m_version != PipelineCacheHeader::kExpectedVersion
			|| header.m_phase != PipelineCacheHeader::Phase::kMerged
			|| memcmp(&header.m_packageUUID, &m_package->GetPackageUUID(), sizeof(header.m_packageUUID)))
		{
			return Result::SoftFault(ResultCode::kOperationFailed);
		}

		FilePos_t cacheSize = m_cacheReadStream->GetSize() - sizeof(header);

		if (cacheSize < sizeof(VkPipelineCacheHeaderVersionOne))
			return Result::SoftFault(ResultCode::kOperationFailed);

		if (cacheSize > std::numeric_limits<size_t>::max())
			return ResultCode::kOperationFailed;

		Vector<uint8_t> mergedData;
		RKIT_CHECK(mergedData.Resize(static_cast<size_t>(cacheSize)));

		RKIT_CHECK(m_cacheReadStream->ReadAll(mergedData.GetBuffer(), mergedData.Count()));

		rkit::endian::LittleUInt32_t headerFields[4];
		memcpy(&headerFields, mergedData.GetBuffer(), 16);

		VkPipelineCacheHeaderVersionOne vkCacheHeader;
		vkCacheHeader.headerSize = headerFields[0].Get();
		vkCacheHeader.headerVersion = static_cast<VkPipelineCacheHeaderVersion>(headerFields[1].Get());
		vkCacheHeader.vendorID = headerFields[2].Get();
		vkCacheHeader.deviceID = headerFields[3].Get();
		memcpy(vkCacheHeader.pipelineCacheUUID, mergedData.GetBuffer() + 16, VK_UUID_SIZE);

		// TODO: Pipeline compatibility check
		const RenderVulkanPhysicalDevice &physDevice = m_device.GetPhysDevice();
		const VkPhysicalDeviceProperties &deviceProps = physDevice.GetPhysDeviceProperties();

		if (vkCacheHeader.headerVersion != VK_PIPELINE_CACHE_HEADER_VERSION_ONE
			|| vkCacheHeader.headerSize != sizeof(VkPipelineCacheHeaderVersionOne)
			|| vkCacheHeader.vendorID != deviceProps.vendorID
			|| vkCacheHeader.deviceID != deviceProps.deviceID
			|| memcmp(vkCacheHeader.pipelineCacheUUID, deviceProps.pipelineCacheUUID, VK_UUID_SIZE))
		{
			// Pipeline cache mismatch
			return Result::SoftFault(ResultCode::kOperationFailed);
		}

		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		pipelineCacheCreateInfo.pInitialData = mergedData.GetBuffer();
		pipelineCacheCreateInfo.initialDataSize = mergedData.Count();

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkCreatePipelineCache(m_device.GetDevice(), &pipelineCacheCreateInfo, m_device.GetAllocCallbacks(), &m_mergedCache));

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::LoadGraphicsPipelineFromMergedLibrary(size_t pipelineIndex, size_t permutationIndex)
	{
		if (m_mergedCache == VK_NULL_HANDLE)
			return ResultCode::kInternalError;

		VkPipeline &outPipelineRef = m_allPipelines[m_graphicPipelinePermutationStarts[pipelineIndex] + permutationIndex];
		if (outPipelineRef != VK_NULL_HANDLE)
			return ResultCode::kInternalError;

		CompiledPipeline compiledPipeline;
		RKIT_CHECK(CheckedCreateGraphicsPipeline(m_mergedCache, compiledPipeline, pipelineIndex, permutationIndex));

		outPipelineRef = compiledPipeline.m_pipeline;

		return ResultCode::kOK;
	}

	void PipelineLibraryLoader::CloseMergedLibrary(bool unloadPipelines, bool unloadMergedCache)
	{
		if (unloadPipelines)
		{
			for (VkPipeline &pipelineRef : m_allPipelines)
			{
				if (pipelineRef != VK_NULL_HANDLE)
				{
					m_device.GetDeviceAPI().vkDestroyPipeline(m_device.GetDevice(), pipelineRef, m_device.GetAllocCallbacks());
					pipelineRef = VK_NULL_HANDLE;
				}
			}
		}

		if (unloadMergedCache)
		{
			if (m_mergedCache != VK_NULL_HANDLE)
			{
				m_device.GetDeviceAPI().vkDestroyPipelineCache(m_device.GetDevice(), m_mergedCache, m_device.GetAllocCallbacks());
				m_mergedCache = VK_NULL_HANDLE;
			}
		}

		m_cacheReadStream.Reset();
		m_cacheWriteStream = nullptr;
	}

	Result PipelineLibraryLoader::CompileUnmergedGraphicsPipeline(size_t pipelineIndex, size_t permutationIndex)
	{
		VkPipelineCache pipelineCache = VK_NULL_HANDLE;
		CompiledPipeline pipeline;

		Result result = CheckedCompileUnmergedGraphicsPipeline(pipelineCache, pipeline, pipelineIndex, permutationIndex);

		const VulkanDeviceAPI &vkd = m_device.GetDeviceAPI();

		if (pipeline.m_pipeline != VK_NULL_HANDLE)
			vkd.vkDestroyPipeline(m_device.GetDevice(), pipeline.m_pipeline, m_device.GetAllocCallbacks());

		if (pipelineCache != VK_NULL_HANDLE)
			vkd.vkDestroyPipelineCache(m_device.GetDevice(), pipelineCache, m_device.GetAllocCallbacks());

		return result;
	}

	Result PipelineLibraryLoader::ResolveVertexInputAttributeFormat(VkFormat &outVkFormat, const VectorOrScalarNumericType &vectorType)
	{
		switch (vectorType.m_cols)
		{
		case VectorOrScalarDimension::Scalar:
			return ResolveVertexInputAttributeFormat_Scalar(outVkFormat, vectorType.m_numericType);
		case VectorOrScalarDimension::Dimension2:
			return ResolveVertexInputAttributeFormat_V2(outVkFormat, vectorType.m_numericType);
		case VectorOrScalarDimension::Dimension3:
			return ResolveVertexInputAttributeFormat_V3(outVkFormat, vectorType.m_numericType);
		case VectorOrScalarDimension::Dimension4:
			return ResolveVertexInputAttributeFormat_V4(outVkFormat, vectorType.m_numericType);
		default:
			return ResultCode::kInternalError;
		}
	}

	Result PipelineLibraryLoader::ResolveRenderTargetFormat(VkFormat &outVkFormat, RenderTargetFormat rtFormat)
	{
		switch (rtFormat)
		{
		case RenderTargetFormat::RGBA_UNorm8:
			outVkFormat = VK_FORMAT_R8G8B8A8_UNORM;
			break;
		case RenderTargetFormat::RGBA_UNorm8_sRGB:
			outVkFormat = VK_FORMAT_R8G8B8A8_SRGB;
			break;
		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveDepthStencilFormat(VkFormat &outVkFormat, DepthStencilFormat dsFormat)
	{
		switch (dsFormat)
		{
		case DepthStencilFormat::DepthFloat32:
			outVkFormat = VK_FORMAT_D32_SFLOAT;
			break;
		case DepthStencilFormat::DepthFloat32_Stencil8_Undefined24:
			outVkFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
			break;
		case DepthStencilFormat::DepthUNorm24_Stencil8:
			outVkFormat = VK_FORMAT_D24_UNORM_S8_UINT;
			break;
		case DepthStencilFormat::DepthUNorm16:
			outVkFormat = VK_FORMAT_D16_UNORM;
			break;
		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveDepthStencilFormatHasStencil(bool &outHaveStencil, DepthStencilFormat dsFormat)
	{
		switch (dsFormat)
		{
		case DepthStencilFormat::DepthFloat32_Stencil8_Undefined24:
		case DepthStencilFormat::DepthUNorm24_Stencil8:
			outHaveStencil = true;
			break;
		case DepthStencilFormat::DepthFloat32:
		case DepthStencilFormat::DepthUNorm16:
			outHaveStencil = false;
			break;
		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}


	Result PipelineLibraryLoader::ResolveColorBlendFactor(VkBlendFactor &outBlendFactor, ColorBlendFactor cbf)
	{
		switch (cbf)
		{
		case ColorBlendFactor::Zero:
			outBlendFactor = VK_BLEND_FACTOR_ZERO;
			break;
		case ColorBlendFactor::One:
			outBlendFactor = VK_BLEND_FACTOR_ONE;
			break;
		case ColorBlendFactor::SrcColor:
			outBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
			break;
		case ColorBlendFactor::InvSrcColor:
			outBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
			break;
		case ColorBlendFactor::SrcAlpha:
			outBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			break;
		case ColorBlendFactor::InvSrcAlpha:
			outBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			break;
		case ColorBlendFactor::DstAlpha:
			outBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
			break;
		case ColorBlendFactor::InvDstAlpha:
			outBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
			break;
		case ColorBlendFactor::DstColor:
			outBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
			break;
		case ColorBlendFactor::InvDstColor:
			outBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
			break;
		case ColorBlendFactor::ConstantColor:
			outBlendFactor = VK_BLEND_FACTOR_CONSTANT_COLOR;
			break;
		case ColorBlendFactor::InvConstantColor:
			outBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
			break;
		case ColorBlendFactor::ConstantAlpha:
			outBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
			break;
		case ColorBlendFactor::InvConstantAlpha:
			outBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
			break;
		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveAlphaBlendFactor(VkBlendFactor &outBlendFactor, AlphaBlendFactor abf)
	{
		switch (abf)
		{
		case AlphaBlendFactor::Zero:
			outBlendFactor = VK_BLEND_FACTOR_ZERO;
			break;
		case AlphaBlendFactor::One:
			outBlendFactor = VK_BLEND_FACTOR_ONE;
			break;
		case AlphaBlendFactor::SrcAlpha:
			outBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			break;
		case AlphaBlendFactor::InvSrcAlpha:
			outBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			break;
		case AlphaBlendFactor::DstAlpha:
			outBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
			break;
		case AlphaBlendFactor::InvDstAlpha:
			outBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
			break;
		case AlphaBlendFactor::ConstantAlpha:
			outBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
			break;
		case AlphaBlendFactor::InvConstantAlpha:
			outBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
			break;
		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveBlendOp(VkBlendOp &outBlendOp, BlendOp bo)
	{
		switch (bo)
		{
		case BlendOp::Add:
			outBlendOp = VK_BLEND_OP_ADD;
			break;
		case BlendOp::Subtract:
			outBlendOp = VK_BLEND_OP_SUBTRACT;
			break;
		case BlendOp::ReverseSubtract:
			outBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
			break;
		case BlendOp::Min:
			outBlendOp = VK_BLEND_OP_MIN;
			break;
		case BlendOp::Max:
			outBlendOp = VK_BLEND_OP_MAX;
			break;
		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveVertexInputAttributeFormat_Scalar(VkFormat &outVkFormat, NumericType numericType)
	{
		switch (numericType)
		{
		case NumericType::Float16:
			outVkFormat = VK_FORMAT_R16_SFLOAT;
			break;
		case NumericType::Float32:
			outVkFormat = VK_FORMAT_R32_SFLOAT;
			break;
		case NumericType::Float64:
			outVkFormat = VK_FORMAT_R64_SFLOAT;
			break;

		case NumericType::SInt8:
			outVkFormat = VK_FORMAT_R8_SINT;
			break;
		case NumericType::SInt16:
			outVkFormat = VK_FORMAT_R16_SINT;
			break;
		case NumericType::SInt32:
			outVkFormat = VK_FORMAT_R32_SINT;
			break;
		case NumericType::SInt64:
			outVkFormat = VK_FORMAT_R64_SINT;
			break;

		case NumericType::UInt8:
			outVkFormat = VK_FORMAT_R8_UINT;
			break;
		case NumericType::UInt16:
			outVkFormat = VK_FORMAT_R16_UINT;
			break;
		case NumericType::UInt32:
			outVkFormat = VK_FORMAT_R32_UINT;
			break;
		case NumericType::UInt64:
			outVkFormat = VK_FORMAT_R64_UINT;
			break;

		case NumericType::SNorm8:
			outVkFormat = VK_FORMAT_R8_SNORM;
			break;
		case NumericType::SNorm16:
			outVkFormat = VK_FORMAT_R16_SNORM;
			break;

		case NumericType::UNorm8:
			outVkFormat = VK_FORMAT_R8_SNORM;
			break;
		case NumericType::UNorm16:
			outVkFormat = VK_FORMAT_R16_UNORM;
			break;

		default:
			return ResultCode::kMalformedFile;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveVertexInputAttributeFormat_V2(VkFormat &outVkFormat, NumericType numericType)
	{
		switch (numericType)
		{
		case NumericType::Float16:
			outVkFormat = VK_FORMAT_R16G16_SFLOAT;
			break;
		case NumericType::Float32:
			outVkFormat = VK_FORMAT_R32G32_SFLOAT;
			break;
		case NumericType::Float64:
			outVkFormat = VK_FORMAT_R64G64_SFLOAT;
			break;

		case NumericType::SInt8:
			outVkFormat = VK_FORMAT_R8G8_SINT;
			break;
		case NumericType::SInt16:
			outVkFormat = VK_FORMAT_R16G16_SINT;
			break;
		case NumericType::SInt32:
			outVkFormat = VK_FORMAT_R32G32_SINT;
			break;
		case NumericType::SInt64:
			outVkFormat = VK_FORMAT_R64G64_SINT;
			break;

		case NumericType::UInt8:
			outVkFormat = VK_FORMAT_R8G8_UINT;
			break;
		case NumericType::UInt16:
			outVkFormat = VK_FORMAT_R16G16_UINT;
			break;
		case NumericType::UInt32:
			outVkFormat = VK_FORMAT_R32G32_UINT;
			break;
		case NumericType::UInt64:
			outVkFormat = VK_FORMAT_R64G64_UINT;
			break;

		case NumericType::SNorm8:
			outVkFormat = VK_FORMAT_R8G8_SNORM;
			break;
		case NumericType::SNorm16:
			outVkFormat = VK_FORMAT_R16G16_SNORM;
			break;

		case NumericType::UNorm8:
			outVkFormat = VK_FORMAT_R8G8_SNORM;
			break;
		case NumericType::UNorm16:
			outVkFormat = VK_FORMAT_R16G16_UNORM;
			break;

		default:
			return ResultCode::kMalformedFile;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveVertexInputAttributeFormat_V3(VkFormat &outVkFormat, NumericType numericType)
	{
		switch (numericType)
		{
		case NumericType::Float16:
			outVkFormat = VK_FORMAT_R16G16B16_SFLOAT;
			break;
		case NumericType::Float32:
			outVkFormat = VK_FORMAT_R32G32B32_SFLOAT;
			break;
		case NumericType::Float64:
			outVkFormat = VK_FORMAT_R64G64B64_SFLOAT;
			break;

		case NumericType::SInt8:
			outVkFormat = VK_FORMAT_R8G8B8_SINT;
			break;
		case NumericType::SInt16:
			outVkFormat = VK_FORMAT_R16G16B16_SINT;
			break;
		case NumericType::SInt32:
			outVkFormat = VK_FORMAT_R32G32B32_SINT;
			break;
		case NumericType::SInt64:
			outVkFormat = VK_FORMAT_R64G64B64_SINT;
			break;

		case NumericType::UInt8:
			outVkFormat = VK_FORMAT_R8G8B8_UINT;
			break;
		case NumericType::UInt16:
			outVkFormat = VK_FORMAT_R16G16B16_UINT;
			break;
		case NumericType::UInt32:
			outVkFormat = VK_FORMAT_R32G32B32_UINT;
			break;
		case NumericType::UInt64:
			outVkFormat = VK_FORMAT_R64G64B64_UINT;
			break;

		case NumericType::SNorm8:
			outVkFormat = VK_FORMAT_R8G8B8_SNORM;
			break;
		case NumericType::SNorm16:
			outVkFormat = VK_FORMAT_R16G16B16_SNORM;
			break;

		case NumericType::UNorm8:
			outVkFormat = VK_FORMAT_R8G8B8_SNORM;
			break;
		case NumericType::UNorm16:
			outVkFormat = VK_FORMAT_R16G16B16_UNORM;
			break;

		default:
			return ResultCode::kMalformedFile;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveVertexInputAttributeFormat_V4(VkFormat &outVkFormat, NumericType numericType)
	{

		switch (numericType)
		{
		case NumericType::Float16:
			outVkFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
			break;
		case NumericType::Float32:
			outVkFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
			break;
		case NumericType::Float64:
			outVkFormat = VK_FORMAT_R64G64B64A64_SFLOAT;
			break;

		case NumericType::SInt8:
			outVkFormat = VK_FORMAT_R8G8B8A8_SINT;
			break;
		case NumericType::SInt16:
			outVkFormat = VK_FORMAT_R16G16B16A16_SINT;
			break;
		case NumericType::SInt32:
			outVkFormat = VK_FORMAT_R32G32B32A32_SINT;
			break;
		case NumericType::SInt64:
			outVkFormat = VK_FORMAT_R64G64B64A64_SINT;
			break;

		case NumericType::UInt8:
			outVkFormat = VK_FORMAT_R8G8B8A8_UINT;
			break;
		case NumericType::UInt16:
			outVkFormat = VK_FORMAT_R16G16B16A16_UINT;
			break;
		case NumericType::UInt32:
			outVkFormat = VK_FORMAT_R32G32B32A32_UINT;
			break;
		case NumericType::UInt64:
			outVkFormat = VK_FORMAT_R64G64B64A64_UINT;
			break;

		case NumericType::SNorm8:
			outVkFormat = VK_FORMAT_R8G8B8A8_SNORM;
			break;
		case NumericType::SNorm16:
			outVkFormat = VK_FORMAT_R16G16B16A16_SNORM;
			break;

		case NumericType::UNorm8:
			outVkFormat = VK_FORMAT_R8G8B8A8_SNORM;
			break;
		case NumericType::UNorm16:
			outVkFormat = VK_FORMAT_R16G16B16A16_UNORM;
			break;

		default:
			return ResultCode::kMalformedFile;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveStencilOps(VkStencilOpState &outOpState, const StencilOpDesc &stencilOpDesc, const DepthStencilOperationDesc &depthStencilDesc)
	{
		RKIT_CHECK(ResolveStencilOp(outOpState.failOp, ResolveConfigurable(stencilOpDesc.m_failOp)));
		RKIT_CHECK(ResolveStencilOp(outOpState.passOp, ResolveConfigurable(stencilOpDesc.m_passOp)));
		RKIT_CHECK(ResolveStencilOp(outOpState.depthFailOp, ResolveConfigurable(stencilOpDesc.m_depthFailOp)));
		RKIT_CHECK(ResolveCompareOp(outOpState.compareOp, ResolveConfigurable(stencilOpDesc.m_compareFunc)));

		outOpState.compareMask = ResolveConfigurable(depthStencilDesc.m_stencilCompareMask);

		if (ResolveConfigurable(depthStencilDesc.m_stencilWrite))
			outOpState.writeMask = ResolveConfigurable(depthStencilDesc.m_stencilWriteMask);

		outOpState.reference = ResolveConfigurable(depthStencilDesc.m_stencilReference);

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveStencilOp(VkStencilOp &outOp, StencilOp stencilOp)
	{
		switch (stencilOp)
		{
		case StencilOp::Keep:
			outOp = VK_STENCIL_OP_KEEP;
			break;
		case StencilOp::Zero:
			outOp = VK_STENCIL_OP_ZERO;
			break;
		case StencilOp::Replace:
			outOp = VK_STENCIL_OP_REPLACE;
			break;
		case StencilOp::IncrementSaturate:
			outOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
			break;
		case StencilOp::DecrementSaturate:
			outOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP;
			break;
		case StencilOp::Invert:
			outOp = VK_STENCIL_OP_INVERT;
			break;
		case StencilOp::Increment:
			outOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
			break;
		case StencilOp::Decrement:
			outOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;
			break;
		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveCompareOp(VkCompareOp &outOp, ComparisonFunction compareFunc)
	{
		switch (compareFunc)
		{
		case ComparisonFunction::Always:
			outOp = VK_COMPARE_OP_NEVER;
			break;
		case ComparisonFunction::Never:
			outOp = VK_COMPARE_OP_NEVER;
			break;
		case ComparisonFunction::Equal:
			outOp = VK_COMPARE_OP_EQUAL;
			break;
		case ComparisonFunction::NotEqual:
			outOp = VK_COMPARE_OP_NOT_EQUAL;
			break;
		case ComparisonFunction::Less:
			outOp = VK_COMPARE_OP_LESS;
			break;
		case ComparisonFunction::LessOrEqual:
			outOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			break;
		case ComparisonFunction::Greater:
			outOp = VK_COMPARE_OP_GREATER;
			break;
		case ComparisonFunction::GreaterOrEqual:
			outOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
			break;
		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveAnyStageFlags(VkPipelineStageFlags &outStageFlags, StageVisibility visibility)
	{
		switch (visibility)
		{
		case StageVisibility::All:
			outStageFlags = VK_SHADER_STAGE_ALL;
			break;
		case StageVisibility::Vertex:
			outStageFlags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
			break;
		case StageVisibility::Pixel:
			outStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;
		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveGraphicsStageFlags(VkPipelineStageFlags &outStageFlags, StageVisibility visibility)
	{
		switch (visibility)
		{
		case StageVisibility::All:
			outStageFlags = VK_SHADER_STAGE_ALL;
			break;
		case StageVisibility::Vertex:
			outStageFlags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
			break;
		case StageVisibility::Pixel:
			outStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;
		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveDescriptorType(VkDescriptorType &outDescType, DescriptorType descType)
	{
		switch (descType)
		{
		case DescriptorType::Sampler:
			outDescType = VK_DESCRIPTOR_TYPE_SAMPLER;
			break;

		case DescriptorType::StaticConstantBuffer:
		case DescriptorType::DynamicConstantBuffer:
			outDescType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			break;

		case DescriptorType::Buffer:
		case DescriptorType::RWBuffer:
		case DescriptorType::ByteAddressBuffer:
		case DescriptorType::RWByteAddressBuffer:
			outDescType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			break;

		case DescriptorType::Texture1D:
		case DescriptorType::Texture1DArray:
		case DescriptorType::Texture2D:
		case DescriptorType::Texture2DArray:
		case DescriptorType::Texture2DMS:
		case DescriptorType::Texture2DMSArray:
		case DescriptorType::Texture3D:
		case DescriptorType::TextureCube:
		case DescriptorType::TextureCubeArray:
			outDescType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			break;

		case DescriptorType::RWTexture1D:
		case DescriptorType::RWTexture1DArray:
		case DescriptorType::RWTexture2D:
		case DescriptorType::RWTexture2DArray:
		case DescriptorType::RWTexture3D:
			outDescType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			break;

		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveFilter(VkFilter &outFilter, Filter filter)
	{
		switch (filter)
		{
		case Filter::Linear:
			outFilter = VK_FILTER_LINEAR;
			break;
		case Filter::Nearest:
			outFilter = VK_FILTER_NEAREST;
			break;

		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveSamplerMipMapMode(VkSamplerMipmapMode &outMipmapMode, MipMapMode mipmapMode)
	{
		switch (mipmapMode)
		{
		case MipMapMode::Linear:
			outMipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			break;
		case MipMapMode::Nearest:
			outMipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;

		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveSamplerAddressMode(VkSamplerAddressMode &outAddressMode, AddressMode addressMode)
	{
		switch (addressMode)
		{
		case AddressMode::Repeat:
			outAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			break;
		case AddressMode::MirrorRepeat:
			outAddressMode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			break;
		case AddressMode::ClampEdge:
			outAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			break;
		case AddressMode::ClampBorder:
			outAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			break;

		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolveAnisotropy(float &outMaxAnisotropy, AnisotropicFiltering anisoFiltering)
	{
		switch (anisoFiltering)
		{
		case AnisotropicFiltering::Anisotropic1:
			outMaxAnisotropy = 1.0f;
			break;
		case AnisotropicFiltering::Anisotropic2:
			outMaxAnisotropy = 2.0f;
			break;
		case AnisotropicFiltering::Anisotropic4:
			outMaxAnisotropy = 4.0f;
			break;
		case AnisotropicFiltering::Anisotropic8:
			outMaxAnisotropy = 8.0f;
			break;
		case AnisotropicFiltering::Anisotropic16:
			outMaxAnisotropy = 16.0f;
			break;

		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolvePushConstantSize(uint32_t &size, const ValueType &valueType)
	{
		switch (valueType.m_type)
		{
		case ValueTypeType::Numeric:
			return ResolvePushConstantSize(size, valueType.m_value.m_numericType);
		case ValueTypeType::CompoundNumeric:
			return ResolvePushConstantSize(size, valueType.m_value.m_compoundNumericType);
		case ValueTypeType::VectorNumeric:
			return ResolvePushConstantSize(size, valueType.m_value.m_vectorNumericType);
		case ValueTypeType::Structure:
			return ResolvePushConstantSize(size, valueType.m_value.m_structureType);
		default:
			return ResultCode::kInternalError;
		}
	}

	Result PipelineLibraryLoader::ResolvePushConstantSize(uint32_t &size, const CompoundNumericType &valueType)
	{
		uint8_t rowsInt = static_cast<uint8_t>(static_cast<int>(valueType.m_rows) - static_cast<int>(VectorDimension::Dimension2) + 2);
		uint8_t colsInt = static_cast<uint8_t>(static_cast<int>(valueType.m_cols) - static_cast<int>(VectorDimension::Dimension2) + 2);
		uint32_t numElements = rowsInt * colsInt;

		uint32_t numberSize = 0;
		RKIT_CHECK(ResolvePushConstantSize(numberSize, valueType.m_numericType));

		RKIT_CHECK(SafeMul(size, numberSize, numElements));

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolvePushConstantSize(uint32_t &size, const VectorNumericType &valueType)
	{
		uint32_t colsInt  = static_cast<size_t>(static_cast<int>(valueType.m_cols) - static_cast<int>(VectorDimension::Dimension2) + 2);

		uint32_t numberSize = 0;
		RKIT_CHECK(ResolvePushConstantSize(numberSize, valueType.m_numericType));

		RKIT_CHECK(SafeMul(size, numberSize, colsInt));

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::ResolvePushConstantSize(uint32_t &size, NumericType numericType)
	{
		switch (numericType)
		{
		case NumericType::Float32:
		case NumericType::SInt32:
		case NumericType::UInt32:
			size = 4;
			break;
		default:
			rkit::log::Error("Push constants element was not DWORD-sized");
			return ResultCode::kMalformedFile;
		}

		return ResultCode::kOK;
	}

#define RKIT_VK_FIND_FROM_LIST(type, object)\
	size_t index = 0;\
	do {\
		const data::IRenderRTTIListBase *list = m_package->GetIndexable(data::RenderRTTIIndexableStructType::type);\
		if (list->GetCount() == 0)\
			return ResultCode::kOperationFailed;\
		const type *firstElement = static_cast<const type *>(list->GetElementPtr(0));\
		index = static_cast<size_t>(object - firstElement);\
	} while (false)

	Result PipelineLibraryLoader::FindSampler(VkSampler &outSampler, const SamplerDesc *sampler) const
	{
		RKIT_VK_FIND_FROM_LIST(SamplerDesc, sampler);

		if (m_samplers[index] == VK_NULL_HANDLE)
			return ResultCode::kOperationFailed;

		outSampler = m_samplers[index];
		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::FindDescriptorSetLayout(VkDescriptorSetLayout &outDSL, const DescriptorLayoutDesc *descLayout) const
	{
		RKIT_VK_FIND_FROM_LIST(DescriptorLayoutDesc, descLayout);

		if (m_descriptorSetLayouts[index] == VK_NULL_HANDLE)
			return ResultCode::kOperationFailed;

		outDSL = m_descriptorSetLayouts[index];
		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::FindPipelineLayout(VkPipelineLayout &outPipelineLayout, const PipelineLayoutDesc *pipelineLayout) const
	{
		RKIT_VK_FIND_FROM_LIST(PipelineLayoutDesc, pipelineLayout);

		if (m_pipelineLayouts[index] == VK_NULL_HANDLE)
			return ResultCode::kOperationFailed;

		outPipelineLayout = m_pipelineLayouts[index];
		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::FindRenderPass(VkRenderPass &outRenderPass, const RenderPassDesc *renderPass) const
	{
		RKIT_VK_FIND_FROM_LIST(RenderPassDesc, renderPass);

		if (m_renderPasses[index] == VK_NULL_HANDLE)
			return ResultCode::kOperationFailed;

		outRenderPass = m_renderPasses[index];
		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::CreateRenderPass(VkRenderPass &outRenderPass, const RenderPassDesc &renderPassDesc)
	{
		const DepthStencilTargetDesc *depthStencil = renderPassDesc.m_depthStencilTarget;

		const size_t numColorAttachments = renderPassDesc.m_renderTargets.Count();
		const size_t numDepthStencilAttachments = (depthStencil != nullptr) ? 1 : 0;

		size_t totalAttachments = numColorAttachments + numDepthStencilAttachments;

		Vector<VkAttachmentDescription> attachmentDescs;
		RKIT_CHECK(attachmentDescs.Resize(totalAttachments));

		const size_t firstDepthStencilAttachment = 0;
		const size_t firstColorAttachment = numDepthStencilAttachments;

		Vector<VkAttachmentReference> colorAttachmentRefs;
		VkAttachmentReference depthStencilAttachmentRef = {};

		RKIT_CHECK(colorAttachmentRefs.Resize(numColorAttachments));

		if (numDepthStencilAttachments > 0)
		{
			VkAttachmentDescription dsDesc = {};
			RKIT_CHECK(ResolveDepthStencilFormat(dsDesc.format, ResolveConfigurable(depthStencil->m_format)));

			dsDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			dsDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

			bool haveStencil = false;
			RKIT_CHECK(ResolveDepthStencilFormatHasStencil(haveStencil, ResolveConfigurable(depthStencil->m_format)));

			if (haveStencil)
			{
				dsDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				dsDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			}
			else
			{
				dsDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				dsDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			}

			dsDesc.samples = VK_SAMPLE_COUNT_1_BIT;
			dsDesc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			dsDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			attachmentDescs[firstDepthStencilAttachment] = dsDesc;

			dsDesc = {};
			depthStencilAttachmentRef.attachment = static_cast<uint32_t>(firstDepthStencilAttachment);
			depthStencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		for (size_t cai = 0; cai < numColorAttachments; cai++)
		{
			VkAttachmentDescription caDesc = {};
			const RenderTargetDesc *rtDesc = renderPassDesc.m_renderTargets[cai];

			RKIT_CHECK(ResolveRenderTargetFormat(caDesc.format, ResolveConfigurable(rtDesc->m_format)));
			caDesc.samples = VK_SAMPLE_COUNT_1_BIT;

			caDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			caDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			caDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			caDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			caDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			caDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			attachmentDescs[firstColorAttachment + cai] = caDesc;

			VkAttachmentReference &attachmentRef = colorAttachmentRefs[cai];

			attachmentRef = {};
			attachmentRef.attachment = static_cast<uint32_t>(firstColorAttachment + cai);
			attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		VkSubpassDescription subpassDesc = {};
		subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDesc.colorAttachmentCount = static_cast<uint32_t>(numColorAttachments);
		subpassDesc.pColorAttachments = colorAttachmentRefs.GetBuffer();

		if (numDepthStencilAttachments > 0)
			subpassDesc.pDepthStencilAttachment = &depthStencilAttachmentRef;

		//StaticArray<VkSubpassDependency, 1> subpassDeps;

		VkRenderPassCreateInfo renderPassCreateInfo = {};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

		renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(totalAttachments);
		renderPassCreateInfo.pAttachments = attachmentDescs.GetBuffer();
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpassDesc;
		//renderPassCreateInfo.dependencyCount = subpassDeps.Count();
		//renderPassCreateInfo.pDependencies = subpassDeps.GetBuffer();

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkCreateRenderPass(m_device.GetDevice(), &renderPassCreateInfo, m_device.GetAllocCallbacks(), &outRenderPass));

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::CheckedCreateGraphicsPipeline(VkPipelineCache &pipelineCache, CompiledPipeline &pipeline, size_t pipelineIndex, size_t permutationIndex)
	{
		const VulkanDeviceAPI &vkd = m_device.GetDeviceAPI();

		const render::GraphicsPipelineDesc *pipelineDesc = static_cast<const render::GraphicsPipelineDesc *>(m_package->GetIndexable(data::RenderRTTIIndexableStructType::GraphicsPipelineDesc)->GetElementPtr(pipelineIndex));
		const render::RenderPassDesc *renderPassDesc = pipelineDesc->m_executeInPass;

		VkPipeline &outPipelineRef = m_allPipelines[m_graphicPipelinePermutationStarts[pipelineIndex] + permutationIndex];

		const size_t kMaxGraphicsStages = static_cast<size_t>(GraphicPipelineStage::Count);

		const ConstSpan<const render::ContentKey *> contentKeys = pipelineDesc->m_compiledContentKeys;

		if (contentKeys.Count() % kMaxGraphicsStages != 0)
			return ResultCode::kMalformedFile;

		if (permutationIndex > contentKeys.Count() / kMaxGraphicsStages)
			return ResultCode::kMalformedFile;

		Vector<VkDynamicState> dynamicStates;
		RKIT_CHECK(dynamicStates.Append(VK_DYNAMIC_STATE_VIEWPORT));
		RKIT_CHECK(dynamicStates.Append(VK_DYNAMIC_STATE_SCISSOR));

		const InputLayoutDesc *inputLayoutDesc = pipelineDesc->m_inputLayout;

		Vector<const InputLayoutVertexFeedDesc *> feedRefs;
		Vector<VkVertexInputBindingDescription> vertexInputBindingDescs;

		Vector<VkVertexInputAttributeDescription> vertexAttributeDescs;
		RKIT_CHECK(vertexAttributeDescs.Resize(inputLayoutDesc->m_vertexInputs.Count()));

		for (size_t i = 0; i < inputLayoutDesc->m_vertexInputs.Count(); i++)
		{
			const render::InputLayoutVertexInputDesc *vertexInput = inputLayoutDesc->m_vertexInputs[i];
			VkVertexInputAttributeDescription &attribDesc = vertexAttributeDescs[i];

			attribDesc = {};
			attribDesc.location = static_cast<uint32_t>(i);
			attribDesc.binding = vertexInput->m_inputFeed->m_inputSlot;
			RKIT_CHECK(ResolveVertexInputAttributeFormat(attribDesc.format, *vertexInput->m_numericType));
			attribDesc.offset = vertexInput->m_byteOffset;

			const InputLayoutVertexFeedDesc *feed = vertexInput->m_inputFeed;

			bool foundFeed = false;
			for (const InputLayoutVertexFeedDesc *feedRef : feedRefs)
			{
				if (feedRef == feed)
				{
					foundFeed = true;
					break;
				}
			}

			if (!foundFeed)
			{
				RKIT_CHECK(feedRefs.Append(feed));

				VkVertexInputBindingDescription bindingDesc = {};
				bindingDesc.binding = feed->m_inputSlot;

				switch (ResolveConfigurable(feed->m_stepping))
				{
				case InputLayoutVertexInputStepping::Vertex:
					bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
					break;
				case InputLayoutVertexInputStepping::Instance:
					bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
					break;
				default:
					return ResultCode::kInternalError;
				}

				bindingDesc.stride = ResolveConfigurable(feed->m_byteStride);

				RKIT_CHECK(vertexInputBindingDescs.Append(bindingDesc));
			}
		}

		VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
		vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputStateCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindingDescs.Count());
		vertexInputStateCreateInfo.pVertexBindingDescriptions = vertexInputBindingDescs.GetBuffer();
		vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescs.Count());
		vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexAttributeDescs.GetBuffer();

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
		inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

		switch (pipelineDesc->m_primitiveTopology)
		{
		case PrimitiveTopology::PointList:
			inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			break;
		case PrimitiveTopology::LineList:
			inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			break;
		case PrimitiveTopology::LineStrip:
			inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
			break;
		case PrimitiveTopology::TriangleList:
			inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			break;
		case PrimitiveTopology::TriangleStrip:
			inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			break;
		default:
			return ResultCode::kInternalError;
		}

		inputAssemblyStateCreateInfo.primitiveRestartEnable = (pipelineDesc->m_primitiveRestart) ? VK_TRUE : VK_FALSE;

		VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
		viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateCreateInfo.viewportCount = 1;
		viewportStateCreateInfo.scissorCount = 1;

		VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
		rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

		switch (pipelineDesc->m_fillMode)
		{
		case FillMode::Wireframe:
			rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;
			rasterizationStateCreateInfo.lineWidth = 1;
			break;
		case FillMode::Solid:
			rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
			break;
		default:
			return ResultCode::kInternalError;
		}

		switch (pipelineDesc->m_cullMode)
		{
		case CullMode::None:
			rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
			break;
		case CullMode::Back:
			rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
			break;
		case CullMode::Front:
			rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
			break;
		default:
			return ResultCode::kInternalError;
		}

		const render::DepthStencilOperationDesc &depthStencilDesc = *pipelineDesc->m_depthStencil;

		rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;	// ???
		rasterizationStateCreateInfo.depthBiasClamp = ResolveConfigurable(depthStencilDesc.m_depthBiasClamp);
		rasterizationStateCreateInfo.depthBiasConstantFactor = static_cast<float>(ResolveConfigurable(depthStencilDesc.m_depthBias));
		rasterizationStateCreateInfo.depthBiasSlopeFactor = static_cast<float>(ResolveConfigurable(depthStencilDesc.m_depthBiasSlopeScale));

		if (rasterizationStateCreateInfo.depthBiasSlopeFactor != 0 || rasterizationStateCreateInfo.depthBiasConstantFactor != 0 || rasterizationStateCreateInfo.depthBiasClamp != 0)
			rasterizationStateCreateInfo.depthBiasEnable = VK_TRUE;

		VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
		multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleStateCreateInfo.alphaToCoverageEnable = (pipelineDesc->m_alphaToCoverage) ? VK_TRUE : VK_FALSE;

		VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
		depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilCreateInfo.depthTestEnable = ResolveConfigurable(depthStencilDesc.m_depthTest) ? VK_TRUE : VK_FALSE;
		depthStencilCreateInfo.depthWriteEnable = ResolveConfigurable(depthStencilDesc.m_depthWrite) ? VK_TRUE : VK_FALSE;

		if (ResolveConfigurable(depthStencilDesc.m_depthCompareOp) == ComparisonFunction::Disabled)
			depthStencilCreateInfo.depthTestEnable = VK_FALSE;
		else
		{
			RKIT_CHECK(ResolveCompareOp(depthStencilCreateInfo.depthCompareOp, ResolveConfigurable(depthStencilDesc.m_depthCompareOp)));
		}


		if (ResolveConfigurable(depthStencilDesc.m_stencilTest))
			depthStencilCreateInfo.stencilTestEnable = VK_TRUE;

		RKIT_CHECK(ResolveStencilOps(depthStencilCreateInfo.front, depthStencilDesc.m_stencilFrontOps, depthStencilDesc));
		RKIT_CHECK(ResolveStencilOps(depthStencilCreateInfo.back, depthStencilDesc.m_stencilBackOps, depthStencilDesc));

		if (ResolveConfigurable(depthStencilDesc.m_dynamicStencilReference))
		{
			RKIT_CHECK(dynamicStates.Append(VK_DYNAMIC_STATE_STENCIL_REFERENCE));
		}

		if (renderPassDesc->m_renderTargets.Count() != pipelineDesc->m_renderTargets.Count())
		{
			rkit::log::Error("Pipeline render target count didn't match render pass count");
			return ResultCode::kMalformedFile;
		}

		Vector<VkPipelineColorBlendAttachmentState> blendAttachments;
		RKIT_CHECK(blendAttachments.Resize(pipelineDesc->m_renderTargets.Count()));

		for (size_t rti = 0; rti < pipelineDesc->m_renderTargets.Count(); rti++)
		{
			VkPipelineColorBlendAttachmentState &vkba = blendAttachments[rti];
			const RenderOperationDesc &rtDesc = *pipelineDesc->m_renderTargets[rti];

			vkba = {};
			if (rtDesc.m_srcBlend != ColorBlendFactor::One || rtDesc.m_dstBlend != ColorBlendFactor::Zero || rtDesc.m_colorBlendOp != BlendOp::Add
				|| rtDesc.m_srcAlphaBlend != AlphaBlendFactor::One || rtDesc.m_dstAlphaBlend != AlphaBlendFactor::Zero || rtDesc.m_alphaBlendOp != BlendOp::Add
				|| !rtDesc.m_writeAlpha || !rtDesc.m_writeRed || !rtDesc.m_writeGreen || !rtDesc.m_writeBlue)
			{
				vkba.blendEnable = VK_TRUE;

				RKIT_CHECK(ResolveColorBlendFactor(vkba.srcColorBlendFactor, rtDesc.m_srcBlend));
				RKIT_CHECK(ResolveColorBlendFactor(vkba.dstColorBlendFactor, rtDesc.m_dstBlend));
				RKIT_CHECK(ResolveBlendOp(vkba.colorBlendOp, rtDesc.m_colorBlendOp));

				RKIT_CHECK(ResolveAlphaBlendFactor(vkba.srcAlphaBlendFactor, rtDesc.m_srcAlphaBlend));
				RKIT_CHECK(ResolveAlphaBlendFactor(vkba.dstAlphaBlendFactor, rtDesc.m_dstAlphaBlend));
				RKIT_CHECK(ResolveBlendOp(vkba.alphaBlendOp, rtDesc.m_alphaBlendOp));

				if (rtDesc.m_writeAlpha)
					vkba.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
				if (rtDesc.m_writeRed)
					vkba.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
				if (rtDesc.m_writeGreen)
					vkba.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
				if (rtDesc.m_writeBlue)
					vkba.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
			}
		}

		if (pipelineDesc->m_dynamicBlendConstants)
		{
			RKIT_CHECK(dynamicStates.Append(VK_DYNAMIC_STATE_BLEND_CONSTANTS));
		}

		VkPipelineColorBlendStateCreateInfo blendStateCreateInfo = {};
		blendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blendStateCreateInfo.attachmentCount = static_cast<uint32_t>(pipelineDesc->m_renderTargets.Count());
		blendStateCreateInfo.pAttachments = blendAttachments.GetBuffer();
		blendStateCreateInfo.blendConstants[0] = pipelineDesc->m_blendConstantRed;
		blendStateCreateInfo.blendConstants[1] = pipelineDesc->m_blendConstantGreen;
		blendStateCreateInfo.blendConstants[2] = pipelineDesc->m_blendConstantBlue;
		blendStateCreateInfo.blendConstants[3] = pipelineDesc->m_blendConstantAlpha;

		// Create stages and final pipeline

		VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
		dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.Count());
		dynamicStateCreateInfo.pDynamicStates = dynamicStates.GetBuffer();

		VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

		pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
		pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
		pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
		pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
		pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
		pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
		pipelineCreateInfo.pColorBlendState = &blendStateCreateInfo;
		RKIT_CHECK(FindPipelineLayout(pipelineCreateInfo.layout, pipelineDesc->m_pipelineLayout));
		RKIT_CHECK(FindRenderPass(pipelineCreateInfo.renderPass, renderPassDesc));
		pipelineCreateInfo.subpass = 0;

		StaticArray<VkPipelineShaderStageCreateInfo, kMaxGraphicsStages> stageCreateInfos;

		const size_t basePermutation = permutationIndex * kMaxGraphicsStages;

		for (size_t stage = 0; stage < kMaxGraphicsStages; stage++)
		{
			const rkit::render::ContentKey *contentKey = contentKeys[basePermutation + stage];

			if (contentKey)
			{
				VkPipelineShaderStageCreateInfo &stageCreateInfo = stageCreateInfos[pipelineCreateInfo.stageCount];
				stageCreateInfo = {};
				stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

				switch (static_cast<GraphicPipelineStage>(stage))
				{
				case GraphicPipelineStage::Vertex:
					stageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
					break;
				case GraphicPipelineStage::Pixel:
					stageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
					break;

				default:
					return ResultCode::kInternalError;
				}

				const size_t bcIndex = contentKey->m_content.m_contentIndex;
				RKIT_CHECK(LoadShaderModule(contentKey->m_content.m_contentIndex));

				stageCreateInfo.module = m_binaryContents[bcIndex].m_shaderModule;
				stageCreateInfo.pName = "main";

				pipelineCreateInfo.stageCount++;
			}
		}

		if (pipelineCreateInfo.stageCount > 0)
			pipelineCreateInfo.pStages = &stageCreateInfos[0];

		RKIT_VK_CHECK(vkd.vkCreateGraphicsPipelines(m_device.GetDevice(), pipelineCache, 1, &pipelineCreateInfo, m_device.GetAllocCallbacks(), &pipeline.m_pipeline));

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::CheckedCompileUnmergedGraphicsPipeline(VkPipelineCache &pipelineCache, CompiledPipeline &pipeline, size_t pipelineIndex, size_t permutationIndex)
	{
		const VulkanDeviceAPI &vkd = m_device.GetDeviceAPI();

		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

		RKIT_VK_CHECK(vkd.vkCreatePipelineCache(m_device.GetDevice(), &pipelineCacheCreateInfo, m_device.GetAllocCallbacks(), &pipelineCache));

		RKIT_CHECK(CheckedCreateGraphicsPipeline(pipelineCache, pipeline, pipelineIndex, permutationIndex));

		if (pipeline.m_pipeline != VK_NULL_HANDLE)
		{
			vkd.vkDestroyPipeline(m_device.GetDevice(), pipeline.m_pipeline, m_device.GetAllocCallbacks());
			pipeline.m_pipeline = VK_NULL_HANDLE;
		}

		size_t dataSize = 0;
		RKIT_VK_CHECK(vkd.vkGetPipelineCacheData(m_device.GetDevice(), pipelineCache, &dataSize, nullptr));

		Vector<uint8_t> pipelineCacheDataBuffer;
		RKIT_CHECK(pipelineCacheDataBuffer.Resize(dataSize));
		RKIT_VK_CHECK(vkd.vkGetPipelineCacheData(m_device.GetDevice(), pipelineCache, &dataSize, pipelineCacheDataBuffer.GetBuffer()));

		// CAUTION: dataSize is overwritten at this point!

		if (pipelineCache != VK_NULL_HANDLE)
		{
			vkd.vkDestroyPipelineCache(m_device.GetDevice(), pipelineCache, m_device.GetAllocCallbacks());
			pipelineCache = VK_NULL_HANDLE;
		}

		// Write out to shadowfile
		String path;
		RKIT_CHECK(GetUnmergedCachePath(path, m_configHashBytes, pipelineIndex, permutationIndex));

		{
			MutexLock lock(*m_cacheStreamMutex);

			if (m_individualPipelineLocators.Count() == 0)
			{
				RKIT_CHECK(m_cacheWriteStream->SeekStart(0));

				PipelineCacheHeader header = {};
				header.m_identifier = PipelineCacheHeader::kExpectedIdentifier;
				header.m_version = PipelineCacheHeader::kExpectedVersion;
				header.m_phase = PipelineCacheHeader::Phase::kIndividual;
				header.m_packageUUID = m_package->GetPackageUUID();

				RKIT_CHECK(m_cacheWriteStream->WriteAll(&header, sizeof(header)));
				RKIT_CHECK(m_cacheWriteStream->Flush());
			}

			CompiledPipelineKey plKey;
			plKey.m_pipelineIndex = pipelineIndex;
			plKey.m_permutationIndex = permutationIndex;

			CompiledPipelineFileLocator locator;
			locator.m_filePos = m_cacheWriteStream->Tell();
			locator.m_size = dataSize;

			RKIT_CHECK((m_individualPipelineLocators.Set<CompiledPipelineKey, CompiledPipelineFileLocator, CompiledPipelineKeyHasher>(std::move(plKey), std::move(locator))));

			RKIT_CHECK(m_cacheWriteStream->WriteAll(pipelineCacheDataBuffer.GetBuffer(), dataSize));
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::LoadShaderModule(size_t binaryContentIndex)
	{
		BinaryContentData &bcd = m_binaryContents[binaryContentIndex];

		{
			MutexLock lock(*m_shaderModuleMutex);
			if (bcd.m_shaderModule != VK_NULL_HANDLE)
				return ResultCode::kOK;
		}

		const size_t binaryContentSize = m_package->GetBinaryContentSize(binaryContentIndex);
		const size_t sizeDWords = binaryContentSize / 4;

		if (binaryContentSize % 4 != 0)
			return ResultCode::kMalformedFile;

		Vector<uint8_t> content;
		RKIT_CHECK(content.Resize(binaryContentSize));

		{
			MutexLock lock(*m_packageStreamMutex);

			RKIT_CHECK(m_packageStream->SeekStart(bcd.m_filePos));
			RKIT_CHECK(m_packageStream->ReadAll(content.GetBuffer(), binaryContentSize));
		}

		Vector<uint32_t> decoded;
		RKIT_CHECK(decoded.Resize(binaryContentSize / 4));

		uint32_t *outDWords = decoded.GetBuffer();
		const uint8_t *inBytes = content.GetBuffer();

		for (size_t i = 0; i < sizeDWords; i++)
		{
			const uint8_t *dwordStart = inBytes + i * 4;
			uint32_t dword = static_cast<uint32_t>(static_cast<uint32_t>(dwordStart[0]) << 0)
				| static_cast<uint32_t>(static_cast<uint32_t>(dwordStart[1]) << 8)
				| static_cast<uint32_t>(static_cast<uint32_t>(dwordStart[2]) << 16)
				| static_cast<uint32_t>(static_cast<uint32_t>(dwordStart[3]) << 24);

			outDWords[i] = dword;
		}

		content.Reset();

		const VulkanDeviceAPI &vkd = m_device.GetDeviceAPI();

		{
			MutexLock lock(*m_shaderModuleMutex);
			if (bcd.m_shaderModule != VK_NULL_HANDLE)
				return ResultCode::kOK;

			VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
			shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			shaderModuleCreateInfo.codeSize = binaryContentSize;
			shaderModuleCreateInfo.pCode = decoded.GetBuffer();

			RKIT_VK_CHECK(vkd.vkCreateShaderModule(m_device.GetDevice(), &shaderModuleCreateInfo, m_device.GetAllocCallbacks(), &bcd.m_shaderModule));
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::GetUnmergedCachePath(String &path, const utils::Sha256DigestBytes &configHash, size_t pipelineIndex, size_t permutationIndex)
	{
		const size_t kNumTriplets = (utils::Sha256DigestBytes::kSize + 2) / 3;
		const size_t kNumChars = kNumTriplets * 4;
		StaticArray<char, kNumChars + 1> chars;

		for (size_t i = 0; i < kNumTriplets; i++)
		{
			uint8_t bytes[3];

			for (size_t bi = 0; bi < 3; bi++)
			{
				const size_t inputByteIndex = i * 3 + bi;
				if (inputByteIndex < utils::Sha256DigestBytes::kSize)
					bytes[bi] = configHash.m_data[inputByteIndex];
				else
					bytes[bi] = 0;
			}

			const size_t firstCharIndex = i / 4;

			size_t charIndexes[4];
			charIndexes[0] = (bytes[0] & 0x3f);
			charIndexes[1] = ((bytes[0] >> 6) | (bytes[1] & 0xf) << 2);
			charIndexes[2] = (((bytes[1] & 0xf0) >> 4) | (bytes[2] & 0x3) << 4);
			charIndexes[3] = ((bytes[2] & 0xfc) >> 2);

			for (size_t ci = 0; ci < 4; ci++)
			{
				size_t charIndex = charIndexes[ci];
				chars[i * 4 + ci] = ("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-")[charIndex];
			}
		}

		chars[kNumChars] = '\0';

		return path.Format("%s/pl_%zu/p_%zu.bin", chars.GetBuffer(), pipelineIndex, permutationIndex);
	}

	template<class T, class TDefaultResolver>
	T PipelineLibraryLoader::ResolveConfigurable(const ConfigurableValueWithDefault<T, TDefaultResolver> &configurable)
	{
		switch (configurable.m_state)
		{
		case ConfigurableValueState::Default:
			RKIT_ASSERT((ConfigurableValueWithDefault<T, TDefaultResolver>::HasDefault()));
			return ConfigurableValueWithDefault<T, TDefaultResolver>::GetDefault();
		case ConfigurableValueState::Explicit:
			return configurable.m_u.m_value;
		default:
			RKIT_ASSERT(false);
			return T();
		};
	}

	Result PipelineLibraryLoader::AddMergedPipeline(size_t pipelineIndex, size_t permutationIndex)
	{
		CompiledPipelineKey key = {};
		key.m_pipelineIndex = pipelineIndex;
		key.m_permutationIndex = permutationIndex;

		HashMap<CompiledPipelineKey, CompiledPipelineFileLocator>::ConstIterator_t it = m_individualPipelineLocators.Find<CompiledPipelineKey, CompiledPipelineKeyHasher>(key);

		if (it == m_individualPipelineLocators.end())
			return ResultCode::kInternalError;

		Vector<uint8_t> pipelineCacheData;
		RKIT_CHECK(pipelineCacheData.Resize(it.Value().m_size));

		{
			MutexLock lock(*m_cacheStreamMutex);

			RKIT_CHECK(m_cacheWriteStream->SeekStart(it.Value().m_filePos));
			RKIT_CHECK(m_cacheWriteStream->ReadAll(pipelineCacheData.GetBuffer(), pipelineCacheData.Count()));
		}

		const VulkanDeviceAPI &vkd = m_device.GetDeviceAPI();

		RKIT_CHECK(m_individualCaches.Append(VK_NULL_HANDLE));

		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		pipelineCacheCreateInfo.pInitialData = pipelineCacheData.GetBuffer();
		pipelineCacheCreateInfo.initialDataSize = pipelineCacheData.Count();

		RKIT_VK_CHECK(vkd.vkCreatePipelineCache(m_device.GetDevice(), &pipelineCacheCreateInfo, m_device.GetAllocCallbacks(), &m_individualCaches[m_individualCaches.Count() - 1]));

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::SaveMergedPipeline()
	{
		const VulkanDeviceAPI &vkd = m_device.GetDeviceAPI();

		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

		RKIT_VK_CHECK(vkd.vkCreatePipelineCache(m_device.GetDevice(), &pipelineCacheCreateInfo, m_device.GetAllocCallbacks(), &m_mergedCache));

		if (m_individualCaches.Count() > 0)
		{
			RKIT_VK_CHECK(vkd.vkMergePipelineCaches(m_device.GetDevice(), m_mergedCache, static_cast<uint32_t>(m_individualCaches.Count()), m_individualCaches.GetBuffer()));
		}

		for (VkPipelineCache &plc : m_individualCaches)
		{
			vkd.vkDestroyPipelineCache(m_device.GetDevice(), plc, m_device.GetAllocCallbacks());
			plc = VK_NULL_HANDLE;
		}

		size_t dataSize = 0;
		RKIT_VK_CHECK(vkd.vkGetPipelineCacheData(m_device.GetDevice(), m_mergedCache, &dataSize, nullptr));

		Vector<uint8_t> pipelineCacheDataBuffer;
		RKIT_CHECK(pipelineCacheDataBuffer.Resize(dataSize));
		RKIT_VK_CHECK(vkd.vkGetPipelineCacheData(m_device.GetDevice(), m_mergedCache, &dataSize, pipelineCacheDataBuffer.GetBuffer()));

		PipelineCacheHeader header = {};
		header.m_identifier = PipelineCacheHeader::kExpectedIdentifier;
		header.m_version = PipelineCacheHeader::kExpectedVersion;
		header.m_phase = PipelineCacheHeader::Phase::kEmpty;
		header.m_packageUUID = m_package->GetPackageUUID();

		{
			MutexLock lock(*m_cacheStreamMutex);
			RKIT_CHECK(m_cacheWriteStream->SeekStart(0));
			RKIT_CHECK(m_cacheWriteStream->WriteAll(&header, sizeof(header)));
			RKIT_CHECK(m_cacheWriteStream->Flush());

			RKIT_CHECK(m_cacheWriteStream->WriteAll(pipelineCacheDataBuffer.GetBuffer(), pipelineCacheDataBuffer.Count()));
			RKIT_CHECK(m_cacheWriteStream->Truncate(m_cacheWriteStream->Tell()));

			header.m_phase = PipelineCacheHeader::Phase::kMerged;

			RKIT_CHECK(m_cacheWriteStream->SeekStart(0));
			RKIT_CHECK(m_cacheWriteStream->WriteAll(&header, sizeof(header)));
			RKIT_CHECK(m_cacheWriteStream->Flush());
		}

		return ResultCode::kOK;
	}

	Result PipelineLibraryLoaderBase::Create(VulkanDeviceBase &device, UniquePtr<PipelineLibraryLoaderBase> &outLoader, UniquePtr<IPipelineLibraryConfigValidator> &&validator,
		UniquePtr<data::IRenderDataPackage> &&package, UniquePtr<ISeekableReadStream> &&packageStream, FilePos_t packageBinaryContentStart)
	{
		UniquePtr<PipelineLibraryLoader> loader;

		RKIT_CHECK(New<PipelineLibraryLoader>(loader, device, std::move(validator), std::move(package), std::move(packageStream), packageBinaryContentStart));

		RKIT_CHECK(loader->Initialize());

		outLoader = std::move(loader);

		return ResultCode::kOK;
	}
}
