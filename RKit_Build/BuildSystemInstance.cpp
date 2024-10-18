#include "BuildSystemInstance.h"

#include "rkit/BuildSystem/DependencyGraph.h"

#include "rkit/Core/HashTable.h"
#include "rkit/Core/LogDriver.h"
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
	class DepsNodeCompiler final : public IDependencyNodeCompiler
	{
	public:
		DepsNodeCompiler();

		Result Initialize() override;

		Result RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;
		Result RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;

		Result CreatePrivateData(UniquePtr<IDependencyNodePrivateData> &outPrivateData) override;

		uint32_t GetVersion() const override;

	private:
		String m_path;
	};

	class DependencyNode final : public IDependencyNode
	{
	public:
		DependencyNode(IDependencyNodeCompiler *compiler);

	private:
		DependencyNode() = delete;

		IDependencyNodeCompiler *m_compiler;
	};

	class BuildSystemInstance final : public IBaseBuildSystemInstance, public IDependencyGraphFactory
	{
	public:
		BuildSystemInstance();

		Result Initialize(const StringView &targetName, const StringView &srcDir, const StringView &intermediateDir, const StringView &dataDir) override;
		IDependencyNode *FindNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation buildFileLocation, const StringView &identifier) const override;
		Result AddRootNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, UniquePtr<IDependencyNode> &&node) const override;

		Result Build() override;

		IDependencyGraphFactory *GetDependencyGraphFactory() const override;

		Result CreateNode(uint32_t nodeNamespace, uint32_t nodeType, BuildFileLocation buildFileLocation, const StringView &identifier, UniquePtr<IDependencyNode> &outNode) const override;
		Result RegisterNodeCompiler(uint32_t nodeNamespace, uint32_t nodeType, UniquePtr<IDependencyNodeCompiler> &&compiler) override;

	private:
		Result AppendPathSeparator(String &str);

		String m_targetName;
		String m_srcDir;
		String m_intermedDir;
		String m_dataDir;
		char m_pathSeparator;

		HashMap<NodeTypeKey, UniquePtr<IDependencyNodeCompiler> > m_nodeCompilers;
		Vector<UniquePtr<IDependencyNode>> m_nodes;
	};

	NodeTypeKey::NodeTypeKey(uint32_t typeNamespace, uint32_t typeID)
		: m_typeNamespace(typeNamespace)
		, m_typeID(typeID)
	{
	}

	DepsNodeCompiler::DepsNodeCompiler()
	{
	}

	Result DepsNodeCompiler::Initialize()
	{
		return ResultCode::kNotYetImplemented;
	}

	Result DepsNodeCompiler::RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result DepsNodeCompiler::RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result DepsNodeCompiler::CreatePrivateData(UniquePtr<IDependencyNodePrivateData> &outPrivateData)
	{
		return ResultCode::kNotYetImplemented;
	}

	uint32_t DepsNodeCompiler::GetVersion() const
	{
		return 1;
	}

	BuildSystemInstance::BuildSystemInstance()
		: m_pathSeparator(0)
	{
	}

	Result IBaseBuildSystemInstance::Create(UniquePtr<IBuildSystemInstance> &outInstance)
	{
		return rkit::New<BuildSystemInstance>(outInstance);
	}

	Result BuildSystemInstance::Initialize(const rkit::StringView &targetName, const StringView &srcDir, const StringView &intermediateDir, const StringView &dataDir)
	{
		m_pathSeparator = GetDrivers().m_systemDriver->GetPathSeparator();

		RKIT_CHECK(m_targetName.Set(targetName));
		RKIT_CHECK(m_srcDir.Set(srcDir));
		RKIT_CHECK(m_intermedDir.Set(intermediateDir));
		RKIT_CHECK(m_dataDir.Set(dataDir));

		RKIT_CHECK(AppendPathSeparator(m_srcDir));
		RKIT_CHECK(AppendPathSeparator(m_intermedDir));
		RKIT_CHECK(AppendPathSeparator(m_dataDir));

		UniquePtr<IDependencyNodeCompiler> depsCompiler;
		RKIT_CHECK(New<DepsNodeCompiler>(depsCompiler));

		RKIT_CHECK(RegisterNodeCompiler(kDefaultNamespace, kDepsNodeID, std::move(depsCompiler)));

		return ResultCode::kOK;
	}

	IDependencyNode *BuildSystemInstance::FindNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation buildFileLocation, const StringView &identifier) const
	{
		return nullptr;
	}

	Result BuildSystemInstance::AddRootNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, UniquePtr<IDependencyNode> &&node) const
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

	Result BuildSystemInstance::CreateNode(uint32_t nodeNamespace, uint32_t nodeType, BuildFileLocation buildFileLocation, const StringView &identifier, UniquePtr<IDependencyNode> &outNode) const
	{
		HashMap<NodeTypeKey, UniquePtr<IDependencyNodeCompiler> >::ConstIterator_t compilerIt = m_nodeCompilers.Find(NodeTypeKey(nodeNamespace, nodeType));

		if (compilerIt == m_nodeCompilers.end())
		{
			rkit::log::ErrorFmt("Unknown dependency node type %u / %u", static_cast<unsigned int>(nodeNamespace), static_cast<unsigned int>(nodeType));
			return ResultCode::kInvalidParameter;
		}

		return ResultCode::kNotYetImplemented;
	}

	Result BuildSystemInstance::RegisterNodeCompiler(uint32_t nodeNamespace, uint32_t nodeType, UniquePtr<IDependencyNodeCompiler> &&compiler)
	{
		UniquePtr<IDependencyNodeCompiler> compilerMoved(std::move(compiler));

		RKIT_CHECK(m_nodeCompilers.Set(NodeTypeKey(nodeNamespace, nodeType), std::move(compilerMoved)));

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
