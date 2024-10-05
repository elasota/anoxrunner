#include "BuildSystemInstance.h"

#include "rkit/BuildSystem/DependencyGraph.h"

#include "rkit/Core/HashTable.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Vector.h"

namespace rkit::buildsystem
{
	class NodeTypeKey
	{
	public:
		NodeTypeKey(uint32_t typeNamespace, uint32_t typeID);

		bool operator==(const NodeTypeKey &other) const;
		bool operator!=(const NodeTypeKey &other) const;

	private:
		NodeTypeKey() = delete;

		uint32_t m_typeNamespace;
		uint32_t m_typeID;
	};
}

RKIT_DECLARE_BINARY_HASHER(rkit::buildsystem::NodeTypeKey);

bool rkit::buildsystem::NodeTypeKey::operator==(const NodeTypeKey &other) const
{
	return m_typeNamespace == other.m_typeNamespace && m_typeID == other.m_typeID;
}

bool rkit::buildsystem::NodeTypeKey::operator!=(const NodeTypeKey &other) const
{
	return !((*this) == other);
}


namespace rkit::buildsystem
{
	class BuildSystemInstance final : public IBaseBuildSystemInstance, public IDependencyGraphFactory
	{
	public:
		BuildSystemInstance();

		Result Initialize(const StringView &srcDir, const StringView &intermediateDir, const StringView &dataDir) override;
		IDependencyNode *FindNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, const StringView &identifier) const override;
		Result AddRootNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, const StringView &identifier, UniquePtr<IDependencyNode> &&node) const override;

		Result Build() override;

		IDependencyGraphFactory *GetDependencyGraphFactory() const override;

		Result CreateNode(uint32_t nodeNamespace, uint32_t nodeType, const StringView &identifier, UniquePtr<IDependencyNode> &outNode) const override;
		Result RegisterNodeFactory(uint32_t nodeNamespace, uint32_t nodeType, INodeFactory *factory) override;

	private:
		Result AppendPathSeparator(String &str);

		String m_srcDir;
		String m_intermedDir;
		String m_dataDir;
		char m_pathSeparator;

		HashMap<NodeTypeKey, INodeFactory *> m_nodeFactories;
		Vector<UniquePtr<IDependencyNode>> m_nodes;
	};


	NodeTypeKey::NodeTypeKey(uint32_t typeNamespace, uint32_t typeID)
		: m_typeNamespace(typeNamespace)
		, m_typeID(typeID)
	{
	}

	BuildSystemInstance::BuildSystemInstance()
		: m_pathSeparator(0)
	{
	}

	Result IBaseBuildSystemInstance::Create(UniquePtr<IBuildSystemInstance> &outInstance)
	{
		return rkit::New<BuildSystemInstance>(outInstance);
	}

	Result BuildSystemInstance::Initialize(const StringView &srcDir, const StringView &intermediateDir, const StringView &dataDir)
	{
		m_pathSeparator = GetDrivers().m_systemDriver->GetPathSeparator();

		RKIT_CHECK(m_srcDir.Set(srcDir));
		RKIT_CHECK(m_intermedDir.Set(intermediateDir));
		RKIT_CHECK(m_dataDir.Set(dataDir));

		RKIT_CHECK(AppendPathSeparator(m_srcDir));
		RKIT_CHECK(AppendPathSeparator(m_intermedDir));
		RKIT_CHECK(AppendPathSeparator(m_dataDir));

		return ResultCode::kOK;
	}

	IDependencyNode *BuildSystemInstance::FindNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, const StringView &identifier) const
	{
		return nullptr;
	}

	Result BuildSystemInstance::AddRootNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, const StringView &identifier, UniquePtr<IDependencyNode> &&node) const
	{
		return ResultCode::kNotYetImplemented;
	}

	Result BuildSystemInstance::Build()
	{
		return ResultCode::kNotYetImplemented;
	}


	IDependencyGraphFactory *BuildSystemInstance::GetDependencyGraphFactory() const
	{
		return const_cast<BuildSystemInstance *>(this);
	}

	Result BuildSystemInstance::CreateNode(uint32_t nodeNamespace, uint32_t nodeType, const StringView &identifier, UniquePtr<IDependencyNode> &outNode) const
	{
		return ResultCode::kNotYetImplemented;
	}

	Result BuildSystemInstance::RegisterNodeFactory(uint32_t nodeNamespace, uint32_t nodeType, INodeFactory *factory)
	{
		RKIT_CHECK(m_nodeFactories.Set(NodeTypeKey(nodeNamespace, nodeType), factory));

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::AppendPathSeparator(String &str)
	{
		if (!str.EndsWith(m_pathSeparator))
		{
			RKIT_CHECK(str.Append(m_pathSeparator));
		}

		return ResultCode::kOK;
	}
}
