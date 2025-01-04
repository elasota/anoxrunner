#pragma once

namespace rkit
{
	struct Result;
	struct IReadStream;
	struct IWriteStream;
	struct ISeekableReadStream;
	struct ISeekableReadWriteStream;

	template<class T>
	class UniquePtr;
}

namespace rkit::render
{
	struct IPipelineLibraryConfigValidator
	{
		virtual ~IPipelineLibraryConfigValidator() {}

		virtual Result CheckConfig(IReadStream &stream, bool &isConfigMatched) = 0;
		virtual Result WriteConfig(IWriteStream &stream) const = 0;
	};

	struct IPipelineLibraryLoosePipeline
	{
		virtual ~IPipelineLibraryLoosePipeline() {}
	};

	struct IPipelineLibrary
	{
		virtual ~IPipelineLibrary() {}
	};

	struct IPipelineLibraryLoader
	{
		virtual ~IPipelineLibraryLoader() {}

		virtual Result LoadObjectsFromPackage() = 0;

		virtual void SetMergedLibraryStream(UniquePtr<ISeekableReadStream> &&cacheReadStream, ISeekableReadWriteStream *cacheWriteStream) = 0;
		virtual Result OpenMergedLibrary() = 0;
		virtual Result LoadGraphicsPipelineFromMergedLibrary(size_t pipelineIndex, size_t permutationIndex) = 0;
		virtual void CloseMergedLibrary(bool unloadPipelines, bool unloadMergedCache) = 0;

		virtual Result CompileUnmergedGraphicsPipeline(size_t pipelineIndex, size_t permutationIndex) = 0;
		virtual Result AddMergedPipeline(size_t pipelineIndex, size_t permutationIndex) = 0;

		virtual Result SaveMergedPipeline() = 0;

		virtual UniquePtr<IPipelineLibrary> GetFinishedPipeline() = 0;
	};
}
