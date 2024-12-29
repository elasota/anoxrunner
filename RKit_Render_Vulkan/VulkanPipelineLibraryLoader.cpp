#include "rkit/Utilities/ShadowFile.h"

#include "rkit/Data/RenderDataHandler.h"

#include "rkit/Core/Stream.h"
#include "rkit/Core/UniquePtr.h"

#include "VulkanDevice.h"
#include "VulkanPipelineLibraryLoader.h"

namespace rkit::render::vulkan
{
	class PipelineLibraryLoader final : public PipelineLibraryLoaderBase
	{
	public:
		PipelineLibraryLoader(VulkanDeviceBase &device, UniquePtr<IPipelineLibraryConfigValidator> &&validator,
			UniquePtr<data::IRenderDataPackage> &&package, UniquePtr<ISeekableReadStream> &&packageStream, FilePos_t packageBinaryContentStart,
			UniquePtr<utils::IShadowFile> &&cacheShadowFile, UniquePtr<ISeekableReadWriteStream> &&cacheStream);
		~PipelineLibraryLoader();

		Result LoadObjectsFromPackage() override;

		Result OpenMergedLibrary() override;
		Result LoadGraphicsPipelineFromMergedLibrary(size_t pipelineIndex, size_t permutationIndex) override;
		void CloseMergedLibrary() override;

		Result CompileUnmergedGraphicsPipeline(size_t pipelineIndex, size_t permutationIndex) override;
		Result AddMergedPipeline(size_t pipelineIndex, size_t permutationIndex) override;

		Result SaveMergedPipeline() override;

	private:
		VulkanDeviceBase &m_device;

		UniquePtr<data::IRenderDataPackage> m_package;
		UniquePtr<ISeekableReadStream> m_packageStream;

		FilePos_t m_packageBinaryContentStart;

		UniquePtr<ISeekableReadWriteStream> m_cacheStream;
		UniquePtr<utils::IShadowFile> m_cacheShadowFile;

		UniquePtr<IPipelineLibraryConfigValidator> m_validator;
	};


	PipelineLibraryLoader::PipelineLibraryLoader(VulkanDeviceBase &device, UniquePtr<IPipelineLibraryConfigValidator> &&validator,
		UniquePtr<data::IRenderDataPackage> &&package, UniquePtr<ISeekableReadStream> &&packageStream, FilePos_t packageBinaryContentStart,
		UniquePtr<utils::IShadowFile> &&cacheShadowFile, UniquePtr<ISeekableReadWriteStream> &&cacheStream)
		: m_device(device)
		, m_validator(std::move(validator))
		, m_package(std::move(package))
		, m_packageStream(std::move(packageStream))
		, m_packageBinaryContentStart(packageBinaryContentStart)
		, m_cacheStream(std::move(cacheStream))
		, m_cacheShadowFile(std::move(cacheShadowFile))
	{
	}

	PipelineLibraryLoader::~PipelineLibraryLoader()
	{
		m_cacheShadowFile.Reset();
		m_cacheStream.Reset();

		m_package.Reset();
		m_packageStream.Reset();

		m_validator.Reset();
	}

	Result PipelineLibraryLoader::LoadObjectsFromPackage()
	{
		return ResultCode::kOK;
	}

	Result PipelineLibraryLoader::OpenMergedLibrary()
	{
		return ResultCode::kNotYetImplemented;
	}

	Result PipelineLibraryLoader::LoadGraphicsPipelineFromMergedLibrary(size_t pipelineIndex, size_t permutationIndex)
	{
		return ResultCode::kNotYetImplemented;
	}

	void PipelineLibraryLoader::CloseMergedLibrary()
	{
	}

	Result PipelineLibraryLoader::CompileUnmergedGraphicsPipeline(size_t pipelineIndex, size_t permutationIndex)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result PipelineLibraryLoader::AddMergedPipeline(size_t pipelineIndex, size_t permutationIndex)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result PipelineLibraryLoader::SaveMergedPipeline()
	{
		return ResultCode::kNotYetImplemented;
	}

	Result PipelineLibraryLoaderBase::Create(VulkanDeviceBase &device, UniquePtr<PipelineLibraryLoaderBase> &outLoader, UniquePtr<IPipelineLibraryConfigValidator> &&validator,
		UniquePtr<data::IRenderDataPackage> &&package, UniquePtr<ISeekableReadStream> &&packageStream, FilePos_t packageBinaryContentStart,
		UniquePtr<utils::IShadowFile> &&cacheShadowFile, UniquePtr<ISeekableReadWriteStream> &&cacheStream)
	{
		UniquePtr<PipelineLibraryLoader> loader;

		RKIT_CHECK(New<PipelineLibraryLoader>(loader, device, std::move(validator), std::move(package), std::move(packageStream),
			packageBinaryContentStart, std::move(cacheShadowFile), std::move(cacheStream)));

		outLoader = std::move(loader);

		return ResultCode::kOK;
	}
}
