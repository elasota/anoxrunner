#pragma once

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

		enum class BuildFileLocation
		{
			kSourceDir,
			kIntermediateDir,
		};

		struct IBuildSystemInstance
		{
			virtual ~IBuildSystemInstance() {}

			virtual Result Initialize(const rkit::StringView &targetName, const StringView &srcDir, const StringView &intermediateDir, const StringView &dataDir) = 0;

			virtual IDependencyNode *FindNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation buildFileLocation, const StringView &identifier) const = 0;
			virtual Result AddRootNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, UniquePtr<IDependencyNode> &&node) const = 0;

			virtual Result Build() = 0;

			virtual IDependencyGraphFactory *GetDependencyGraphFactory() const = 0;
		};

		struct IBuildSystemDriver : public ICustomDriver
		{
			virtual Result CreateBuildSystemInstance(UniquePtr<IBuildSystemInstance> &outInstance) const = 0;
		};
	}
}
