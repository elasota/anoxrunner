#pragma once

#include <cstddef>

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/StringProto.h"

namespace rkit
{
	struct IReadStream;
	struct IWriteStream;
	struct ISeekableReadStream;
	struct ISeekableReadWriteStream;

	template<class T>
	class UniquePtr;
}

namespace rkit { namespace render
{
	struct IPipelineLibrary;

	struct IPipelineLibraryConfigValidator
	{
		virtual ~IPipelineLibraryConfigValidator() {}

		virtual Result CheckConfig(IReadStream &stream, bool &isConfigMatched) = 0;
		virtual Result WriteConfig(IWriteStream &stream) const = 0;
	};

	struct IPipelineLibraryLoader
	{
		virtual ~IPipelineLibraryLoader() {}

		virtual Result LoadObjectsFromPackage() = 0;

		virtual void SetMergedLibraryStream(UniquePtr<ISeekableReadStream> &&cacheReadStream, ISeekableReadWriteStream *cacheWriteStream) = 0;
		virtual Result TryOpenMergedLibrary(bool &outSucceeded) = 0;
		virtual Result LoadGraphicsPipelineFromMergedLibrary(size_t pipelineIndex, size_t permutationIndex) = 0;
		virtual void CloseMergedLibrary(bool unloadPipelines, bool unloadMergedCache) = 0;

		virtual Result CompileUnmergedGraphicsPipeline(size_t pipelineIndex, size_t permutationIndex) = 0;
		virtual Result AddMergedPipeline(size_t pipelineIndex, size_t permutationIndex) = 0;

		virtual Result SaveMergedPipeline() = 0;

		virtual Result GetFinishedPipeline(UniquePtr<IPipelineLibrary> &outPipelineLibrary) = 0;
	};
} } // rkit::render
