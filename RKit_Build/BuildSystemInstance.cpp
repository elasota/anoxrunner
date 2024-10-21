#include "BuildSystemInstance.h"

#include "DepsNodeCompiler.h"

#include "rkit/BuildSystem/DependencyGraph.h"

#include "rkit/Core/HashTable.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/String.h"
#include "rkit/Core/StringView.h"
#include "rkit/Core/Vector.h"


namespace rkit::buildsystem
{
	class NodeTypeKey
	{
	public:
		NodeTypeKey(uint32_t typeNamespace, uint32_t typeID);

		uint32_t GetNamespace() const;
		uint32_t GetType() const;

		bool operator==(const NodeTypeKey &other) const;
		bool operator!=(const NodeTypeKey &other) const;

	private:
		NodeTypeKey() = delete;

		uint32_t m_typeNamespace;
		uint32_t m_typeID;
	};

	class FileLocationKey
	{
	public:
		FileLocationKey();
		FileLocationKey(BuildFileLocation inputLocation, const StringView &identifier);

		bool operator==(const FileLocationKey &other) const;
		bool operator!=(const FileLocationKey &other) const;

		HashValue_t ComputeHash(HashValue_t baseHash) const;

	private:
		BuildFileLocation m_location;
		const StringView &m_path;
	};

	class NodeKey
	{
	public:
		NodeKey(const NodeTypeKey &nodeTypeKey, BuildFileLocation inputLocation, const StringView &identifier);

		bool operator==(const NodeKey &other) const;
		bool operator!=(const NodeKey &other) const;

		HashValue_t ComputeHash(HashValue_t baseHash) const;

	private:
		NodeKey() = delete;

		NodeTypeKey m_typeKey;
		BuildFileLocation m_inputLocation;
		StringView m_identifier;
	};
}

RKIT_DECLARE_BINARY_HASHER(rkit::buildsystem::NodeTypeKey);

template<>
struct ::rkit::Hasher<rkit::buildsystem::FileLocationKey>
{
	static HashValue_t ComputeHash(HashValue_t baseHash, const rkit::buildsystem::FileLocationKey &value);
};

template<>
struct ::rkit::Hasher<rkit::buildsystem::NodeKey>
{
	static HashValue_t ComputeHash(HashValue_t baseHash, const rkit::buildsystem::NodeKey &value);
};

bool rkit::buildsystem::NodeTypeKey::operator==(const NodeTypeKey &other) const
{
	return m_typeNamespace == other.m_typeNamespace && m_typeID == other.m_typeID;
}

bool rkit::buildsystem::NodeTypeKey::operator!=(const NodeTypeKey &other) const
{
	return !((*this) == other);
}


rkit::buildsystem::FileLocationKey::FileLocationKey()
	: m_location(BuildFileLocation::kInvalid)
	, m_path("")
{
}

rkit::buildsystem::FileLocationKey::FileLocationKey(BuildFileLocation inputLocation, const StringView &path)
	: m_location(inputLocation)
	, m_path(path)
{
}

bool rkit::buildsystem::FileLocationKey::operator==(const FileLocationKey &other) const
{
	return m_location == other.m_location && m_path == other.m_path;
}

bool rkit::buildsystem::FileLocationKey::operator!=(const FileLocationKey &other) const
{
	return !((*this) == other);
}

rkit::HashValue_t rkit::buildsystem::FileLocationKey::ComputeHash(HashValue_t baseHash) const
{
	HashValue_t hash = baseHash;
	hash = Hasher<uint32_t>::ComputeHash(hash, static_cast<uint32_t>(m_location));
	hash = Hasher<StringView>::ComputeHash(hash, m_path);
	return hash;
}

rkit::buildsystem::NodeKey::NodeKey(const NodeTypeKey &nodeTypeKey, BuildFileLocation inputLocation, const StringView &identifier)
	: m_typeKey(nodeTypeKey)
	, m_identifier(identifier)
	, m_inputLocation(inputLocation)
{
}


bool rkit::buildsystem::NodeKey::operator==(const NodeKey &other) const
{
	return m_typeKey == other.m_typeKey && m_identifier == other.m_identifier && m_inputLocation == other.m_inputLocation;
}

bool rkit::buildsystem::NodeKey::operator!=(const NodeKey &other) const
{
	return !((*this) == other);
}

