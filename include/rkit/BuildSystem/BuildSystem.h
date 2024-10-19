#pragma once

#include "BuildFileLocation.h"

#include "rkit/Core/Drivers.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct IReadStream;

	namespace buildsystem
	{
		static const uint32_t kDefaultNamespace = 0x74694b52;
		static const uint32_t kDepsNodeID = 0x53504544;

		struct IDependencyNode;
		struct IDependencyGraphFactory;
		struct FileStatusView;

		struct IBuildFileSystem
		{
			typedef Result (*ApplyFileStatusCallback_t)(void *userdata, const FileStatusView &fileStatus);

			virtual ~IBuildFileSystem() {}

			virtual Result ResolveFileStatusIfExists(BuildFileLocation inputFileLocation, const StringView &identifier, void *userdata, ApplyFileStatusCallback_t applyStatus) = 0;
		};

		struct IBuildSystemInstance
		{
			virtual ~IBuildSystemInstance() {}

			virtual Result Initialize(const rkit::StringView &targetName, const StringView &srcDir, const StringView &intermediateDir, const StringView &dataDir) = 0;

			virtual IDependencyNode *FindNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier) const = 0;
			virtual Result FindOrCreateNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier, IDependencyNode *&outNode) = 0;

			virtual Result AddRootNode(IDependencyNode *node) = 0;

			virtual Result Build(IBuildFileSystem *fs) = 0;

			virtual IDependencyGraphFactory *GetDependencyGraphFactory() const = 0;
		};

		struct IBuildSystemDriver : public ICustomDriver
		{
			virtual Result CreateBuildSystemInstance(UniquePtr<IBuildSystemInstance> &outInstance) const = 0;
		};
	}
}
