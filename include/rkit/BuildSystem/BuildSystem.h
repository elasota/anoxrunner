#pragma once

#include "BuildFileLocation.h"

#include "rkit/Core/Drivers.h"
#include "rkit/Core/FourCC.h"
#include "rkit/Core/StringProto.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct IReadStream;

	namespace data
	{
		struct IRenderDataHandler;
	}

	namespace buildsystem
	{
		static const uint32_t kDefaultNamespace = RKIT_FOURCC('R', 'K', 'i', 't');
		static const uint32_t kDepsNodeID = RKIT_FOURCC('D', 'E', 'P', 'S');
		static const uint32_t kRenderGraphicsPipelineNodeID = RKIT_FOURCC('R', 'G', 'P', 'L');
		static const uint32_t kRenderComputePipelineNodeID = RKIT_FOURCC('R', 'C', 'P', 'L');
		static const uint32_t kRenderPipelineLibraryNodeID = RKIT_FOURCC('R', 'P', 'L', 'L');

		struct IDependencyNode;
		struct IDependencyGraphFactory;
		struct IPackageBuilder;
		struct IPackageObjectWriter;
		struct FileStatusView;

		StringView GetShaderSourceBasePath();

		struct IBuildFileSystem
		{
			typedef Result (*ApplyFileStatusCallback_t)(void *userdata, const FileStatusView &fileStatus);

			virtual ~IBuildFileSystem() {}

			virtual Result ResolveFileStatusIfExists(BuildFileLocation inputFileLocation, const StringView &identifier, void *userdata, ApplyFileStatusCallback_t applyStatus) = 0;
		};

		struct IPipelineLibraryCombiner
		{
			virtual ~IPipelineLibraryCombiner() {}

			virtual Result AddInput(IReadStream &stream) = 0;
			virtual Result WritePackage(ISeekableWriteStream &stream) = 0;
		};

		struct IBuildSystemInstance
		{
			virtual ~IBuildSystemInstance() {}

			virtual Result Initialize(const rkit::StringView &targetName, const StringView &srcDir, const StringView &intermediateDir, const StringView &dataDir) = 0;

			virtual IDependencyNode *FindNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier) const = 0;
			virtual Result FindOrCreateNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier, IDependencyNode *&outNode) = 0;
			virtual Result RegisterNodeTypeByExtension(const StringView &ext, uint32_t nodeNamespace, uint32_t nodeType) = 0;

			virtual Result AddRootNode(IDependencyNode *node) = 0;

			virtual Result Build(IBuildFileSystem *fs) = 0;

			virtual IDependencyGraphFactory *GetDependencyGraphFactory() const = 0;

			virtual CallbackSpan<IDependencyNode *, const IBuildSystemInstance *> GetBuildRelevantNodes() const = 0;
		};

		struct IBuildSystemDriver : public ICustomDriver
		{
			virtual Result CreateBuildSystemInstance(UniquePtr<IBuildSystemInstance> &outInstance) const = 0;
			virtual Result CreatePackageObjectWriter(UniquePtr<IPackageObjectWriter> &outWriter) const = 0;
			virtual Result CreatePackageBuilder(data::IRenderDataHandler *dataHandler, IPackageObjectWriter *objWriter, bool allowTempStrings, UniquePtr<IPackageBuilder> &outBuilder) const = 0;
			virtual Result CreatePipelineLibraryCombiner(UniquePtr<IPipelineLibraryCombiner> &outCombiner) const = 0;
		};

		struct IBuildSystemAddOnDriver : public ICustomDriver
		{
			virtual Result RegisterBuildSystemAddOn(IBuildSystemInstance *instance) = 0;
		};
	}
}

#include "rkit/Core/StringView.h"

namespace rkit::buildsystem
{
	inline StringView GetShaderSourceBasePath()
	{
		return "rkit/render/src/";
	}

	inline StringView GetCompiledPipelineIntermediateBasePath()
	{
		return "rpll_c/";
	}
}