rkit::HashValue_t rkit::buildsystem::NodeKey::ComputeHash(HashValue_t baseHash) const
{
	HashValue_t hash = baseHash;
	hash = Hasher<NodeTypeKey>::ComputeHash(hash, m_typeKey);
	hash = Hasher<StringView>::ComputeHash(hash, m_identifier);
	hash = Hasher<uint32_t>::ComputeHash(hash, static_cast<uint32_t>(m_inputLocation));

	return hash;
}


rkit::HashValue_t rkit::Hasher<rkit::buildsystem::FileLocationKey>::ComputeHash(HashValue_t baseHash, const rkit::buildsystem::FileLocationKey &value)
{
	return value.ComputeHash(baseHash);
}

rkit::HashValue_t rkit::Hasher<rkit::buildsystem::NodeKey>::ComputeHash(HashValue_t baseHash, const rkit::buildsystem::NodeKey &value)
{
	return value.ComputeHash(baseHash);
}

namespace rkit::buildsystem
{
	class BuildSystemInstance;

	enum class DependencyCheckPhase
	{
		None,
		CheckFiles,
		EnumerateNodes,
		CheckNodes,
		Completed,
	};

	class DependencyNode final : public IDependencyNode
	{
	public:
		DependencyNode(IDependencyNodeCompiler *compiler, uint32_t nodeNamespace, uint32_t nodeType, BuildFileLocation inputLocation, UniquePtr<IDependencyNodePrivateData> &&privateData);

		Result Initialize(const StringView &identifier);

		IDependencyNodeCompiler *GetCompiler() const;
		uint32_t GetLastCompilerVersion() const;

		bool IsMarkedAsRoot() const;
		void MarkAsRoot();

		void SetState(DependencyState depState);

		void MarkOutOfDate() override;

		StringView GetIdentifier() const override;
		DependencyState GetDependencyState() const override;

		DependencyCheckPhase GetDependencyCheckPhase() const;
		void SetDependencyCheckPhase(DependencyCheckPhase phase);
		bool GetNextNodeDependencyToCheck(NodeDependencyInfo &outNDI);

		uint32_t GetDependencyNodeNamespace() const override;
		uint32_t GetDependencyNodeType() const override;
		BuildFileLocation GetInputFileLocation() const override;
		Result RunAnalysis(IBuildSystemInstance *instance) override;
		Result RunCompile(IBuildSystemInstance *instance) override;

		CallbackSpan<FileStatusView, const IDependencyNode *> GetProducts() const override;
		CallbackSpan<FileDependencyInfoView, const IDependencyNode *> GetFileDependencies() const override;
		CallbackSpan<NodeDependencyInfo, const IDependencyNode *> GetNodeDependencies() const override;

		Result AddProduct(const FileStatusView &fileInfo);
		Result AddFileDependency(const FileDependencyInfoView &fileInfo);
		Result AddNodeDependency(const NodeDependencyInfo &nodeInfo);

		Result Serialize(IWriteStream *stream) const override;
		Result Deserialize(IReadStream *stream) override;

	private:
		DependencyNode() = delete;

	protected:
		IDependencyNodeCompiler *m_compiler;
		UniquePtr<IDependencyNodePrivateData> m_privateData;

	private:
		class DependencyNodeCompilerFeedback final : public IDependencyNodeCompilerFeedback
		{
		public:
			explicit DependencyNodeCompilerFeedback(BuildSystemInstance *instance, DependencyNode *node);

			Result TryOpenInput(BuildFileLocation location, const StringView &path, UniquePtr<ISeekableReadStream> &inputFile) override;
			Result TryOpenOutput(BuildFileLocation location, const StringView &path, UniquePtr<ISeekableReadWriteStream> &outputFile) override;

			Result AddNodeDependency(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier) override;
			bool FindNodeTypeByFileExtension(const StringView &ext, uint32_t &outNamespace, uint32_t &outType) const override;

		private:
			BuildSystemInstance *m_buildInstance;
			DependencyNode *m_dependencyNode;
		};

		static FileStatusView GetProductByIndex(const IDependencyNode *const& node, size_t index);
		static FileDependencyInfoView GetFileDependencyByIndex(const IDependencyNode *const & node, size_t index);
		static NodeDependencyInfo GetNodeDependencyByIndex(const IDependencyNode *const & node, size_t index);

		Vector<FileStatus> m_products;
		Vector<FileDependencyInfo> m_fileDependencies;
		Vector<NodeDependencyInfo> m_nodeDependencies;

