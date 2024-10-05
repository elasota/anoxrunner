#pragma once

#include "rkit/Core/Drivers.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct IReadStream;

	namespace buildsystem
	{
		struct IDependencyNode;
		struct IDependencyGraphFactory;

		struct IBuildSystemInstance
		{
			virtual ~IBuildSystemInstance() {}

			virtual Result Initialize(const StringView &srcDir, const StringView &intermediateDir, const StringView &dataDir) = 0;

			virtual IDependencyNode *FindNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, const StringView &identifier) const = 0;
			virtual Result AddRootNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, const StringView &identifier, UniquePtr<IDependencyNode> &&node) const = 0;

			virtual Result Build() = 0;

			virtual IDependencyGraphFactory *GetDependencyGraphFactory() const = 0;
		};

		struct IBuildSystemDriver : public ICustomDriver
		{
			virtual Result CreateBuildSystemInstance(UniquePtr<IBuildSystemInstance> &outInstance) const = 0;
		};
	}
}
