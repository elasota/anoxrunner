#pragma once

#include "BuildFileLocation.h"

#include "rkit/Core/Drivers.h"
#include "rkit/Core/FourCC.h"
#include "rkit/Core/PathProto.h"
#include "rkit/Core/StringProto.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct IReadStream;

	namespace data
	{
		struct IRenderDataHandler;
		struct ContentID;
	}

	namespace buildsystem
	{
		static const uint32_t kDefaultNamespace = RKIT_FOURCC('R', 'K', 'i', 't');
		static const uint32_t kDepsNodeID = RKIT_FOURCC('D', 'E', 'P', 'S');
		static const uint32_t kRenderGraphicsPipelineNodeID = RKIT_FOURCC('R', 'G', 'P', 'L');
		static const uint32_t kRenderComputePipelineNodeID = RKIT_FOURCC('R', 'C', 'P', 'L');
		static const uint32_t kRenderPipelineLibraryNodeID = RKIT_FOURCC('R', 'P', 'L', 'L');
		static const uint32_t kCopyFileNodeID = RKIT_FOURCC('C', 'O', 'P', 'Y');

		struct IDependencyNode;
		struct IDependencyGraphFactory;
		struct IPackageBuilder;
		struct IPackageObjectWriter;
		struct FileStatusView;
		struct IDependencyNodeCompiler;

		CIPathView GetShaderSourceBasePath();

		struct IBuildFileSystem
		{
			typedef Result (*ApplyFileStatusCallback_t)(void *userdata, const FileStatusView &fileStatus);

			virtual ~IBuildFileSystem() {}

			virtual Result ResolveFileStatusIfExists(BuildFileLocation inputFileLocation, const CIPathView &path, bool allowDirectories, void *userdata, ApplyFileStatusCallback_t applyStatus) = 0;
			virtual Result TryOpenFileRead(BuildFileLocation inputFileLocation, const CIPathView &path, UniquePtr<ISeekableReadStream> &outStream) = 0;
			virtual Result EnumerateDirectory(BuildFileLocation inputFileLocation, const CIPathView &path, bool listFiles, bool listDirectories, void *userdata, ApplyFileStatusCallback_t callback) = 0;
		};

		struct IPipelineLibraryCombiner
		{
			virtual ~IPipelineLibraryCombiner() {}

			virtual Result AddInput(IReadStream &stream) = 0;
			virtual Result WritePackage(ISeekableWriteStream &stream) = 0;
		};

		struct IBuildSystemAction
		{
			virtual Result Run() = 0;
		};

		struct IBuildSystemInstance
		{
			typedef Result (*EnumerateFilesResultCallback_t)(void *userdata, const CIPathView &path);

			virtual ~IBuildSystemInstance() {}

			virtual Result Initialize(const StringView &targetName, const OSAbsPathView &srcDir, const OSAbsPathView &intermediateDir, const OSAbsPathView &dataFilesDir, const OSAbsPathView &dataContentDir) = 0;
			virtual Result LoadCache() = 0;

			virtual IDependencyNode *FindNamedNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier) const = 0;
			virtual IDependencyNode *FindContentNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const Span<const uint8_t> &content) const = 0;
			virtual Result FindOrCreateNamedNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier, IDependencyNode *&outNode) = 0;
			virtual Result FindOrCreateContentNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, Vector<uint8_t> &&content, IDependencyNode *&outNode) = 0;
			virtual Result RegisterNodeTypeByExtension(const StringSliceView &ext, uint32_t nodeNamespace, uint32_t nodeType) = 0;

			virtual Result RegisterCASSource(const data::ContentID &contentID, BuildFileLocation inputFileLocation, const CIPathView &path) = 0;

			virtual Result AddRootNode(IDependencyNode *node) = 0;
			virtual Result AddPostBuildAction(IBuildSystemAction *action) = 0;

			virtual Result Build(IBuildFileSystem *fs) = 0;

			virtual IDependencyGraphFactory *GetDependencyGraphFactory() const = 0;

			virtual CallbackSpan<IDependencyNode *, const IBuildSystemInstance *> GetBuildRelevantNodes() const = 0;

			virtual Result TryOpenFileRead(BuildFileLocation location, const CIPathView &path, UniquePtr<ISeekableReadStream> &outFile) = 0;
			virtual Result OpenFileWrite(BuildFileLocation location, const CIPathView &path, UniquePtr<ISeekableReadWriteStream> &outFile) = 0;

			virtual Result EnumerateDirectories(BuildFileLocation location, const CIPathView &path, void *userdata, EnumerateFilesResultCallback_t resultCallback) = 0;
			virtual Result EnumerateFiles(BuildFileLocation location, const CIPathView &path, void *userdata, EnumerateFilesResultCallback_t resultCallback) = 0;

			virtual Result ConstructIntermediatePath(OSAbsPath &outStr, const CIPathView &path) const = 0;
			virtual Result ConstructOutputFilePath(OSAbsPath &outStr, const CIPathView &path) const = 0;
			virtual Result ConstructOutputContentPath(OSAbsPath &outStr, const CIPathView &path) const = 0;
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

#include "rkit/Core/Path.h"

namespace rkit
{
	namespace buildsystem
	{
		inline CIPathView GetShaderSourceBasePath()
		{
			return "rkit/render/src";
		}

		inline CIPathView GetCompiledPipelineIntermediateBasePath()
		{
			return "rpll_c/pipe";
		}

		inline CIPathView GetCompiledGlobalsIntermediateBasePath()
		{
			return "rpll_c/glob";
		}
	}
}