		DependencyState m_dependencyState;
		size_t m_checkNodeIndex;

		uint32_t m_nodeType;
		uint32_t m_nodeNamespace;
		uint32_t m_lastCompilerVersion;
		BuildFileLocation m_inputLocation;
		String m_identifier;

		bool m_isMarkedAsRoot;
		DependencyCheckPhase m_depCheckPhase;
	};

	class BuildSystemInstance final : public IBaseBuildSystemInstance, public IDependencyGraphFactory
	{
	public:
		BuildSystemInstance();

		Result Initialize(const StringView &targetName, const StringView &srcDir, const StringView &intermediateDir, const StringView &dataDir) override;
		IDependencyNode *FindNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier) const override;
		Result FindOrCreateNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier, IDependencyNode *&outNode) override;
		Result AddRootNode(IDependencyNode *node) override;

		Result Build(IBuildFileSystem *fs) override;

		IDependencyGraphFactory *GetDependencyGraphFactory() const override;

		Result CreateNode(uint32_t nodeNamespace, uint32_t nodeType, BuildFileLocation buildFileLocation, const StringView &identifier, UniquePtr<IDependencyNode> &outNode) const override;
		Result RegisterNodeCompiler(uint32_t nodeNamespace, uint32_t nodeType, UniquePtr<IDependencyNodeCompiler> &&compiler) override;

		Result ResolveFileStatus(BuildFileLocation location, const StringView &path, FileStatusView &outStatusView, bool &outExists);

		Result OpenFileRead(BuildFileLocation location, const StringView &path, UniquePtr<ISeekableReadStream> &outFile);

		Result RegisterNodeTypeByExtension(const StringView &ext, uint32_t nodeNamespace, uint32_t nodeType) override;
		bool FindNodeTypeByFileExtension(const StringView &ext, uint32_t &outNamespace, uint32_t &outType) const;

	private:
		struct CachedFileStatus
		{
			bool m_exists = true;
			FileStatus m_status;
		};

		class UpdateOnCloseWriteStreamWrapper final : public ISeekableReadWriteStream
		{
		public:
			UpdateOnCloseWriteStreamWrapper(CachedFileStatus *cfs);
			~UpdateOnCloseWriteStreamWrapper();

			Result Initialize(BuildFileLocation location, const StringView &path);

		private:
			CachedFileStatus *m_cfs;
			BuildFileLocation m_location;
			String m_path;
		};

		Result AddNode(UniquePtr<DependencyNode> &&node);
		Result AddRelevantNode(DependencyNode *node);
		Result ProcessTopDepCheck();
		Result CheckNodeFilesAndVersion(DependencyNode *node);
		Result CheckNodeNodeDependencies(DependencyNode *node);

		static Result ResolveCachedFileStatusCallback(void *userdata, const FileStatusView &status);

		void ErrorBlameNode(DependencyNode *node, const StringView &msg);

		Result AppendPathSeparator(String &str);

		String m_targetName;
		String m_srcDir;
		String m_intermedDir;
		String m_dataDir;
		char m_pathSeparator;

		HashMap<NodeTypeKey, UniquePtr<IDependencyNodeCompiler> > m_nodeCompilers;
		HashMap<NodeKey, DependencyNode *> m_nodeLookup;

		Vector<UniquePtr<DependencyNode>> m_nodes;
		Vector<DependencyNode *> m_rootNodes;

		Vector<DependencyNode *> m_relevantNodes;
		Vector<DependencyNode *> m_depCheckStack;

		HashMap<FileLocationKey, UniquePtr<CachedFileStatus> > m_cachedFileStatus;
		HashMap<String, NodeTypeKey> m_nodeTypesByExtension;

		IBuildFileSystem *m_fs;
	};

	NodeTypeKey::NodeTypeKey(uint32_t typeNamespace, uint32_t typeID)
		: m_typeNamespace(typeNamespace)
		, m_typeID(typeID)
	{
	}

	uint32_t NodeTypeKey::GetNamespace() const
	{
		return m_typeNamespace;
	}

	uint32_t NodeTypeKey::GetType() const
	{
		return m_typeID;
	}

	DependencyNode::DependencyNode(IDependencyNodeCompiler *compiler, uint32_t nodeNamespace, uint32_t nodeType, BuildFileLocation inputLocation, UniquePtr<IDependencyNodePrivateData> &&privateData)
		: m_compiler(compiler)
		, m_privateData(std::move(privateData))
		, m_inputLocation(inputLocation)
		, m_nodeType(nodeType)
		, m_nodeNamespace(nodeNamespace)
		, m_dependencyState(DependencyState::NotAnalyzedOrCompiled)
		, m_checkNodeIndex(0)
		, m_isMarkedAsRoot(false)
		, m_depCheckPhase(DependencyCheckPhase::None)
		, m_lastCompilerVersion(0)
	{
	}

	Result DependencyNode::Initialize(const StringView &identifier)
	{
		RKIT_CHECK(m_identifier.Set(identifier));

		return ResultCode::kOK;
	}

	IDependencyNodeCompiler *DependencyNode::GetCompiler() const
	{
		return m_compiler;
	}

	uint32_t DependencyNode::GetLastCompilerVersion() const
	{
		return m_lastCompilerVersion;
	}

	bool DependencyNode::IsMarkedAsRoot() const
	{
		return m_isMarkedAsRoot;
	}

	void DependencyNode::MarkAsRoot()
	{
		m_isMarkedAsRoot = true;
	}

	void DependencyNode::SetState(DependencyState depState)
	{
		m_dependencyState = depState;

		if (depState == DependencyState::NotAnalyzedOrCompiled)
		{
			m_products.Reset();
			m_fileDependencies.Reset();
			m_nodeDependencies.Reset();
		}

		if (depState == DependencyState::NotCompiled)
			m_products.Reset();
	}

	void DependencyNode::MarkOutOfDate()
	{
		SetState(DependencyState::NotAnalyzedOrCompiled);
	}

	StringView DependencyNode::GetIdentifier() const
	{
		return m_identifier;
	}

	DependencyState DependencyNode::GetDependencyState() const
	{
		return m_dependencyState;
	}

	DependencyCheckPhase DependencyNode::GetDependencyCheckPhase() const
	{
		return m_depCheckPhase;
	}

	void DependencyNode::SetDependencyCheckPhase(DependencyCheckPhase phase)
	{
		m_depCheckPhase = phase;

		if (phase == DependencyCheckPhase::CheckNodes)
			m_checkNodeIndex = 0;
	}

	bool DependencyNode::GetNextNodeDependencyToCheck(NodeDependencyInfo &outNDI)
	{
		if (m_checkNodeIndex == m_nodeDependencies.Count())
			return false;

		outNDI = m_nodeDependencies[m_checkNodeIndex++];
		return true;
	}

	uint32_t DependencyNode::GetDependencyNodeNamespace() const
	{
		return m_nodeNamespace;
	}

	uint32_t DependencyNode::GetDependencyNodeType() const
	{
		return m_nodeType;
	}

	BuildFileLocation DependencyNode::GetInputFileLocation() const
	{
		return m_inputLocation;
	}

	Result DependencyNode::RunAnalysis(IBuildSystemInstance *instance)
	{
		DependencyNodeCompilerFeedback feedback(static_cast<BuildSystemInstance *>(instance), this);

		RKIT_CHECK(m_compiler->RunAnalysis(this, &feedback));

		return ResultCode::kOK;
	}

	Result DependencyNode::RunCompile(IBuildSystemInstance *instance)
	{
		DependencyNodeCompilerFeedback feedback(static_cast<BuildSystemInstance *>(instance), this);

		RKIT_CHECK(m_compiler->RunCompile(this, &feedback));

		return ResultCode::kOK;
	}

	CallbackSpan<FileStatusView, const IDependencyNode *> DependencyNode::GetProducts() const
	{
		return CallbackSpan<FileStatusView, const IDependencyNode *>(GetProductByIndex, this, m_products.Count());
	}

	CallbackSpan<FileDependencyInfoView, const IDependencyNode *> DependencyNode::GetFileDependencies() const
	{
		return CallbackSpan<FileDependencyInfoView, const IDependencyNode *>(GetFileDependencyByIndex, this, m_fileDependencies.Count());
	}

	CallbackSpan<NodeDependencyInfo, const IDependencyNode *> DependencyNode::GetNodeDependencies() const
	{
		return CallbackSpan<NodeDependencyInfo, const IDependencyNode *>(GetNodeDependencyByIndex, this, m_nodeDependencies.Count());
	}

	Result DependencyNode::AddProduct(const FileStatusView &fileInfo)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result DependencyNode::AddFileDependency(const FileDependencyInfoView &fileInfo)
	{
		for (FileDependencyInfo &existingFDI : m_fileDependencies)
		{
			if (existingFDI.m_status.m_location == fileInfo.m_status.m_location && existingFDI.m_status.m_filePath == fileInfo.m_status.m_filePath)
			{
				RKIT_CHECK(existingFDI.Set(fileInfo));
				return ResultCode::kOK;
			}
		}

		FileDependencyInfo fdi;
		RKIT_CHECK(fdi.Set(fileInfo));

		RKIT_CHECK(m_fileDependencies.Append(std::move(fdi)));

		return ResultCode::kOK;
	}

	Result DependencyNode::AddNodeDependency(const NodeDependencyInfo &nodeInfo)
	{
		for (const NodeDependencyInfo &nodeInfo : m_nodeDependencies)
		{
			if (nodeInfo.m_node == nodeInfo.m_node)
				return ResultCode::kOK;
		}

		RKIT_CHECK(m_nodeDependencies.Append(nodeInfo));

		return ResultCode::kOK;
	}

	Result DependencyNode::Serialize(IWriteStream *stream) const
	{
		return ResultCode::kNotYetImplemented;
	}

	Result DependencyNode::Deserialize(IReadStream *stream)
	{
		return ResultCode::kNotYetImplemented;
	}

	DependencyNode::DependencyNodeCompilerFeedback::DependencyNodeCompilerFeedback(BuildSystemInstance *instance, DependencyNode *node)
		: m_buildInstance(instance)
		, m_dependencyNode(node)
	{
	}


	Result DependencyNode::DependencyNodeCompilerFeedback::TryOpenInput(BuildFileLocation location, const StringView &path, UniquePtr<ISeekableReadStream> &inputFile)
	{
		inputFile.Reset();

		for (FileDependencyInfoView fStatus : m_dependencyNode->GetFileDependencies())
		{
			if (fStatus.m_status.m_location == location && fStatus.m_status.m_filePath == path)
			{
				if (!fStatus.m_fileExists)
					return ResultCode::kOK;
			}
		}

		FileStatusView newFStatusView;
		bool exists = false;
		RKIT_CHECK(m_buildInstance->ResolveFileStatus(location, path, newFStatusView, exists));

		FileDependencyInfoView newDepInfo;
		newDepInfo.m_status = newFStatusView;
		newDepInfo.m_fileExists = exists;
		newDepInfo.m_mustBeUpToDate = true;

		RKIT_CHECK(m_dependencyNode->AddFileDependency(newDepInfo));

		RKIT_CHECK(m_buildInstance->OpenFileRead(location, path, inputFile));

		return ResultCode::kOK;
	}

	Result DependencyNode::DependencyNodeCompilerFeedback::TryOpenOutput(BuildFileLocation location, const StringView &path, UniquePtr<ISeekableReadWriteStream> &outputFile)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result DependencyNode::DependencyNodeCompilerFeedback::AddNodeDependency(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier)
	{
		IDependencyNode *node = nullptr;
		RKIT_CHECK(m_buildInstance->FindOrCreateNode(nodeTypeNamespace, nodeTypeID, inputFileLocation, identifier, node));

		NodeDependencyInfo depInfo;
		depInfo.m_mustBeUpToDate = true;
		depInfo.m_node = node;
		RKIT_CHECK(m_dependencyNode->AddNodeDependency(depInfo));

		return ResultCode::kOK;
	}

	bool DependencyNode::DependencyNodeCompilerFeedback::FindNodeTypeByFileExtension(const StringView &ext, uint32_t &outNamespace, uint32_t &outType) const
	{
		return m_buildInstance->FindNodeTypeByFileExtension(ext, outNamespace, outType);
	}

	FileStatusView DependencyNode::GetProductByIndex(const IDependencyNode *const & node, size_t index)
	{
		const FileStatus &fileStatus = static_cast<const DependencyNode *>(node)->m_products[index];

		return fileStatus.ToView();
	}

	FileDependencyInfoView DependencyNode::GetFileDependencyByIndex(const IDependencyNode *const &node, size_t index)
	{
		const FileDependencyInfo &fileDependencyInfo = static_cast<const DependencyNode *>(node)->m_fileDependencies[index];

		return fileDependencyInfo.ToView();
	}

	NodeDependencyInfo DependencyNode::GetNodeDependencyByIndex(const IDependencyNode *const &node, size_t index)
	{
		return static_cast<const DependencyNode *>(node)->m_nodeDependencies[index];
	}

	BuildSystemInstance::BuildSystemInstance()
		: m_pathSeparator(0)
		, m_fs(nullptr)
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
		RKIT_CHECK(RegisterNodeTypeByExtension("deps", kDefaultNamespace, kDepsNodeID));

		return ResultCode::kOK;
	}

	IDependencyNode *BuildSystemInstance::FindNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier) const
	{
		return nullptr;
	}

	Result BuildSystemInstance::FindOrCreateNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier, IDependencyNode *&outNode)
	{
		outNode = nullptr;

		{
			IDependencyNode *node = FindNode(nodeTypeNamespace, nodeTypeID, inputFileLocation, identifier);

			if (node)
			{
				outNode = node;
				return ResultCode::kOK;
			}
		}

		{
			UniquePtr<IDependencyNode> node;
			RKIT_CHECK(CreateNode(nodeTypeNamespace, nodeTypeID, inputFileLocation, identifier, node));

			IDependencyNode *nodePtr = node.Get();

			RKIT_CHECK(AddNode(node.StaticCast<DependencyNode>()));

			outNode = nodePtr;
		}

		return ResultCode::kOK;
	}


	Result BuildSystemInstance::AddRootNode(IDependencyNode *node)
	{
		DependencyNode *dnode = static_cast<DependencyNode *>(node);

		if (!dnode->IsMarkedAsRoot())
		{
			RKIT_CHECK(m_rootNodes.Append(dnode));
			dnode->MarkAsRoot();
		}

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::Build(IBuildFileSystem *fs)
	{
		m_fs = fs;
		m_cachedFileStatus.Clear();

		// Step 1: Compute initial dependency graph
		for (const UniquePtr<DependencyNode> &nodeUPtr : m_nodes)
			nodeUPtr->SetDependencyCheckPhase(DependencyCheckPhase::None);

		m_depCheckStack.Reset();
		m_relevantNodes.Reset();

		for (DependencyNode *node : m_rootNodes)
		{
			RKIT_CHECK(AddRelevantNode(node));
		}

		for (size_t i = 0; i < m_relevantNodes.Count(); i++)
		{
			DependencyNode *node = m_relevantNodes[i];

			RKIT_CHECK(m_depCheckStack.Append(node));

			while (m_depCheckStack.Count() > 0)
			{
				RKIT_CHECK(ProcessTopDepCheck());
			}
		}

		// Step 2: Compile
		for (DependencyNode *node : m_relevantNodes)
		{
			RKIT_ASSERT(node->GetDependencyState() != DependencyState::NotAnalyzedOrCompiled);

			if (node->GetDependencyState() == DependencyState::NotCompiled)
			{
				RKIT_CHECK(node->RunCompile(this));
				node->SetState(DependencyState::UpToDate);
			}
		}

		return ResultCode::kOK;
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

		UniquePtr<IDependencyNodePrivateData> privateData;
		RKIT_CHECK(compilerIt.Value()->CreatePrivateData(privateData));

		UniquePtr<DependencyNode> depNode;
		RKIT_CHECK(New<DependencyNode>(depNode, compilerIt.Value().Get(), nodeNamespace, nodeType, buildFileLocation, std::move(privateData)));

		RKIT_CHECK(depNode->Initialize(identifier));

		outNode = std::move(depNode);

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::RegisterNodeCompiler(uint32_t nodeNamespace, uint32_t nodeType, UniquePtr<IDependencyNodeCompiler> &&compiler)
	{
		UniquePtr<IDependencyNodeCompiler> compilerMoved(std::move(compiler));

		RKIT_CHECK(m_nodeCompilers.Set(NodeTypeKey(nodeNamespace, nodeType), std::move(compilerMoved)));

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::AddNode(UniquePtr<DependencyNode> &&node)
	{
		NodeTypeKey typeKey(node->GetDependencyNodeNamespace(), node->GetDependencyNodeType());
		NodeKey nodeKey(typeKey, node->GetInputFileLocation(), node->GetIdentifier());

		DependencyNode *nodePtr = node.Get();

		RKIT_CHECK(m_nodes.Append(std::move(node)));

		rkit::Result htabAddResult = m_nodeLookup.Set(nodeKey, nodePtr);
		if (!htabAddResult.IsOK())
			m_nodes.RemoveRange(m_nodes.Count() - 1, 1);

		return htabAddResult;
	}

	Result BuildSystemInstance::AddRelevantNode(DependencyNode *node)
	{
		if (node->GetDependencyCheckPhase() == DependencyCheckPhase::None)
		{
			RKIT_CHECK(m_relevantNodes.Append(node));
			node->SetDependencyCheckPhase(DependencyCheckPhase::CheckFiles);
		}

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::ProcessTopDepCheck()
	{
		DependencyNode *node = m_depCheckStack[m_depCheckStack.Count() - 1];

		if (node->GetDependencyCheckPhase() == DependencyCheckPhase::CheckFiles)
		{
			// This may lower the state to NotAnalyzedOrCompiled
			RKIT_CHECK(CheckNodeFilesAndVersion(node));

			node->SetDependencyCheckPhase(DependencyCheckPhase::EnumerateNodes);
		}

		if (node->GetDependencyCheckPhase() == DependencyCheckPhase::EnumerateNodes)
		{
			// This may lower the state to NotCompiled
			DependencyState depState = node->GetDependencyState();

			if (depState == DependencyState::NotAnalyzedOrCompiled)
			{
				if (node->GetCompiler()->HasAnalysisStage())
				{
					RKIT_CHECK(node->RunAnalysis(this));
				}

				depState = DependencyState::NotCompiled;
				node->SetState(depState);
			}

			node->SetDependencyCheckPhase(DependencyCheckPhase::CheckNodes);

			return ResultCode::kOK;
		}

		RKIT_ASSERT(node->GetDependencyCheckPhase() == DependencyCheckPhase::CheckNodes);

		RKIT_CHECK(CheckNodeNodeDependencies(node));

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::CheckNodeFilesAndVersion(DependencyNode *node)
	{
		if (node->GetLastCompilerVersion() != node->GetCompiler()->GetVersion())
		{
			node->MarkOutOfDate();
			return ResultCode::kOK;
		}

		for (FileDependencyInfoView fdiView : node->GetFileDependencies())
		{
			if (!fdiView.m_mustBeUpToDate)
				continue;

			FileStatusView fileStatus;
			bool exists = false;
			RKIT_CHECK(ResolveFileStatus(fdiView.m_status.m_location, fdiView.m_status.m_filePath, fileStatus, exists));

			if (exists != fdiView.m_fileExists)
			{
				node->MarkOutOfDate();
				return ResultCode::kOK;
			}

			if (exists && fileStatus != fdiView.m_status)
			{
				node->MarkOutOfDate();
				return ResultCode::kOK;
			}
		}

		for (FileStatusView productStatus : node->GetProducts())
		{
			FileStatusView fileStatus;
			bool exists = false;
			RKIT_CHECK(ResolveFileStatus(productStatus.m_location, productStatus.m_filePath, fileStatus, exists));

			if (!exists)
			{
				node->MarkOutOfDate();
				return ResultCode::kOK;
			}

			if (fileStatus != productStatus)
			{
				node->MarkOutOfDate();
				return ResultCode::kOK;
			}
		}

		if (node->GetDependencyState() == DependencyState::NotCompiled)
			node->SetState(DependencyState::UpToDate);

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::CheckNodeNodeDependencies(DependencyNode *node)
	{
		bool allUpToDate = true;

		NodeDependencyInfo nodeDep;
		if (!node->GetNextNodeDependencyToCheck(nodeDep))
		{
			node->SetDependencyCheckPhase(DependencyCheckPhase::Completed);

			m_depCheckStack.RemoveRange(m_depCheckStack.Count() - 1, 1);

			return ResultCode::kOK;
		}

		if (!nodeDep.m_mustBeUpToDate)
			return ResultCode::kOK;

		DependencyNode *nodeDepNode = static_cast<DependencyNode *>(nodeDep.m_node);
		if (nodeDepNode->GetDependencyState() != DependencyState::UpToDate)
			node->SetState(DependencyState::NotCompiled);

		if (nodeDepNode->GetDependencyCheckPhase() == DependencyCheckPhase::Completed)
			return ResultCode::kOK;

		if (nodeDepNode->GetDependencyCheckPhase() != DependencyCheckPhase::None)
		{
			ErrorBlameNode(nodeDepNode, "Circular dependency");
			return ResultCode::kOperationFailed;
		}

		RKIT_CHECK(AddRelevantNode(nodeDepNode));

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::ResolveFileStatus(BuildFileLocation location, const StringView &path, FileStatusView &outStatusView, bool &outExists)
	{
		// Keep the behavior here in sync with ResolveFileStatus
		{
			FileLocationKey lookupLocKey(location, path);

			HashMap<FileLocationKey, UniquePtr<CachedFileStatus> >::ConstIterator_t lookupIt = m_cachedFileStatus.Find(lookupLocKey);
			if (lookupIt != m_cachedFileStatus.end())
			{
				const CachedFileStatus *fStatus = lookupIt.Value().Get();

				outExists = fStatus->m_exists;

				if (fStatus->m_exists)
					outStatusView = fStatus->m_status.ToView();
				else
					outStatusView = FileStatusView();

				return ResultCode::kOK;
			}
		}

		UniquePtr<CachedFileStatus> fileStatus;
		RKIT_CHECK(New<CachedFileStatus>(fileStatus));

		RKIT_CHECK(m_fs->ResolveFileStatusIfExists(location, path, fileStatus.Get(), ResolveCachedFileStatusCallback));

		if (!fileStatus->m_exists)
		{
			RKIT_CHECK(fileStatus->m_status.m_filePath.Set(path));
		}

		const CachedFileStatus *cfs = fileStatus.Get();

		FileLocationKey insertLocKey(location, fileStatus->m_status.m_filePath);
		RKIT_CHECK(m_cachedFileStatus.Set(insertLocKey, std::move(fileStatus)));

		if (cfs->m_exists)
			outStatusView = cfs->m_status.ToView();
		else
			outStatusView = FileStatusView();

		outExists = cfs->m_exists;

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::OpenFileRead(BuildFileLocation location, const StringView &path, UniquePtr<ISeekableReadStream> &outFile)
	{
		// Keep the behavior here in sync with ResolveFileStatus
		ISystemDriver *sysDriver = GetDrivers().m_systemDriver;

		if (location == rkit::buildsystem::BuildFileLocation::kSourceDir)
		{
			// Try to import file from the root directory
			outFile = sysDriver->OpenFileRead(rkit::FileLocation::kDataSourceDirectory, path.GetChars());
			if (outFile.Get())
				return ResultCode::kOK;
		}

		return ResultCode::kNotYetImplemented;
	}

	Result BuildSystemInstance::RegisterNodeTypeByExtension(const StringView &ext, uint32_t nodeNamespace, uint32_t nodeType)
	{
		NodeTypeKey nodeTypeKey(nodeNamespace, nodeType);

		HashMap<String, NodeTypeKey>::Iterator_t it = m_nodeTypesByExtension.Find(ext);
		if (it != m_nodeTypesByExtension.end())
		{
			it.Value() = nodeTypeKey;
			return ResultCode::kOK;
		}

		String str;
		RKIT_CHECK(str.Set(ext));
		RKIT_CHECK(m_nodeTypesByExtension.Set(std::move(str), nodeTypeKey));

		return ResultCode::kOK;
	}

	bool BuildSystemInstance::FindNodeTypeByFileExtension(const StringView &ext, uint32_t &outNamespace, uint32_t &outType) const
	{
		HashMap<String, NodeTypeKey>::ConstIterator_t it = m_nodeTypesByExtension.Find(ext);
		if (it == m_nodeTypesByExtension.end())
			return false;

		outNamespace = it.Value().GetNamespace();
		outType = it.Value().GetType();

		return true;
	}

	Result BuildSystemInstance::ResolveCachedFileStatusCallback(void *userdata, const FileStatusView &status)
	{
		BuildSystemInstance::CachedFileStatus *cfs = static_cast<CachedFileStatus *>(userdata);

		FileStatus &fStatus = cfs->m_status;

		RKIT_CHECK(fStatus.m_filePath.Set(status.m_filePath));

		fStatus.m_location = status.m_location;
		fStatus.m_fileSize = status.m_fileSize;
		fStatus.m_fileTime = status.m_fileTime;

		cfs->m_exists = true;

		return ResultCode::kOK;
	}

	void BuildSystemInstance::ErrorBlameNode(DependencyNode *node, const StringView &msg)
	{
		rkit::log::ErrorFmt("Node %s: %s", node->GetIdentifier().GetChars(), msg.GetChars());
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
