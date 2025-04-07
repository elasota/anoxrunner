#include "BuildSystemInstance.h"

#include "DepsNodeCompiler.h"
#include "RenderPipelineLibraryCompiler.h"

#include "rkit/BuildSystem/DependencyGraph.h"

#include "rkit/Core/BufferStream.h"
#include "rkit/Core/FileAttributes.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/HybridVector.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/String.h"
#include "rkit/Core/StringPool.h"
#include "rkit/Core/StringView.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Vector.h"

#include "rkit/Data/ContentID.h"

#include "rkit/Utilities/Sha2.h"

#include <algorithm>

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
		FileLocationKey(BuildFileLocation inputLocation, const CIPathView &path);

		bool operator==(const FileLocationKey &other) const;
		bool operator!=(const FileLocationKey &other) const;

		bool operator<(const FileLocationKey &other) const;

		FileLocationKey &operator=(const FileLocationKey &other);

		HashValue_t ComputeHash(HashValue_t baseHash) const;

	private:
		BuildFileLocation m_location;
		CIPathView m_path;
	};

	class DirectoryScanKey
	{
	public:
		DirectoryScanKey();
		DirectoryScanKey(BuildFileLocation inputLocation, const CIPathView &path, bool directoryMode);

		bool operator==(const DirectoryScanKey &other) const;
		bool operator!=(const DirectoryScanKey &other) const;

		bool operator<(const DirectoryScanKey &other) const;

		DirectoryScanKey &operator=(const DirectoryScanKey &other);

		HashValue_t ComputeHash(HashValue_t baseHash) const;

	private:
		BuildFileLocation m_location;
		CIPathView m_path;
		bool m_directoryMode;
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
struct ::rkit::Hasher<rkit::buildsystem::DirectoryScanKey>
{
	static HashValue_t ComputeHash(HashValue_t baseHash, const rkit::buildsystem::DirectoryScanKey &value);
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

rkit::buildsystem::FileLocationKey::FileLocationKey(BuildFileLocation inputLocation, const CIPathView &path)
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


bool rkit::buildsystem::FileLocationKey::operator<(const FileLocationKey &other) const
{
	if (m_location != other.m_location)
		return m_location < other.m_location;

	return m_path < other.m_path;
}

rkit::buildsystem::FileLocationKey &rkit::buildsystem::FileLocationKey::operator=(const FileLocationKey &other)
{
	m_location = other.m_location;
	m_path = other.m_path;
	return *this;
}

rkit::HashValue_t rkit::buildsystem::FileLocationKey::ComputeHash(HashValue_t baseHash) const
{
	HashValue_t hash = baseHash;
	hash = Hasher<uint32_t>::ComputeHash(hash, static_cast<uint32_t>(m_location));
	hash = Hasher<rkit::CIPathView>::ComputeHash(hash, m_path);
	return hash;
}



rkit::buildsystem::DirectoryScanKey::DirectoryScanKey()
	: m_location(BuildFileLocation::kInvalid)
	, m_path("")
	, m_directoryMode(false)
{
}

rkit::buildsystem::DirectoryScanKey::DirectoryScanKey(BuildFileLocation inputLocation, const CIPathView &path, bool directoryMode)
	: m_location(inputLocation)
	, m_path(path)
	, m_directoryMode(directoryMode)
{
}

bool rkit::buildsystem::DirectoryScanKey::operator==(const DirectoryScanKey &other) const
{
	return m_location == other.m_location && m_path == other.m_path && m_directoryMode == other.m_directoryMode;
}

bool rkit::buildsystem::DirectoryScanKey::operator!=(const DirectoryScanKey &other) const
{
	return !((*this) == other);
}


bool rkit::buildsystem::DirectoryScanKey::operator<(const DirectoryScanKey &other) const
{
	if (m_location != other.m_location)
		return m_location < other.m_location;

	if (m_path != other.m_path)
		return m_path < other.m_path;

	return m_directoryMode < other.m_directoryMode;
}

rkit::buildsystem::DirectoryScanKey &rkit::buildsystem::DirectoryScanKey::operator=(const DirectoryScanKey &other)
{
	m_location = other.m_location;
	m_path = other.m_path;
	m_directoryMode = other.m_directoryMode;
	return *this;
}

rkit::HashValue_t rkit::buildsystem::DirectoryScanKey::ComputeHash(HashValue_t baseHash) const
{
	HashValue_t hash = baseHash;
	hash = Hasher<uint32_t>::ComputeHash(hash, static_cast<uint32_t>(m_location));
	hash = Hasher<rkit::CIPathView>::ComputeHash(hash, m_path);
	hash = Hasher<bool>::ComputeHash(hash, m_directoryMode);
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

rkit::HashValue_t rkit::Hasher<rkit::buildsystem::DirectoryScanKey>::ComputeHash(HashValue_t baseHash, const rkit::buildsystem::DirectoryScanKey &value)
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
	class DependencyNode;

	enum class DependencyCheckPhase
	{
		None,
		CheckFiles,
		EnumerateNodes,
		CheckNodes,
		Completed,
	};

	namespace serializer
	{
		template<class T>
		static Result SerializeVector(IWriteStream &stream, StringPoolBuilder &stringPool, const Vector<T> &vec);

		template<class T>
		static Result SerializeEnum(IWriteStream &stream, const T &value);

		static Result SerializeString(IWriteStream &stream, StringPoolBuilder &stringPool, const StringView &str);
		static Result SerializeCIPath(IWriteStream &stream, StringPoolBuilder &stringPool, const CIPathView &path);

		static Result Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, const data::ContentID &contentID);
		static Result Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, const CIPath &path);
		static Result Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, const FileStatus &fs);
		static Result Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, const DirectoryScan &ds);
		static Result Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, const FileDependencyInfo &fdi);
		static Result Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, const DirectoryScanDependencyInfo &dsdi);
		static Result Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, const NodeDependencyInfo &ndi);
		static Result Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, bool value);

		static Result SerializeCompactSize(IWriteStream &stream, size_t sz);

		class DeserializeResolver final : public IDeserializeResolver
		{
		public:
			DeserializeResolver(const Span<const UniquePtr<DependencyNode>> &nodes, const Span<const String> &strings);

			Result GetString(size_t index, StringView &outString) const override;
			Result GetDependencyNode(size_t index, IDependencyNode *&outNode) const override;

		private:
			Span<const UniquePtr<DependencyNode>> m_nodes;
			Span<const String> m_strings;
		};

		template<class T>
		static Result DeserializeVector(IReadStream &stream, const IDeserializeResolver &resolver, Vector<T> &vec);

		template<class T>
		static Result DeserializeEnum(IReadStream &stream, T &value);

		static Result DeserializeString(IReadStream &stream, const IDeserializeResolver &resolver, String &str);
		static Result DeserializeCIPath(IReadStream &stream, const IDeserializeResolver &resolver, CIPath &path);

		static Result Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, data::ContentID &contentID);
		static Result Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, CIPath &path);
		static Result Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, FileStatus &fs);
		static Result Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, DirectoryScan &ds);
		static Result Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, FileDependencyInfo &fdi);
		static Result Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, DirectoryScanDependencyInfo &dsdi);
		static Result Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, NodeDependencyInfo &ndi);
		static Result Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, bool &value);

		static Result DeserializeCompactSize(IReadStream &stream, size_t &sz);
	}

	class DependencyNode final : public IDependencyNode
	{
	public:
		DependencyNode(IDependencyNodeCompiler *compiler, uint32_t nodeNamespace, uint32_t nodeType, Vector<uint8_t> &&content, BuildFileLocation inputLocation);

		Result Initialize(const StringView &identifier);
		Result Initialize(const StringView &identifier, const StringView &origin);

		IDependencyNodeCompiler *GetCompiler() const;
		uint32_t GetLastCompilerVersion() const;

		bool IsMarkedAsRoot() const;
		void MarkAsRoot();

		void SetState(DependencyState depState);

		void MarkOutOfDate() override;
		bool WasCompiled() const override;
		bool IsContentBased() const override;

		Span<const uint8_t> GetContent() const override;

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

		CallbackSpan<FileStatusView, const IDependencyNode *> GetAnalysisProducts() const override;
		CallbackSpan<FileStatusView, const IDependencyNode *> GetCompileProducts() const override;
		CallbackSpan<data::ContentID, const IDependencyNode *> GetCompileCASProducts() const override;
		CallbackSpan<FileDependencyInfoView, const IDependencyNode *> GetAnalysisFileDependencies() const override;
		CallbackSpan<FileDependencyInfoView, const IDependencyNode *> GetCompileFileDependencies() const override;
		CallbackSpan<DirectoryScanDependencyInfoView, const IDependencyNode *> GetAnalysisDirectoryScanDependencies() const override;
		CallbackSpan<DirectoryScanDependencyInfoView, const IDependencyNode *> GetCompileDirectoryScanDependencies() const override;
		CallbackSpan<NodeDependencyInfo, const IDependencyNode *> GetNodeDependencies() const override;

		Result FindOrAddAnalysisProduct(BuildFileLocation location, const CIPathView &path, size_t &outIndex);
		Result FindOrAddCompileProduct(BuildFileLocation location, const CIPathView &path, size_t &outIndex);
		Result AddCASProduct(const data::ContentID &contentID);

		Result AddAnalysisFileDependency(const FileDependencyInfoView &fileInfo);
		Result AddCompileFileDependency(const FileDependencyInfoView &fileInfo);
		Result AddAnalysisDirectoryScanDependency(const DirectoryScanDependencyInfoView &dirScanInfo);
		Result AddCompileDirectoryScanDependency(const DirectoryScanDependencyInfoView &dirScanInfo);
		Result AddNodeDependency(const NodeDependencyInfo &nodeInfo);

		Result SerializeInitialState(IWriteStream &stream) const;
		static Result DeserializeInitialState(IReadStream &stream, uint32_t &outNodeNamespace, uint32_t &outNodeType, Vector<uint8_t> &outContent, BuildFileLocation &outInputLocation);

		Result Serialize(IWriteStream &stream, StringPoolBuilder &stringPool) const override;
		Result Deserialize(IReadStream &stream, const IDeserializeResolver &resolver) override;

		Result MarkProductFinished(bool isCompilePhase, size_t productIndex, const FileStatusView &fstatus);

		void SetSerializedIndex(size_t index);
		size_t GetSerializedIndex() const;

	private:
		DependencyNode() = delete;

	protected:
		IDependencyNodeCompiler *m_compiler;

	private:
		class DependencyNodeCompilerFeedback final : public IDependencyNodeCompilerFeedback
		{
		public:
			explicit DependencyNodeCompilerFeedback(BuildSystemInstance *instance, DependencyNode *node, bool isCompilePhase);

			Result CheckInputExists(BuildFileLocation location, const CIPathView &path, bool &outExists) override;
			Result OpenInput(BuildFileLocation location, const CIPathView &path, UniquePtr<ISeekableReadStream> &inputFile) override;
			Result TryOpenInput(BuildFileLocation location, const CIPathView &path, UniquePtr<ISeekableReadStream> &inputFile) override;
			Result OpenOutput(BuildFileLocation location, const CIPathView &path, UniquePtr<ISeekableReadWriteStream> &outputFile) override;
			Result AddAnonymousDeployableContent(BuildFileLocation location, const CIPathView &path) override;

			Result IndexCAS(BuildFileLocation location, const CIPathView &path, data::ContentID &outContentID) override;

			Result AddNodeDependency(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier) override;
			bool FindNodeTypeByFileExtension(const StringSliceView &ext, uint32_t &outNamespace, uint32_t &outType) const override;

			Result EnumerateFiles(BuildFileLocation location, const CIPathView &path, void *userdata, EnumerateFilesResultCallback_t resultCallback) override;
			Result EnumerateDirectories(BuildFileLocation location, const CIPathView &path, void *userdata, EnumerateFilesResultCallback_t resultCallback) override;

			IBuildSystemInstance *GetBuildSystemInstance() const override;

			void MarkOutputFileFinished(size_t productIndex, BuildFileLocation location, const CIPathView &path);

			Result CheckFault() const;

		private:
			Result CheckedMarkOutputFileFinished(size_t productIndex, BuildFileLocation location, const CIPathView &path);

			Result InternalEnumerateFilesOrDirectories(BuildFileLocation location, const CIPathView &path, bool directoryMode, void *userdata, EnumerateFilesResultCallback_t resultCallback);

			BuildSystemInstance *m_buildInstance;
			DependencyNode *m_dependencyNode;
			bool m_isCompilePhase;

			Result m_fault;
		};

		class FeedbackWrapperStream final : public ISeekableReadWriteStream
		{
		public:
			FeedbackWrapperStream(DependencyNodeCompilerFeedback &feedback, size_t productIndex, BuildFileLocation location, CIPath &&path, UniquePtr<ISeekableReadWriteStream> &&stream);
			~FeedbackWrapperStream();

			Result ReadPartial(void *data, size_t count, size_t &outCountRead) override;
			Result WritePartial(const void *data, size_t count, size_t &outCountWritten) override;
			Result Flush() override;

			Result SeekStart(FilePos_t pos) override;
			Result SeekCurrent(FileOffset_t pos) override;
			Result SeekEnd(FileOffset_t pos) override;

			FilePos_t Tell() const override;
			FilePos_t GetSize() const override;

			Result Truncate(FilePos_t newSize) override;

		private:
			DependencyNodeCompilerFeedback &m_feedback;

			size_t m_productIndex;
			BuildFileLocation m_location;
			CIPath m_path;
			UniquePtr<ISeekableReadWriteStream> m_stream;
		};

		static FileStatusView GetAnalysisProductByIndex(const IDependencyNode *const& node, size_t index);
		static FileStatusView GetCompileProductByIndex(const IDependencyNode *const &node, size_t index);
		static data::ContentID GetCompileCASProductByIndex(const IDependencyNode *const &node, size_t index);
		static FileDependencyInfoView GetAnalysisFileDependencyByIndex(const IDependencyNode *const & node, size_t index);
		static FileDependencyInfoView GetCompileFileDependencyByIndex(const IDependencyNode *const &node, size_t index);
		static DirectoryScanDependencyInfoView GetAnalysisDirectoryScanDependencyByIndex(const IDependencyNode *const &node, size_t index);
		static DirectoryScanDependencyInfoView GetCompileDirectoryScanDependencyByIndex(const IDependencyNode *const &node, size_t index);
		static NodeDependencyInfo GetNodeDependencyByIndex(const IDependencyNode *const & node, size_t index);

		static Result FindOrAddProduct(Vector<FileStatus> &products, BuildFileLocation location, const CIPathView &path, size_t &outIndex);
		static Result AddFileDependency(Vector<FileDependencyInfo> &fileDependencies, const FileDependencyInfoView &fileInfo);
		static Result AddDirectoryScanDependency(Vector<DirectoryScanDependencyInfo> &dirScanDependencies, const DirectoryScanDependencyInfoView &dirScanInfo);

		Vector<FileStatus> m_analysisProducts;
		Vector<FileStatus> m_compileProducts;
		Vector<data::ContentID> m_casProducts;
		Vector<FileDependencyInfo> m_analysisFileDependencies;
		Vector<FileDependencyInfo> m_compileFileDependencies;
		Vector<DirectoryScanDependencyInfo> m_analysisDirectoryScanDependencies;
		Vector<DirectoryScanDependencyInfo> m_compileDirectoryScanDependencies;
		Vector<NodeDependencyInfo> m_nodeDependencies;

		Vector<uint8_t> m_content;

		DependencyState m_dependencyState;
		size_t m_checkNodeIndex;

		uint32_t m_nodeNamespace;
		uint32_t m_nodeType;
		uint32_t m_lastCompilerVersion;
		BuildFileLocation m_inputLocation;
		String m_identifier;

		bool m_isMarkedAsRoot;
		DependencyCheckPhase m_depCheckPhase;

		size_t m_serializedIndex;
		bool m_wasCompiled;
	};

	struct BuildCacheInstanceInfo
	{
		uint64_t m_filePos = 0;
		uint64_t m_size = 0;
	};

	struct BuildCacheFileHeader
	{
		static const uint32_t kCacheIdentifier = RKIT_FOURCC('B', 'S', 'C', 'F');
		static const uint32_t kCacheVersion = 1;

		uint32_t m_identifier = 0;
		uint32_t m_version = 0;

		BuildCacheInstanceInfo m_instances[2];

		uint32_t m_activeInstance = 0;
	};

	class BuildSystemInstance final : public IBaseBuildSystemInstance, public IDependencyGraphFactory
	{
	public:
		BuildSystemInstance();

		Result Initialize(const StringView &targetName, const OSAbsPathView &srcDir, const OSAbsPathView &intermediateDir, const OSAbsPathView &dataDir) override;
		IDependencyNode *FindNamedNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier) const override;
		IDependencyNode *FindContentNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const Span<const uint8_t> &content) const override;
		Result FindOrCreateNamedNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier, IDependencyNode *&outNode) override;
		Result FindOrCreateContentNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, Vector<uint8_t> &&content, IDependencyNode *&outNode) override;
		Result AddRootNode(IDependencyNode *node) override;
		Result AddPostBuildAction(IBuildSystemAction *action) override;

		Result LoadCache() override;

		Result Build(IBuildFileSystem *fs) override;

		IDependencyGraphFactory *GetDependencyGraphFactory() const override;

		Result CreateNamedNode(uint32_t nodeNamespace, uint32_t nodeType, BuildFileLocation buildFileLocation, const StringView &identifier, UniquePtr<IDependencyNode> &outNode) const override;
		Result CreateContentNode(uint32_t nodeNamespace, uint32_t nodeType, BuildFileLocation buildFileLocation, Vector<uint8_t> &&content, UniquePtr<IDependencyNode> &outNode) const override;
		Result RegisterNodeCompiler(uint32_t nodeNamespace, uint32_t nodeType, UniquePtr<IDependencyNodeCompiler> &&compiler) override;

		Result ResolveFileStatus(BuildFileLocation location, const CIPathView &path, bool allowDirectories, FileStatusView &outStatusView, bool cached, bool &outExists);
		Result ResolveDirectoryScan(BuildFileLocation location, const CIPathView &path, bool directoryMode, DirectoryScanView &outScanView, bool cached, bool &outExists);

		Result TryOpenFileRead(BuildFileLocation location, const CIPathView &path, UniquePtr<ISeekableReadStream> &outFile) override;
		Result OpenFileWrite(BuildFileLocation location, const CIPathView &path, UniquePtr<ISeekableReadWriteStream> &outFile) override;

		Result EnumerateDirectories(BuildFileLocation location, const CIPathView &path, void *userdata, EnumerateFilesResultCallback_t resultCallback) override;
		Result EnumerateFiles(BuildFileLocation location, const CIPathView &path, void *userdata, EnumerateFilesResultCallback_t resultCallback) override;

		Result RegisterNodeTypeByExtension(const StringSliceView &ext, uint32_t nodeNamespace, uint32_t nodeType) override;
		bool FindNodeTypeByFileExtension(const StringSliceView &ext, uint32_t &outNamespace, uint32_t &outType) const;

		CallbackSpan<IDependencyNode *, const IBuildSystemInstance *> GetBuildRelevantNodes() const override;

		Result RegisterCASSource(const data::ContentID &contentID, BuildFileLocation inputFileLocation, const CIPathView &path) override;

	private:
		struct CachedFileStatus
		{
			bool m_exists = true;
			FileStatus m_status;
		};

		struct CachedDirScan
		{
			bool m_exists = true;
			DirectoryScan m_scan;
		};

		struct PrintableFourCC
		{
			explicit PrintableFourCC(uint32_t fourCC);

			const char *GetChars() const;

			char m_chars[5];
		};

		struct ContentID
		{
			static const size_t kContentStringSize = 51;	// 255 bits

			char m_chars[kContentStringSize + 1];

			StringView GetStringView() const;
		};

		struct CASSource
		{
			BuildFileLocation m_location = BuildFileLocation::kInvalid;
			CIPath m_path;
		};

		static ContentID CreateContentID(const Span<const uint8_t> &content);

		Result CreateNode(uint32_t nodeNamespace, uint32_t nodeType, BuildFileLocation buildFileLocation, const StringView &identifier, Vector<uint8_t> &&content, UniquePtr<IDependencyNode> &outNode) const;

		Result AddNode(UniquePtr<DependencyNode> &&node);
		Result AddRelevantNode(DependencyNode *node);
		Result ProcessTopDepCheck();
		Result CheckNodeFilesAndVersion(DependencyNode *node);
		Result CheckNodeNodeDependencies(DependencyNode *node);

		static Result ResolveCachedFileStatusCallback(void *userdata, const FileStatusView &status);

		void ErrorBlameNode(DependencyNode *node, const StringView &msg);

		Result ConstructIntermediatePath(OSAbsPath &outStr, const CIPathView &path) const;
		Result ConstructOutputPath(OSAbsPath &outStr, const CIPathView &path) const;

		static PrintableFourCC FourCCToPrintable(uint32_t fourCC);

		static CIPathView GetCacheFileName();

		static IDependencyNode *GetRelevantNodeByIndex(const IBuildSystemInstance * const& instance, size_t index);

		Result CheckedLoadCache(ISeekableReadStream &stream, FilePos_t pos);
		Result SaveCache();

		Result InternalEnumerateFilesOrDirectories(BuildFileLocation location, const CIPathView &path, bool isDirectoryMode, void *userdata, EnumerateFilesResultCallback_t resultCallback);

		Result UpdateCAS(const data::ContentID &contentID);

		String m_targetName;
		OSAbsPath m_srcDir;
		OSAbsPath m_intermedDir;
		OSAbsPath m_dataDir;

		HashMap<NodeTypeKey, UniquePtr<IDependencyNodeCompiler> > m_nodeCompilers;
		HashMap<NodeKey, DependencyNode *> m_nodeLookup;

		Vector<UniquePtr<DependencyNode>> m_nodes;
		Vector<DependencyNode *> m_rootNodes;

		Vector<DependencyNode *> m_relevantNodes;
		Vector<DependencyNode *> m_depCheckStack;

		Vector<IBuildSystemAction *> m_postBuildActions;

		HashMap<FileLocationKey, UniquePtr<CachedFileStatus> > m_cachedFileStatus;
		HashMap<DirectoryScanKey, UniquePtr<CachedDirScan> > m_cachedDirScan;
		HashMap<String, NodeTypeKey> m_nodeTypesByExtension;

		HashMap<data::ContentID, CASSource> m_casSources;

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

	DependencyNode::DependencyNode(IDependencyNodeCompiler *compiler, uint32_t nodeNamespace, uint32_t nodeType, Vector<uint8_t> &&content, BuildFileLocation inputLocation)
		: m_compiler(compiler)
		, m_inputLocation(inputLocation)
		, m_nodeType(nodeType)
		, m_nodeNamespace(nodeNamespace)
		, m_dependencyState(DependencyState::NotAnalyzedOrCompiled)
		, m_checkNodeIndex(0)
		, m_isMarkedAsRoot(false)
		, m_depCheckPhase(DependencyCheckPhase::None)
		, m_lastCompilerVersion(0)
		, m_serializedIndex(0)
		, m_wasCompiled(false)
		, m_content(std::move(content))
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
			m_analysisProducts.Reset();
			m_analysisFileDependencies.Reset();
			m_nodeDependencies.Reset();

			m_compileProducts.Reset();
			m_compileFileDependencies.Reset();
		}

		if (depState == DependencyState::NotCompiled)
		{
			m_compileProducts.Reset();
			m_compileFileDependencies.Reset();
		}
	}

	void DependencyNode::MarkOutOfDate()
	{
		SetState(DependencyState::NotAnalyzedOrCompiled);
	}

	bool DependencyNode::WasCompiled() const
	{
		return m_wasCompiled;
	}

	bool DependencyNode::IsContentBased() const
	{
		return m_content.Count() > 0;
	}

	Span<const uint8_t> DependencyNode::GetContent() const
	{
		return m_content.ToSpan();
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
		DependencyNodeCompilerFeedback feedback(static_cast<BuildSystemInstance *>(instance), this, false);

		m_lastCompilerVersion = m_compiler->GetVersion();
		RKIT_CHECK(m_compiler->RunAnalysis(this, &feedback));

		return ResultCode::kOK;
	}

	Result DependencyNode::RunCompile(IBuildSystemInstance *instance)
	{
		DependencyNodeCompilerFeedback feedback(static_cast<BuildSystemInstance *>(instance), this, true);

		m_lastCompilerVersion = m_compiler->GetVersion();
		RKIT_CHECK(m_compiler->RunCompile(this, &feedback));

		m_wasCompiled = true;
		return ResultCode::kOK;
	}

	CallbackSpan<FileStatusView, const IDependencyNode *> DependencyNode::GetAnalysisProducts() const
	{
		return CallbackSpan<FileStatusView, const IDependencyNode *>(GetAnalysisProductByIndex, this, m_analysisProducts.Count());
	}

	CallbackSpan<FileStatusView, const IDependencyNode *> DependencyNode::GetCompileProducts() const
	{
		return CallbackSpan<FileStatusView, const IDependencyNode *>(GetCompileProductByIndex, this, m_compileProducts.Count());
	}

	CallbackSpan<data::ContentID, const IDependencyNode *> DependencyNode::GetCompileCASProducts() const
	{
		return CallbackSpan<data::ContentID, const IDependencyNode *>(GetCompileCASProductByIndex, this, m_casProducts.Count());
	}

	CallbackSpan<FileDependencyInfoView, const IDependencyNode *> DependencyNode::GetAnalysisFileDependencies() const
	{
		return CallbackSpan<FileDependencyInfoView, const IDependencyNode *>(GetAnalysisFileDependencyByIndex, this, m_analysisFileDependencies.Count());
	}

	CallbackSpan<FileDependencyInfoView, const IDependencyNode *> DependencyNode::GetCompileFileDependencies() const
	{
		return CallbackSpan<FileDependencyInfoView, const IDependencyNode *>(GetCompileFileDependencyByIndex, this, m_compileFileDependencies.Count());
	}

	CallbackSpan<NodeDependencyInfo, const IDependencyNode *> DependencyNode::GetNodeDependencies() const
	{
		return CallbackSpan<NodeDependencyInfo, const IDependencyNode *>(GetNodeDependencyByIndex, this, m_nodeDependencies.Count());
	}

	CallbackSpan<DirectoryScanDependencyInfoView, const IDependencyNode *> DependencyNode::GetAnalysisDirectoryScanDependencies() const
	{
		return CallbackSpan<DirectoryScanDependencyInfoView, const IDependencyNode *>(GetAnalysisDirectoryScanDependencyByIndex, this, m_analysisDirectoryScanDependencies.Count());
	}

	CallbackSpan<DirectoryScanDependencyInfoView, const IDependencyNode *> DependencyNode::GetCompileDirectoryScanDependencies() const
	{
		return CallbackSpan<DirectoryScanDependencyInfoView, const IDependencyNode *>(GetCompileDirectoryScanDependencyByIndex, this, m_compileDirectoryScanDependencies.Count());
	}

	Result DependencyNode::FindOrAddProduct(Vector<FileStatus> &products, BuildFileLocation location, const CIPathView &path, size_t &outIndex)
	{
		for (size_t i = 0; i < products.Count(); i++)
		{
			const FileStatus &fileStatus = products[i];
			if (fileStatus.m_location == location && fileStatus.m_filePath == path)
			{
				outIndex = i;
				return ResultCode::kOK;
			}
		}

		FileStatus newStatus;
		RKIT_CHECK(newStatus.m_filePath.Set(path));
		newStatus.m_location = location;

		outIndex = products.Count();
		RKIT_CHECK(products.Append(std::move(newStatus)));

		return ResultCode::kOK;
	}

	Result DependencyNode::FindOrAddAnalysisProduct(BuildFileLocation location, const CIPathView &path, size_t &outIndex)
	{
		RKIT_ASSERT(path.Length() != 0);
		return FindOrAddProduct(m_analysisProducts, location, path, outIndex);
	}

	Result DependencyNode::FindOrAddCompileProduct(BuildFileLocation location, const CIPathView &path, size_t &outIndex)
	{
		RKIT_ASSERT(path.Length() != 0);
		return FindOrAddProduct(m_compileProducts, location, path, outIndex);
	}

	Result DependencyNode::AddCASProduct(const data::ContentID &contentID)
	{
		for (const data::ContentID &casContentID : m_casProducts)
		{
			if (casContentID == contentID)
				return ResultCode::kOK;
		}

		return m_casProducts.Append(contentID);
	}

	Result DependencyNode::AddAnalysisFileDependency(const FileDependencyInfoView &fileInfo)
	{
		return AddFileDependency(m_analysisFileDependencies, fileInfo);
	}

	Result DependencyNode::AddCompileFileDependency(const FileDependencyInfoView &fileInfo)
	{
		return AddFileDependency(m_compileFileDependencies, fileInfo);
	}

	Result DependencyNode::AddAnalysisDirectoryScanDependency(const DirectoryScanDependencyInfoView &dirScanInfo)
	{
		return AddDirectoryScanDependency(m_analysisDirectoryScanDependencies, dirScanInfo);
	}

	Result DependencyNode::AddCompileDirectoryScanDependency(const DirectoryScanDependencyInfoView &dirScanInfo)
	{
		return AddDirectoryScanDependency(m_compileDirectoryScanDependencies, dirScanInfo);
	}

	Result DependencyNode::AddFileDependency(Vector<FileDependencyInfo> &fileDependencies, const FileDependencyInfoView &fileInfo)
	{
		for (FileDependencyInfo &existingFDI : fileDependencies)
		{
			if (existingFDI.m_status.m_location == fileInfo.m_status.m_location && existingFDI.m_status.m_filePath == fileInfo.m_status.m_filePath)
			{
				RKIT_CHECK(existingFDI.Set(fileInfo));
				return ResultCode::kOK;
			}
		}

		FileDependencyInfo fdi;
		RKIT_CHECK(fdi.Set(fileInfo));

		RKIT_CHECK(fileDependencies.Append(std::move(fdi)));

		return ResultCode::kOK;
	}

	Result DependencyNode::AddDirectoryScanDependency(Vector<DirectoryScanDependencyInfo> &dirScanDependencies, const DirectoryScanDependencyInfoView &dirScanInfo)
	{
		RKIT_ASSERT(dirScanInfo.m_dirScan.m_directoryPath.Length() != 0);

		for (DirectoryScanDependencyInfo &existingDSDI : dirScanDependencies)
		{
			if (existingDSDI.m_dirScan.m_directoryLocation == dirScanInfo.m_dirScan.m_directoryLocation
				&& existingDSDI.m_dirScan.m_directoryPath == dirScanInfo.m_dirScan.m_directoryPath
				&& existingDSDI.m_dirScan.m_directoryMode == dirScanInfo.m_dirScan.m_directoryMode)
			{
				RKIT_CHECK(existingDSDI.Set(dirScanInfo));
				return ResultCode::kOK;
			}
		}

		DirectoryScanDependencyInfo dsdi;
		RKIT_CHECK(dsdi.Set(dirScanInfo));

		RKIT_CHECK(dirScanDependencies.Append(std::move(dsdi)));

		return ResultCode::kOK;
	}

	template<class T>
	Result serializer::SerializeVector(IWriteStream &stream, StringPoolBuilder &stringPool, const Vector<T> &vec)
	{
		size_t count = vec.Count();

		RKIT_CHECK(serializer::SerializeCompactSize(stream, count));

		for (const T &item : vec)
		{
			RKIT_CHECK(Serialize(stream, stringPool, item));
		}

		return ResultCode::kOK;
	}

	template<class T>
	static Result serializer::SerializeEnum(IWriteStream &stream, const T &value)
	{
		return SerializeCompactSize(stream, static_cast<size_t>(value));
	}

	Result serializer::SerializeString(IWriteStream &stream, StringPoolBuilder &stringPool, const StringView &str)
	{
		size_t index = 0;
		RKIT_CHECK(stringPool.IndexString(str, index));

		return SerializeCompactSize(stream, index);
	}

	Result serializer::SerializeCIPath(IWriteStream &stream, StringPoolBuilder &stringPool, const CIPathView &path)
	{
		return SerializeString(stream, stringPool, path.ToStringView());
	}

	Result serializer::Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, const data::ContentID &contentID)
	{
		return stream.WriteAll(contentID.m_data, sizeof(contentID.m_data));
	}

	Result serializer::Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, const CIPath &path)
	{
		return SerializeCIPath(stream, stringPool, path);
	}

	Result serializer::Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, const FileStatus &fs)
	{
		RKIT_CHECK(SerializeCIPath(stream, stringPool, fs.m_filePath));
		RKIT_CHECK(SerializeEnum(stream, fs.m_location));
		RKIT_CHECK(stream.WriteAll(&fs.m_fileSize, sizeof(fs.m_fileSize)));
		RKIT_CHECK(stream.WriteAll(&fs.m_fileTime, sizeof(fs.m_fileTime)));

		return ResultCode::kOK;
	}

	Result serializer::Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, const DirectoryScan &ds)
	{
		RKIT_CHECK(SerializeVector(stream, stringPool, ds.m_paths));
		RKIT_CHECK(SerializeCIPath(stream, stringPool, ds.m_directoryPath));
		RKIT_CHECK(SerializeEnum(stream, ds.m_directoryLocation));
		RKIT_CHECK(Serialize(stream, stringPool, ds.m_directoryMode));

		return ResultCode::kOK;
	}

	Result serializer::Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, const FileDependencyInfo &fdi)
	{
		RKIT_CHECK(Serialize(stream, stringPool, fdi.m_status));
		RKIT_CHECK(Serialize(stream, stringPool, fdi.m_fileExists));
		RKIT_CHECK(Serialize(stream, stringPool, fdi.m_mustBeUpToDate));

		return ResultCode::kOK;
	}

	Result serializer::Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, const DirectoryScanDependencyInfo &dsdi)
	{
		RKIT_CHECK(Serialize(stream, stringPool, dsdi.m_dirScan));
		RKIT_CHECK(Serialize(stream, stringPool, dsdi.m_dirExists));
		RKIT_CHECK(Serialize(stream, stringPool, dsdi.m_mustBeUpToDate));

		return ResultCode::kOK;
	}

	Result serializer::Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, const NodeDependencyInfo &ndi)
	{
		RKIT_CHECK(SerializeCompactSize(stream, static_cast<DependencyNode *>(ndi.m_node)->GetSerializedIndex()));
		RKIT_CHECK(Serialize(stream, stringPool, ndi.m_mustBeUpToDate));

		return ResultCode::kOK;
	}

	Result serializer::Serialize(IWriteStream &stream, StringPoolBuilder &stringPool, bool value)
	{
		uint8_t valueByte = value ? 1 : 0;
		return stream.WriteAll(&valueByte, 1);
	}

	Result serializer::SerializeCompactSize(IWriteStream &stream, size_t size)
	{
		uint64_t index64 = size;

		if (index64 > 0x3fffffffffffffffu)
			return ResultCode::kOutOfMemory;

		uint8_t bytes[8];
		bytes[0] = static_cast<uint8_t>((index64 << 2) & 0xffu);

		for (int i = 1; i < 8; i++)
			bytes[i] = static_cast<uint8_t>((index64 >> (i * 8 - 2)) & 0xffu);

		size_t length = 8;
		if (bytes[7] != 0 || bytes[6] != 0 || bytes[5] != 0 || bytes[4] != 0)
			bytes[0] |= 3;
		else if (bytes[3] != 0 || bytes[2] != 0)
		{
			bytes[0] |= 2;
			length = 4;
		}
		else if (bytes[1] != 0)
		{
			bytes[0] |= 1;
			length = 2;
		}
		else
			length = 1;

		return stream.WriteAll(bytes, length);
	}

	serializer::DeserializeResolver::DeserializeResolver(const Span<const UniquePtr<DependencyNode>> &nodes, const Span<const String> &strings)
		: m_nodes(nodes)
		, m_strings(strings)
	{
	}

	Result serializer::DeserializeResolver::GetString(size_t index, StringView &outString) const
	{
		if (index >= m_strings.Count())
			return ResultCode::kInvalidParameter;

		outString = m_strings[index];
		return ResultCode::kOK;
	}

	Result serializer::DeserializeResolver::GetDependencyNode(size_t index, IDependencyNode *&outNode) const
	{
		if (index >= m_nodes.Count())
			return ResultCode::kInvalidParameter;

		outNode = m_nodes[index].Get();
		return ResultCode::kOK;
	}


	template<class T>
	Result serializer::DeserializeVector(IReadStream &stream, const IDeserializeResolver &resolver, Vector<T> &vec)
	{
		size_t sz = 0;
		RKIT_CHECK(DeserializeCompactSize(stream, sz));

		RKIT_CHECK(vec.Resize(sz));
		for (T &item : vec)
		{
			RKIT_CHECK(Deserialize(stream, resolver, item));
		}

		return ResultCode::kOK;
	}

	template<class T>
	Result serializer::DeserializeEnum(IReadStream &stream, T &value)
	{
		size_t szValue = 0;
		RKIT_CHECK(DeserializeCompactSize(stream, szValue));

		value = static_cast<T>(szValue);
		return ResultCode::kOK;
	}

	Result serializer::DeserializeString(IReadStream &stream, const IDeserializeResolver &resolver, String &str)
	{
		size_t index = 0;
		RKIT_CHECK(DeserializeCompactSize(stream, index));

		StringView strView;
		RKIT_CHECK(resolver.GetString(index, strView));

		RKIT_CHECK(str.Set(strView));

		return ResultCode::kOK;
	}

	Result serializer::DeserializeCIPath(IReadStream &stream, const IDeserializeResolver &resolver, CIPath &path)
	{
		String str;
		RKIT_CHECK(DeserializeString(stream, resolver, str));

		RKIT_CHECK(path.Set(str));

		return ResultCode::kOK;
	}

	Result serializer::Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, data::ContentID &contentID)
	{
		return stream.ReadAll(contentID.m_data, sizeof(contentID.m_data));
	}

	Result serializer::Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, CIPath &path)
	{
		return DeserializeCIPath(stream, resolver, path);
	}

	Result serializer::Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, FileStatus &fs)
	{
		RKIT_CHECK(DeserializeCIPath(stream, resolver, fs.m_filePath));
		RKIT_CHECK(DeserializeEnum(stream, fs.m_location));
		RKIT_CHECK(stream.ReadAll(&fs.m_fileSize, sizeof(fs.m_fileSize)));
		RKIT_CHECK(stream.ReadAll(&fs.m_fileTime, sizeof(fs.m_fileTime)));

		return ResultCode::kOK;
	}

	Result serializer::Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, DirectoryScan &ds)
	{
		RKIT_CHECK(DeserializeVector(stream, resolver, ds.m_paths));
		RKIT_CHECK(DeserializeCIPath(stream, resolver, ds.m_directoryPath));
		RKIT_CHECK(DeserializeEnum(stream, ds.m_directoryLocation));
		RKIT_CHECK(Deserialize(stream, resolver, ds.m_directoryMode));

		return ResultCode::kOK;
	}

	Result serializer::Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, FileDependencyInfo &fdi)
	{
		RKIT_CHECK(Deserialize(stream, resolver, fdi.m_status));
		RKIT_CHECK(Deserialize(stream, resolver, fdi.m_fileExists));
		RKIT_CHECK(Deserialize(stream, resolver, fdi.m_mustBeUpToDate));

		return ResultCode::kOK;
	}

	Result serializer::Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, DirectoryScanDependencyInfo &dsdi)
	{
		RKIT_CHECK(Deserialize(stream, resolver, dsdi.m_dirScan));
		RKIT_CHECK(Deserialize(stream, resolver, dsdi.m_dirExists));
		RKIT_CHECK(Deserialize(stream, resolver, dsdi.m_mustBeUpToDate));

		return ResultCode::kOK;
	}

	Result serializer::Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, NodeDependencyInfo &ndi)
	{
		size_t nodeIndex = 0;
		RKIT_CHECK(DeserializeCompactSize(stream, nodeIndex));

		IDependencyNode *node = nullptr;
		RKIT_CHECK(resolver.GetDependencyNode(nodeIndex, ndi.m_node));

		RKIT_CHECK(Deserialize(stream, resolver, ndi.m_mustBeUpToDate));

		return ResultCode::kOK;
	}

	Result serializer::Deserialize(IReadStream &stream, const IDeserializeResolver &resolver, bool &value)
	{
		uint8_t valueByte = 0;
		RKIT_CHECK(stream.ReadAll(&valueByte, 1));

		value = (valueByte != 0);
		return ResultCode::kOK;
	}

	Result serializer::DeserializeCompactSize(IReadStream &stream, size_t &sz)
	{
		uint8_t bytes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

		RKIT_CHECK(stream.ReadAll(bytes, 1));

		switch (bytes[0] & 3)
		{
		case 0:
			break;
		case 1:
			RKIT_CHECK(stream.ReadAll(bytes + 1, 1));
			break;
		case 2:
			RKIT_CHECK(stream.ReadAll(bytes + 1, 3));
			break;
		case 3:
			RKIT_CHECK(stream.ReadAll(bytes + 1, 7));
			break;
		default:
			return ResultCode::kInternalError;
		};

		uint64_t u64 = static_cast<uint64_t>((bytes[0] >> 2) & 0x3f);
		for (int i = 1; i < 8; i++)
			u64 |= static_cast<uint64_t>(static_cast<uint64_t>(bytes[i]) << (i * 8 - 2));

		if (u64 > std::numeric_limits<size_t>::max())
			return ResultCode::kIntegerOverflow;

		sz = static_cast<size_t>(u64);
		return ResultCode::kOK;
	}

	Result DependencyNode::AddNodeDependency(const NodeDependencyInfo &nodeInfo)
	{
		for (const NodeDependencyInfo &existingInfo : m_nodeDependencies)
		{
			if (nodeInfo.m_node == existingInfo.m_node)
				return ResultCode::kOK;
		}

		RKIT_CHECK(m_nodeDependencies.Append(nodeInfo));

		return ResultCode::kOK;
	}

	Result DependencyNode::SerializeInitialState(IWriteStream &stream) const
	{
		RKIT_CHECK(stream.WriteAll(&m_nodeNamespace, sizeof(m_nodeNamespace)));
		RKIT_CHECK(stream.WriteAll(&m_nodeType, sizeof(m_nodeType)));

		const uint64_t contentSize = m_content.Count();
		RKIT_CHECK(stream.WriteAll(&contentSize, sizeof(contentSize)));

		if (contentSize > 0)
		{
			RKIT_CHECK(stream.WriteAll(m_content.GetBuffer(), m_content.Count()));
		}

		RKIT_CHECK(serializer::SerializeEnum(stream, m_inputLocation));

		return ResultCode::kOK;
	}

	Result DependencyNode::DeserializeInitialState(IReadStream &stream, uint32_t &outNodeNamespace, uint32_t &outNodeType, Vector<uint8_t> &outContent, BuildFileLocation &outInputLocation)
	{
		RKIT_CHECK(stream.ReadAll(&outNodeNamespace, sizeof(outNodeNamespace)));
		RKIT_CHECK(stream.ReadAll(&outNodeType, sizeof(outNodeType)));


		uint64_t contentSize = 0;
		RKIT_CHECK(stream.ReadAll(&contentSize, sizeof(contentSize)));

		if (contentSize > std::numeric_limits<size_t>::max())
			return ResultCode::kOutOfMemory;

		RKIT_CHECK(outContent.Resize(static_cast<size_t>(contentSize)));

		if (contentSize > 0)
		{
			RKIT_CHECK(stream.ReadAll(outContent.GetBuffer(), outContent.Count()));
		}

		RKIT_CHECK(serializer::DeserializeEnum(stream, outInputLocation));

		return ResultCode::kOK;
	}

	Result DependencyNode::Serialize(IWriteStream &stream, StringPoolBuilder &stringPool) const
	{
		// Match with Deserialize
		RKIT_CHECK(serializer::SerializeEnum(stream, m_dependencyState));

		RKIT_CHECK(serializer::SerializeVector(stream, stringPool, m_analysisProducts));
		RKIT_CHECK(serializer::SerializeVector(stream, stringPool, m_compileProducts));
		RKIT_CHECK(serializer::SerializeVector(stream, stringPool, m_casProducts));
		RKIT_CHECK(serializer::SerializeVector(stream, stringPool, m_analysisFileDependencies));
		RKIT_CHECK(serializer::SerializeVector(stream, stringPool, m_compileFileDependencies));
		RKIT_CHECK(serializer::SerializeVector(stream, stringPool, m_analysisDirectoryScanDependencies));
		RKIT_CHECK(serializer::SerializeVector(stream, stringPool, m_compileDirectoryScanDependencies));
		RKIT_CHECK(serializer::SerializeVector(stream, stringPool, m_nodeDependencies));

		RKIT_CHECK(serializer::SerializeString(stream, stringPool, m_identifier));

		RKIT_CHECK(stream.WriteAll(&m_lastCompilerVersion, sizeof(m_lastCompilerVersion)));

		// Intentionally not serialized: compiler, m_isMarkedAsRoot, m_checkNodeIndex, m_depCheckPhase, m_serializedIndex
		return ResultCode::kOK;
	}

	Result DependencyNode::Deserialize(IReadStream &stream, const IDeserializeResolver &resolver)
	{
		// Match with Serialize
		RKIT_CHECK(serializer::DeserializeEnum(stream, m_dependencyState));

		RKIT_CHECK(serializer::DeserializeVector(stream, resolver, m_analysisProducts));
		RKIT_CHECK(serializer::DeserializeVector(stream, resolver, m_compileProducts));
		RKIT_CHECK(serializer::DeserializeVector(stream, resolver, m_casProducts));
		RKIT_CHECK(serializer::DeserializeVector(stream, resolver, m_analysisFileDependencies));
		RKIT_CHECK(serializer::DeserializeVector(stream, resolver, m_compileFileDependencies));
		RKIT_CHECK(serializer::DeserializeVector(stream, resolver, m_analysisDirectoryScanDependencies));
		RKIT_CHECK(serializer::DeserializeVector(stream, resolver, m_compileDirectoryScanDependencies));
		RKIT_CHECK(serializer::DeserializeVector(stream, resolver, m_nodeDependencies));

		RKIT_CHECK(serializer::DeserializeString(stream, resolver, m_identifier));

		RKIT_CHECK(stream.ReadAll(&m_lastCompilerVersion, sizeof(m_lastCompilerVersion)));

		// Intentionally not serialized: compiler, m_isMarkedAsRoot, m_checkNodeIndex, m_depCheckPhase, m_serializedIndex
		return ResultCode::kOK;
	}

	Result DependencyNode::MarkProductFinished(bool isCompilePhase, size_t productIndex, const FileStatusView &fstatus)
	{
		Vector<FileStatus> &products = (isCompilePhase ? m_compileProducts : m_analysisProducts);
		return products[productIndex].Set(fstatus);
	}

	void DependencyNode::SetSerializedIndex(size_t index)
	{
		m_serializedIndex = index;
	}

	size_t DependencyNode::GetSerializedIndex() const
	{
		return m_serializedIndex;
	}

	DependencyNode::DependencyNodeCompilerFeedback::DependencyNodeCompilerFeedback(BuildSystemInstance *instance, DependencyNode *node, bool isCompilePhase)
		: m_buildInstance(instance)
		, m_dependencyNode(node)
		, m_isCompilePhase(isCompilePhase)
	{
	}


	Result DependencyNode::DependencyNodeCompilerFeedback::CheckInputExists(BuildFileLocation location, const CIPathView &path, bool &outExists)
	{
		CallbackSpan<FileDependencyInfoView, const IDependencyNode *> depsSpan = m_isCompilePhase ? m_dependencyNode->GetCompileFileDependencies() : m_dependencyNode->GetAnalysisFileDependencies();

		for (FileDependencyInfoView fStatus : depsSpan)
		{
			if (fStatus.m_status.m_location == location && fStatus.m_status.m_filePath == path)
			{
				outExists = fStatus.m_fileExists;
				return ResultCode::kOK;
			}
		}

		FileStatusView newFStatusView;
		bool exists = false;
		RKIT_CHECK(m_buildInstance->ResolveFileStatus(location, path, false, newFStatusView, true, exists));

		// If these don't exist, then the file status view will be empty
		if (!exists)
		{
			newFStatusView.m_filePath = path;
			newFStatusView.m_location = location;
		}

		FileDependencyInfoView newDepInfo;
		newDepInfo.m_status = newFStatusView;
		newDepInfo.m_fileExists = exists;
		newDepInfo.m_mustBeUpToDate = true;

		if (m_isCompilePhase)
		{
			RKIT_CHECK(m_dependencyNode->AddCompileFileDependency(newDepInfo));
		}
		else
		{
			RKIT_CHECK(m_dependencyNode->AddAnalysisFileDependency(newDepInfo));
		}

		outExists = exists;

		return ResultCode::kOK;
	}

	Result DependencyNode::DependencyNodeCompilerFeedback::OpenInput(BuildFileLocation location, const CIPathView &path, UniquePtr<ISeekableReadStream> &inputFile)
	{
		RKIT_CHECK(TryOpenInput(location, path, inputFile));
		if (!inputFile.IsValid())
			return ResultCode::kFileOpenError;

		return ResultCode::kOK;
	}

	Result DependencyNode::DependencyNodeCompilerFeedback::TryOpenInput(BuildFileLocation location, const CIPathView &path, UniquePtr<ISeekableReadStream> &inputFile)
	{
		inputFile.Reset();

		CallbackSpan<FileDependencyInfoView, const IDependencyNode *> depsSpan = m_isCompilePhase ? m_dependencyNode->GetCompileFileDependencies() : m_dependencyNode->GetAnalysisFileDependencies();

		for (FileDependencyInfoView fStatus : depsSpan)
		{
			if (fStatus.m_status.m_location == location && fStatus.m_status.m_filePath == path)
			{
				if (!fStatus.m_fileExists)
					return ResultCode::kOK;
			}
		}

		FileStatusView newFStatusView;
		bool exists = false;
		RKIT_CHECK(m_buildInstance->ResolveFileStatus(location, path, false, newFStatusView, true, exists));

		FileDependencyInfoView newDepInfo;
		newDepInfo.m_status = newFStatusView;
		newDepInfo.m_fileExists = exists;
		newDepInfo.m_mustBeUpToDate = true;

		if (m_isCompilePhase)
		{
			RKIT_CHECK(m_dependencyNode->AddCompileFileDependency(newDepInfo));
		}
		else
		{
			RKIT_CHECK(m_dependencyNode->AddAnalysisFileDependency(newDepInfo));
		}

		RKIT_CHECK(m_buildInstance->TryOpenFileRead(location, path, inputFile));

		return ResultCode::kOK;
	}

	Result DependencyNode::DependencyNodeCompilerFeedback::OpenOutput(BuildFileLocation location, const CIPathView &path, UniquePtr<ISeekableReadWriteStream> &outputFile)
	{
		size_t productIndex = 0;
		if (m_isCompilePhase)
		{
			RKIT_CHECK(m_dependencyNode->FindOrAddCompileProduct(location, path, productIndex));
		}
		else
		{
			RKIT_CHECK(m_dependencyNode->FindOrAddAnalysisProduct(location, path, productIndex));
		}

		UniquePtr<ISeekableReadWriteStream> realFile;
		RKIT_CHECK(m_buildInstance->OpenFileWrite(location, path, realFile));

		CIPath pathCopy;
		RKIT_CHECK(pathCopy.Set(path));

		RKIT_CHECK(New<FeedbackWrapperStream>(outputFile, *this, productIndex, location, std::move(pathCopy), std::move(realFile)));

		return ResultCode::kOK;
	}

	Result DependencyNode::DependencyNodeCompilerFeedback::AddAnonymousDeployableContent(BuildFileLocation location, const CIPathView &path)
	{
		data::ContentID contentID;
		RKIT_CHECK(IndexCAS(location, path, contentID));

		RKIT_CHECK(m_dependencyNode->AddCASProduct(contentID));

		RKIT_CHECK(m_buildInstance->RegisterCASSource(contentID, location, path));

		CIPath casPath;
		RKIT_CHECK(casPath.AppendComponent("cas"));
		RKIT_CHECK(casPath.Append(path));

		UniquePtr<ISeekableReadWriteStream> casIDFile;
		RKIT_CHECK(OpenOutput(location, casPath, casIDFile));

		RKIT_CHECK(casIDFile->WriteAll(contentID.m_data, data::ContentID::kSize));

		return ResultCode::kOK;
	}

	Result DependencyNode::DependencyNodeCompilerFeedback::IndexCAS(BuildFileLocation location, const CIPathView &path, data::ContentID &outContentID)
	{
		UniquePtr<ISeekableReadStream> inputFile;
		RKIT_CHECK(this->OpenInput(location, path, inputFile));

		const utils::ISha256Calculator *calculator = GetDrivers().m_utilitiesDriver->GetSha256Calculator();
		utils::Sha256StreamingState streamingState = calculator->CreateStreamingState();

		FilePos_t amountRemaining = inputFile->GetSize();
		uint8_t buffer[1024];

		while (amountRemaining > 0)
		{
			size_t amountToRead = sizeof(buffer);
			if (amountToRead > amountRemaining)
				amountToRead = static_cast<size_t>(amountRemaining);

			amountRemaining -= static_cast<FilePos_t>(amountToRead);

			RKIT_CHECK(inputFile->ReadAll(buffer, amountToRead));
			calculator->AppendStreamingState(streamingState, buffer, amountToRead);
		}

		calculator->FinalizeStreamingState(streamingState);

		utils::Sha256DigestBytes digest = calculator->FlushToBytes(streamingState.m_state);

		static_assert(sizeof(digest.m_data) == sizeof(outContentID.m_data));

		memcpy(outContentID.m_data, digest.m_data, sizeof(digest.m_data));

		RKIT_CHECK(m_buildInstance->RegisterCASSource(outContentID, location, path));

		RKIT_CHECK(m_dependencyNode->AddCASProduct(outContentID));

		return ResultCode::kOK;
	}

	Result DependencyNode::DependencyNodeCompilerFeedback::AddNodeDependency(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier)
	{
		IDependencyNode *node = nullptr;
		RKIT_CHECK(m_buildInstance->FindOrCreateNamedNode(nodeTypeNamespace, nodeTypeID, inputFileLocation, identifier, node));

		NodeDependencyInfo depInfo;
		depInfo.m_mustBeUpToDate = true;
		depInfo.m_node = node;
		RKIT_CHECK(m_dependencyNode->AddNodeDependency(depInfo));

		return ResultCode::kOK;
	}

	bool DependencyNode::DependencyNodeCompilerFeedback::FindNodeTypeByFileExtension(const StringSliceView &ext, uint32_t &outNamespace, uint32_t &outType) const
	{
		return m_buildInstance->FindNodeTypeByFileExtension(ext, outNamespace, outType);
	}

	Result DependencyNode::DependencyNodeCompilerFeedback::InternalEnumerateFilesOrDirectories(BuildFileLocation location, const CIPathView &path, bool directoryMode, void *userdata, EnumerateFilesResultCallback_t resultCallback)
	{
		DirectoryScanDependencyInfoView newDepInfo;

		RKIT_CHECK(m_buildInstance->ResolveDirectoryScan(location, path, directoryMode, newDepInfo.m_dirScan, true, newDepInfo.m_dirExists));

		if (m_isCompilePhase)
		{
			RKIT_CHECK(m_dependencyNode->AddCompileDirectoryScanDependency(newDepInfo));
		}
		else
		{
			RKIT_CHECK(m_dependencyNode->AddAnalysisDirectoryScanDependency(newDepInfo));
		}

		if (newDepInfo.m_dirExists)
		{
			for (const CIPathView &pathView : newDepInfo.m_dirScan.m_paths)
			{
				RKIT_CHECK(resultCallback(userdata, pathView));
			}
		}		

		return ResultCode::kOK;
	}

	Result DependencyNode::DependencyNodeCompilerFeedback::EnumerateFiles(BuildFileLocation location, const CIPathView &path, void *userdata, EnumerateFilesResultCallback_t resultCallback)
	{
		return InternalEnumerateFilesOrDirectories(location, path, false, userdata, resultCallback);
	}

	Result DependencyNode::DependencyNodeCompilerFeedback::EnumerateDirectories(BuildFileLocation location, const CIPathView &path, void *userdata, EnumerateFilesResultCallback_t resultCallback)
	{
		return InternalEnumerateFilesOrDirectories(location, path, true, userdata, resultCallback);
	}

	IBuildSystemInstance *DependencyNode::DependencyNodeCompilerFeedback::GetBuildSystemInstance() const
	{
		return m_buildInstance;
	}

	void DependencyNode::DependencyNodeCompilerFeedback::MarkOutputFileFinished(size_t productIndex, BuildFileLocation location, const CIPathView &path)
	{
		Result result = CheckedMarkOutputFileFinished(productIndex, location, path);

		if (!result.IsOK())
			m_fault = result;
	}

	Result DependencyNode::DependencyNodeCompilerFeedback::CheckFault() const
	{
		return m_fault;
	}

	Result DependencyNode::DependencyNodeCompilerFeedback::CheckedMarkOutputFileFinished(size_t productIndex, BuildFileLocation location, const CIPathView &path)
	{
		FileStatusView fileStatusView;
		bool exists = false;
		RKIT_CHECK(m_buildInstance->ResolveFileStatus(location, path, false, fileStatusView, false, exists));

		if (!exists)
		{
			rkit::log::Error("A problem occurred refreshing the file status of an output");
			return ResultCode::kFileOpenError;
		}

		RKIT_CHECK(m_dependencyNode->MarkProductFinished(m_isCompilePhase, productIndex, fileStatusView));

		return ResultCode::kOK;
	}

	DependencyNode::FeedbackWrapperStream::FeedbackWrapperStream(DependencyNodeCompilerFeedback &feedback, size_t productIndex, BuildFileLocation location, CIPath &&path, UniquePtr<ISeekableReadWriteStream> &&stream)
		: m_feedback(feedback)
		, m_productIndex(productIndex)
		, m_location(location)
		, m_path(std::move(path))
		, m_stream(std::move(stream))
	{
	}

	DependencyNode::FeedbackWrapperStream::~FeedbackWrapperStream()
	{
		m_stream.Reset();

		m_feedback.MarkOutputFileFinished(m_productIndex, m_location, m_path);
	}

	Result DependencyNode::FeedbackWrapperStream::ReadPartial(void *data, size_t count, size_t &outCountRead)
	{
		return m_stream->ReadPartial(data, count, outCountRead);
	}

	Result DependencyNode::FeedbackWrapperStream::WritePartial(const void *data, size_t count, size_t &outCountWritten)
	{
		return m_stream->WritePartial(data, count, outCountWritten);
	}

	Result DependencyNode::FeedbackWrapperStream::Flush()
	{
		return m_stream->Flush();
	}

	Result DependencyNode::FeedbackWrapperStream::SeekStart(FilePos_t pos)
	{
		return m_stream->SeekStart(pos);
	}

	Result DependencyNode::FeedbackWrapperStream::SeekCurrent(FileOffset_t pos)
	{
		return m_stream->SeekCurrent(pos);
	}

	Result DependencyNode::FeedbackWrapperStream::SeekEnd(FileOffset_t pos)
	{
		return m_stream->SeekEnd(pos);
	}

	FilePos_t DependencyNode::FeedbackWrapperStream::Tell() const
	{
		return m_stream->Tell();
	}

	FilePos_t DependencyNode::FeedbackWrapperStream::GetSize() const
	{
		return m_stream->GetSize();
	}

	Result DependencyNode::FeedbackWrapperStream::Truncate(FilePos_t newSize)
	{
		return m_stream->Truncate(newSize);
	}

	FileStatusView DependencyNode::GetAnalysisProductByIndex(const IDependencyNode *const & node, size_t index)
	{
		const FileStatus &fileStatus = static_cast<const DependencyNode *>(node)->m_analysisProducts[index];

		return fileStatus.ToView();
	}

	FileStatusView DependencyNode::GetCompileProductByIndex(const IDependencyNode *const &node, size_t index)
	{
		const FileStatus &fileStatus = static_cast<const DependencyNode *>(node)->m_compileProducts[index];

		return fileStatus.ToView();
	}

	data::ContentID DependencyNode::GetCompileCASProductByIndex(const IDependencyNode *const &node, size_t index)
	{
		return static_cast<const DependencyNode *>(node)->m_casProducts[index];
	}

	FileDependencyInfoView DependencyNode::GetAnalysisFileDependencyByIndex(const IDependencyNode *const &node, size_t index)
	{
		const FileDependencyInfo &fileDependencyInfo = static_cast<const DependencyNode *>(node)->m_analysisFileDependencies[index];

		return fileDependencyInfo.ToView();
	}

	FileDependencyInfoView DependencyNode::GetCompileFileDependencyByIndex(const IDependencyNode *const &node, size_t index)
	{
		const FileDependencyInfo &fileDependencyInfo = static_cast<const DependencyNode *>(node)->m_compileFileDependencies[index];

		return fileDependencyInfo.ToView();
	}

	DirectoryScanDependencyInfoView DependencyNode::GetAnalysisDirectoryScanDependencyByIndex(const IDependencyNode *const &node, size_t index)
	{
		const DirectoryScanDependencyInfo &dirScan = static_cast<const DependencyNode *>(node)->m_analysisDirectoryScanDependencies[index];

		return dirScan.ToView();
	}

	DirectoryScanDependencyInfoView DependencyNode::GetCompileDirectoryScanDependencyByIndex(const IDependencyNode *const &node, size_t index)
	{
		const DirectoryScanDependencyInfo &dirScan = static_cast<const DependencyNode *>(node)->m_compileDirectoryScanDependencies[index];

		return dirScan.ToView();
	}

	NodeDependencyInfo DependencyNode::GetNodeDependencyByIndex(const IDependencyNode *const &node, size_t index)
	{
		return static_cast<const DependencyNode *>(node)->m_nodeDependencies[index];
	}

	BuildSystemInstance::BuildSystemInstance()
		: m_fs(nullptr)
	{
	}

	Result IBaseBuildSystemInstance::Create(UniquePtr<IBuildSystemInstance> &outInstance)
	{
		return rkit::New<BuildSystemInstance>(outInstance);
	}

	Result BuildSystemInstance::Initialize(const rkit::StringView &targetName, const OSAbsPathView &srcDir, const OSAbsPathView &intermediateDir, const OSAbsPathView &dataDir)
	{
		RKIT_CHECK(m_targetName.Set(targetName));
		RKIT_CHECK(m_srcDir.Set(srcDir));
		RKIT_CHECK(m_intermedDir.Set(intermediateDir));
		RKIT_CHECK(m_dataDir.Set(dataDir));

		UniquePtr<IDependencyNodeCompiler> depsCompiler;
		RKIT_CHECK(New<DepsNodeCompiler>(depsCompiler));

		UniquePtr<IDependencyNodeCompiler> pipelineLibraryCompiler;
		RKIT_CHECK(New<RenderPipelineLibraryCompiler>(pipelineLibraryCompiler));

		RKIT_CHECK(RegisterNodeCompiler(kDefaultNamespace, kDepsNodeID, std::move(depsCompiler)));
		RKIT_CHECK(RegisterNodeCompiler(kDefaultNamespace, kRenderPipelineLibraryNodeID, std::move(pipelineLibraryCompiler)));

		RKIT_CHECK(RegisterNodeTypeByExtension("deps", kDefaultNamespace, kDepsNodeID));
		RKIT_CHECK(RegisterNodeTypeByExtension("rkp", kDefaultNamespace, kRenderPipelineLibraryNodeID));

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::CheckedLoadCache(ISeekableReadStream &seekableStream, FilePos_t pos)
	{
		RKIT_CHECK(seekableStream.SeekStart(pos));

		IReadStream &stream = seekableStream;

		Vector<UniquePtr<DependencyNode>> nodesVector;
		HashMap<NodeKey, DependencyNode *> nodeLookup;

		Vector<String> stringsVector;

		size_t numStrings = 0;
		RKIT_CHECK(serializer::DeserializeCompactSize(stream, numStrings));

		RKIT_CHECK(stringsVector.Resize(numStrings));

		Span<const String> strings = stringsVector.ToSpan();

		for (size_t i = 0; i < numStrings; i++)
		{
			size_t strLength = 0;
			RKIT_CHECK(serializer::DeserializeCompactSize(stream, strLength));

			StringConstructionBuffer strBuf;
			RKIT_CHECK(strBuf.Allocate(strLength));

			Span<char> chars = strBuf.GetSpan();
			RKIT_CHECK(stream.ReadAll(chars.Ptr(), chars.Count()));

			stringsVector[i] = String(std::move(strBuf));
		}

		size_t numNodes = 0;
		RKIT_CHECK(serializer::DeserializeCompactSize(stream, numNodes));

		RKIT_CHECK(nodesVector.Resize(numNodes));

		Span<const UniquePtr<DependencyNode>> nodes = nodesVector.ToSpan();

		for (size_t i = 0; i < numNodes; i++)
		{
			uint32_t nodeNamespace = 0;
			uint32_t nodeType = 0;
			BuildFileLocation inputLocation = BuildFileLocation::kInvalid;
			Vector<uint8_t> content;

			RKIT_CHECK(DependencyNode::DeserializeInitialState(stream, nodeNamespace, nodeType, content, inputLocation));

			HashMap<NodeTypeKey, UniquePtr<IDependencyNodeCompiler>>::ConstIterator_t it = m_nodeCompilers.Find(NodeTypeKey(nodeNamespace, nodeType));
			if (it == m_nodeCompilers.end())
				return ResultCode::kOperationFailed;

			IDependencyNodeCompiler *compiler = it.Value().Get();

			RKIT_CHECK(New<DependencyNode>(nodesVector[i], compiler, nodeNamespace, nodeType, std::move(content), inputLocation));
		}

		serializer::DeserializeResolver resolver(nodes, strings);
		for (size_t i = 0; i < numNodes; i++)
		{
			RKIT_CHECK(nodes[i]->Deserialize(stream, resolver));
		}

		for (size_t i = 0; i < numNodes; i++)
		{
			DependencyNode *node = nodes[i].Get();

			NodeKey nodeKey(NodeTypeKey(node->GetDependencyNodeNamespace(), node->GetDependencyNodeType()), node->GetInputFileLocation(), node->GetIdentifier());

			RKIT_CHECK(nodeLookup.Set(nodeKey, node));
		}

		m_nodes = std::move(nodesVector);
		m_nodeLookup = std::move(nodeLookup);

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::LoadCache()
	{
		if (m_nodes.Count() != 0)
			return ResultCode::kOperationFailed;

		OSAbsPath cacheFullPath;
		RKIT_CHECK(ConstructIntermediatePath(cacheFullPath, GetCacheFileName()));

		ISystemDriver *sysDriver = GetDrivers().m_systemDriver;

		UniquePtr<ISeekableReadStream> graphStream;
		Result openResult = sysDriver->OpenFileReadAbs(graphStream, cacheFullPath);

		if (openResult.GetResultCode() == ResultCode::kFileOpenError)
			return ResultCode::kOK;

		RKIT_CHECK(openResult);

		BuildCacheFileHeader header;
		size_t countRead = 0;
		if (!graphStream->ReadPartial(&header, sizeof(header), countRead).IsOK() || countRead != sizeof(header))
			return ResultCode::kOK;

		if (header.m_identifier != BuildCacheFileHeader::kCacheIdentifier || header.m_version != BuildCacheFileHeader::kCacheVersion || header.m_activeInstance >= 2)
			return ResultCode::kOK;

		const BuildCacheInstanceInfo &cacheInstance = header.m_instances[header.m_activeInstance];

		if (!CheckedLoadCache(*graphStream, cacheInstance.m_filePos).IsOK())
		{
			m_nodeLookup.Clear();
			m_nodes.Reset();
		}

		return ResultCode::kOK;
	}

	IDependencyNode *BuildSystemInstance::FindNamedNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier) const
	{
		NodeTypeKey ntk(nodeTypeNamespace, nodeTypeID);
		NodeKey key(ntk, inputFileLocation, identifier);

		HashMap<NodeKey, DependencyNode *>::ConstIterator_t it = m_nodeLookup.Find(key);

		if (it == m_nodeLookup.end())
			return nullptr;

		return it.Value();
	}

	IDependencyNode *BuildSystemInstance::FindContentNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const Span<const uint8_t> &content) const
	{
		return FindNamedNode(nodeTypeNamespace, nodeTypeID, inputFileLocation, CreateContentID(content).GetStringView());
	}

	Result BuildSystemInstance::FindOrCreateContentNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, Vector<uint8_t> &&contentRef, IDependencyNode *&outNode)
	{
		Vector<uint8_t> content = std::move(contentRef);

		outNode = nullptr;

		ContentID contentID = CreateContentID(content.ToSpan());
		StringView contentIdentifier = contentID.GetStringView();

		{
			IDependencyNode *node = FindNamedNode(nodeTypeNamespace, nodeTypeID, inputFileLocation, contentIdentifier);

			if (node)
			{
				outNode = node;
				return ResultCode::kOK;
			}
		}

		{
			UniquePtr<IDependencyNode> node;
			RKIT_CHECK(CreateNode(nodeTypeNamespace, nodeTypeID, inputFileLocation, contentIdentifier, std::move(content), node));

			IDependencyNode *nodePtr = node.Get();

			RKIT_CHECK(AddNode(node.StaticCast<DependencyNode>()));

			outNode = nodePtr;
		}

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::FindOrCreateNamedNode(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier, IDependencyNode *&outNode)
	{
		outNode = nullptr;

		{
			IDependencyNode *node = FindNamedNode(nodeTypeNamespace, nodeTypeID, inputFileLocation, identifier);

			if (node)
			{
				outNode = node;
				return ResultCode::kOK;
			}
		}

		{
			UniquePtr<IDependencyNode> node;
			RKIT_CHECK(CreateNamedNode(nodeTypeNamespace, nodeTypeID, inputFileLocation, identifier, node));

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

	Result BuildSystemInstance::AddPostBuildAction(IBuildSystemAction *action)
	{
		return m_postBuildActions.Append(action);
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

		// May add more relevant nodes during this loop
		for (size_t i = 0; i < m_relevantNodes.Count(); i++)
		{
			DependencyNode *node = m_relevantNodes[i];

			RKIT_CHECK(m_depCheckStack.Append(node));

			while (m_depCheckStack.Count() > 0)
			{
				RKIT_CHECK(ProcessTopDepCheck());
			}
		}

		// Step 2: Compile (NOTE: relevant nodes list may be added during this)
		for (size_t ri = 0; ri < m_relevantNodes.Count(); ri++)
		{
			DependencyNode *node = m_relevantNodes[m_relevantNodes.Count() - 1 - ri];

			RKIT_ASSERT(node->GetDependencyState() != DependencyState::NotAnalyzedOrCompiled);

			if (node->GetDependencyState() == DependencyState::NotCompiled)
			{
				rkit::log::LogInfoFmt("Build Compile : %s %s %s", FourCCToPrintable(node->GetDependencyNodeNamespace()).GetChars(), FourCCToPrintable(node->GetDependencyNodeType()).GetChars(), node->GetIdentifier().GetChars());
				RKIT_CHECK(node->RunCompile(this));
				node->SetState(DependencyState::UpToDate);
			}
		}

		// Step 3: Run post-build actions
		for (size_t i = 0; i < m_postBuildActions.Count(); i++)
		{
			RKIT_CHECK(m_postBuildActions[i]->Run());
		}

		// Step 4: Copy CAS files
		for (size_t i = 0; i < m_relevantNodes.Count(); i++)
		{
			const DependencyNode *node = m_relevantNodes[i];

			for (const data::ContentID &contentID : node->GetCompileCASProducts())
			{
				RKIT_CHECK(UpdateCAS(contentID));
			}
		}

		// Step 5: Write updated state
		RKIT_CHECK(SaveCache());

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::SaveCache()
	{
		rkit::BufferStream depsGraphNodesStream;
		StringPoolBuilder stringPool;

		// Write nodes
		size_t serializedIndex = 0;
		for (DependencyNode *node : m_relevantNodes)
		{
			node->SetSerializedIndex(serializedIndex++);
		}

		RKIT_CHECK(serializer::SerializeCompactSize(depsGraphNodesStream, serializedIndex));
		for (DependencyNode *node : m_relevantNodes)
		{
			RKIT_CHECK(node->SerializeInitialState(depsGraphNodesStream));
		}

		for (DependencyNode *node : m_relevantNodes)
		{
			RKIT_CHECK(node->Serialize(depsGraphNodesStream, stringPool));
		}

		// Write strings
		rkit::BufferStream stringPoolStream;
		size_t numStrings = stringPool.NumStrings();

		RKIT_CHECK(serializer::SerializeCompactSize(stringPoolStream, numStrings));
		for (size_t i = 0; i < numStrings; i++)
		{
			StringView str = stringPool.GetStringByIndex(i);
			RKIT_CHECK(serializer::SerializeCompactSize(stringPoolStream, str.Length()));
			RKIT_CHECK(stringPoolStream.WriteAll(str.GetChars(), str.Length()));
		}

		FilePos_t stringPoolSize = stringPoolStream.GetSize();
		FilePos_t graphSize = depsGraphNodesStream.GetSize();

		FilePos_t combinedSize = 0;
		RKIT_CHECK(SafeAdd(combinedSize, stringPoolSize, graphSize));

		OSAbsPath cacheFullPath;
		RKIT_CHECK(ConstructIntermediatePath(cacheFullPath, GetCacheFileName()));

		ISystemDriver *sysDriver = GetDrivers().m_systemDriver;
		UniquePtr<ISeekableReadWriteStream> graphStream;
		RKIT_CHECK(sysDriver->OpenFileReadWriteAbs(graphStream, cacheFullPath, true, true, false));

		bool headerOK = false;

		BuildCacheFileHeader header;
		if (graphStream->GetSize() >= sizeof(header))
		{
			RKIT_CHECK(graphStream->ReadAll(&header, sizeof(header)));

			if (header.m_version == BuildCacheFileHeader::kCacheVersion && header.m_identifier == BuildCacheFileHeader::kCacheIdentifier && header.m_activeInstance < 2)
				headerOK = true;
		}

		if (!headerOK)
		{
			header = BuildCacheFileHeader();
			header.m_identifier = BuildCacheFileHeader::kCacheIdentifier;
			header.m_version = BuildCacheFileHeader::kCacheVersion;
			header.m_activeInstance = 1;

			RKIT_CHECK(graphStream->SeekStart(0));
			RKIT_CHECK(graphStream->WriteAll(&header, sizeof(header)));
			RKIT_CHECK(graphStream->Flush());
		}

		const BuildCacheInstanceInfo &oldInst = header.m_instances[header.m_activeInstance];
		BuildCacheInstanceInfo &newInst = header.m_instances[1 - header.m_activeInstance];

		bool canWriteBeforeOldInst = false;
		if (oldInst.m_filePos < sizeof(header) || (oldInst.m_filePos - sizeof(header)) >= combinedSize)
			canWriteBeforeOldInst = true;

		if (!canWriteBeforeOldInst)
		{
			RKIT_CHECK(graphStream->SeekEnd(0));
		}

		newInst.m_filePos = graphStream->Tell();
		newInst.m_size = combinedSize;

		Span<const uint8_t> stringPoolData = stringPoolStream.GetBuffer().ToSpan();
		RKIT_CHECK(graphStream->WriteAll(stringPoolData.Ptr(), stringPoolData.Count()));

		Span<const uint8_t> nodesData = depsGraphNodesStream.GetBuffer().ToSpan();
		RKIT_CHECK(graphStream->WriteAll(nodesData.Ptr(), nodesData.Count()));
		RKIT_CHECK(graphStream->Flush());

		header.m_activeInstance = 1 - header.m_activeInstance;

		RKIT_CHECK(graphStream->SeekStart(0));
		RKIT_CHECK(graphStream->WriteAll(&header, sizeof(header)));

		return ResultCode::kOK;
	}

	IDependencyGraphFactory *BuildSystemInstance::GetDependencyGraphFactory() const
	{
		return const_cast<BuildSystemInstance *>(this);
	}

	BuildSystemInstance::ContentID BuildSystemInstance::CreateContentID(const Span<const uint8_t> &content)
	{
		const utils::ISha256Calculator *calculator = GetDrivers().m_utilitiesDriver->GetSha256Calculator();
		utils::Sha256DigestBytes digest = calculator->SimpleHashBuffer(content.Ptr(), content.Count());

		const char *contentIDChars = "abcdefghijklmnopqrstuvwxyz012345";

		ContentID contentID;
		contentID.m_chars[ContentID::kContentStringSize] = '\0';

		uint8_t numBitsBuffered = 0;
		uint16_t bufferedNumber = 0;
		size_t hashRead = 0;
		for (size_t i = 0; i < ContentID::kContentStringSize; i++)
		{
			while (numBitsBuffered < 5)
			{
				RKIT_ASSERT(hashRead < utils::Sha256DigestBytes::kSize);
				bufferedNumber |= digest.m_data[hashRead++] << (numBitsBuffered);
				numBitsBuffered += 8;
			}

			contentID.m_chars[i] = contentIDChars[bufferedNumber % 32];
		}

		return contentID;
	}

	Result BuildSystemInstance::CreateNode(uint32_t nodeNamespace, uint32_t nodeType, BuildFileLocation buildFileLocation, const StringView &identifier, Vector<uint8_t> &&contentRef, UniquePtr<IDependencyNode> &outNode) const
	{
		Vector<uint8_t> content = std::move(contentRef);

		HashMap<NodeTypeKey, UniquePtr<IDependencyNodeCompiler> >::ConstIterator_t compilerIt = m_nodeCompilers.Find(NodeTypeKey(nodeNamespace, nodeType));

		if (compilerIt == m_nodeCompilers.end())
		{
			rkit::log::ErrorFmt("Unknown dependency node type %u / %u", static_cast<unsigned int>(nodeNamespace), static_cast<unsigned int>(nodeType));
			return ResultCode::kInvalidParameter;
		}

		UniquePtr<DependencyNode> depNode;
		RKIT_CHECK(New<DependencyNode>(depNode, compilerIt.Value().Get(), nodeNamespace, nodeType, std::move(content), buildFileLocation));

		RKIT_CHECK(depNode->Initialize(identifier));

		outNode = std::move(depNode);

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::CreateNamedNode(uint32_t nodeNamespace, uint32_t nodeType, BuildFileLocation buildFileLocation, const StringView &identifier, UniquePtr<IDependencyNode> &outNode) const
	{
		return CreateNode(nodeNamespace, nodeType, buildFileLocation, identifier, Vector<uint8_t>(), outNode);
	}

	Result BuildSystemInstance::CreateContentNode(uint32_t nodeNamespace, uint32_t nodeType, BuildFileLocation buildFileLocation, Vector<uint8_t> &&contentRef, UniquePtr<IDependencyNode> &outNode) const
	{
		Vector<uint8_t> content = std::move(contentRef);

		if (content.Count() == 0)
			return ResultCode::kInvalidParameter;

		return CreateNode(nodeNamespace, nodeType, buildFileLocation, CreateContentID(content.ToSpan()).GetStringView(), std::move(content), outNode);
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

		if (node->GetDependencyCheckPhase() == DependencyCheckPhase::Completed)
		{
			m_depCheckStack.RemoveRange(m_depCheckStack.Count() - 1, 1);
			return ResultCode::kOK;
		}

		if (node->GetDependencyCheckPhase() == DependencyCheckPhase::CheckFiles)
		{
			// This may lower the state to NotAnalyzedOrCompiled or NotCompiled
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
					rkit::log::LogInfoFmt("Build Analysis: %s %s %s", FourCCToPrintable(node->GetDependencyNodeNamespace()).GetChars(), FourCCToPrintable(node->GetDependencyNodeType()).GetChars(), node->GetIdentifier().GetChars());
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
			node->SetState(DependencyState::NotAnalyzedOrCompiled);
			return ResultCode::kOK;
		}

		const int kCompilePhase = 0;
		const int kAnalysisPhase = 1;

		DependencyState resultState = node->GetDependencyState();

		for (int phase = 0; phase < 2; phase++)
		{
			if (phase == kCompilePhase)
			{
				if (node->GetDependencyState() != DependencyState::UpToDate)
					continue;
			}

			if (phase == kAnalysisPhase)
			{
				if (node->GetDependencyState() != DependencyState::UpToDate && node->GetDependencyState() != DependencyState::NotCompiled)
					continue;
			}

			CallbackSpan<FileStatusView, const IDependencyNode *> productsSpan = (phase == kCompilePhase) ? node->GetCompileProducts() : node->GetAnalysisProducts();
			CallbackSpan<FileDependencyInfoView, const IDependencyNode *> fileDepsSpan = (phase == kCompilePhase) ? node->GetCompileFileDependencies() : node->GetAnalysisFileDependencies();
			CallbackSpan<DirectoryScanDependencyInfoView, const IDependencyNode *> dirScanDepsSpan = (phase == kCompilePhase) ? node->GetCompileDirectoryScanDependencies() : node->GetAnalysisDirectoryScanDependencies();

			bool demote = false;
			if (!demote)
			{
				for (const FileDependencyInfoView &fdiView : fileDepsSpan)
				{
					if (!fdiView.m_mustBeUpToDate)
						continue;

					FileStatusView fileStatus;
					bool exists = false;
					RKIT_CHECK(ResolveFileStatus(fdiView.m_status.m_location, fdiView.m_status.m_filePath, false, fileStatus, true, exists));

					if (exists != fdiView.m_fileExists)
					{
						demote = true;
						break;
					}

					if (exists && fileStatus != fdiView.m_status)
					{
						demote = true;
						break;
					}
				}
			}

			if (!demote)
			{
				for (const FileStatusView &productStatus : productsSpan)
				{
					FileStatusView fileStatus;
					bool exists = false;
					RKIT_CHECK(ResolveFileStatus(productStatus.m_location, productStatus.m_filePath, false, fileStatus, true, exists));

					if (!exists)
					{
						demote = true;
						break;
					}

					if (fileStatus != productStatus)
					{
						demote = true;
						break;
					}
				}
			}

			if (!demote)
			{
				if (phase == kCompilePhase)
				{
					for (const data::ContentID &contentID : node->GetCompileCASProducts())
					{
						data::ContentIDString idString = contentID.ToString();

						CIPath contentPath;
						RKIT_CHECK(contentPath.AppendComponent("content"));
						RKIT_CHECK(contentPath.AppendComponent(idString.ToStringView()));

						FileStatusView fileStatus;
						bool exists = false;
						RKIT_CHECK(ResolveFileStatus(rkit::buildsystem::BuildFileLocation::kOutputDir, contentPath, false, fileStatus, true, exists));

						if (!exists)
							demote = true;
					}
				}
			}

			if (!demote)
			{
				for (DirectoryScanDependencyInfoView dsdiView : dirScanDepsSpan)
				{
					if (!dsdiView.m_mustBeUpToDate)
						continue;

					DirectoryScanView dirScanStatus;
					bool exists = false;
					RKIT_CHECK(ResolveDirectoryScan(dsdiView.m_dirScan.m_directoryLocation, dsdiView.m_dirScan.m_directoryPath, dsdiView.m_dirScan.m_directoryMode, dirScanStatus, true, exists));

					if (exists != dsdiView.m_dirExists)
					{
						demote = true;
						break;
					}

					if (exists && dirScanStatus != dsdiView.m_dirScan)
					{
						demote = true;
						break;
					}
				}
			}

			if (demote)
			{
				if (phase == kCompilePhase)
					resultState = DependencyState::NotCompiled;
				else
					resultState = DependencyState::NotAnalyzedOrCompiled;
			}
		}

		if (node->GetDependencyState() != resultState)
			node->SetState(resultState);

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

			for (const NodeDependencyInfo &nodeDep : node->GetNodeDependencies())
			{
				if (nodeDep.m_mustBeUpToDate && nodeDep.m_node->GetDependencyState() != DependencyState::UpToDate)
				{
					node->SetState(DependencyState::NotCompiled);
					break;
				}
			}

			return ResultCode::kOK;
		}

		if (!nodeDep.m_mustBeUpToDate)
			return ResultCode::kOK;

		DependencyNode *nodeDepNode = static_cast<DependencyNode *>(nodeDep.m_node);
		if (nodeDepNode->GetDependencyCheckPhase() == DependencyCheckPhase::Completed)
			return ResultCode::kOK;

		if (nodeDepNode->GetDependencyCheckPhase() != DependencyCheckPhase::None)
		{
			ErrorBlameNode(nodeDepNode, "Circular dependency");
			return ResultCode::kOperationFailed;
		}

		RKIT_CHECK(AddRelevantNode(nodeDepNode));
		RKIT_CHECK(m_depCheckStack.Append(nodeDepNode));

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::ResolveDirectoryScan(BuildFileLocation location, const CIPathView &path, bool directoryMode, DirectoryScanView &outStatusView, bool cached, bool &outExists)
	{
		if (cached)
		{
			DirectoryScanKey lookupLocKey(location, path, directoryMode);

			HashMap<DirectoryScanKey, UniquePtr<CachedDirScan> >::ConstIterator_t lookupIt = m_cachedDirScan.Find(lookupLocKey);
			if (lookupIt != m_cachedDirScan.end())
			{
				const CachedDirScan *fStatus = lookupIt.Value().Get();

				outExists = fStatus->m_exists;

				if (fStatus->m_exists)
					outStatusView = fStatus->m_scan.ToView();
				else
					outStatusView = DirectoryScanView();

				return ResultCode::kOK;
			}
		}

		UniquePtr<CachedDirScan> dirScan;
		RKIT_CHECK(New<CachedDirScan>(dirScan));

		RKIT_CHECK(dirScan->m_scan.m_directoryPath.Set(path));
		dirScan->m_scan.m_directoryLocation = location;
		dirScan->m_exists = false;

		rkit::buildsystem::FileStatusView dirFSView;
		bool dirExists = false;
		RKIT_CHECK(ResolveFileStatus(location, path, true, dirFSView, true, dirScan->m_exists));

		if (dirExists && dirFSView.m_isDirectory)
		{
			dirScan->m_exists = true;

			class ResultCallbackShim
			{
			public:
				explicit ResultCallbackShim(DirectoryScan &dirScan, bool directoryMode)
					: m_dirScan(dirScan)
					, m_directoryMode(directoryMode)
				{
				}

				Result ApplyFileStatus(const FileStatusView &fileStatus)
				{
					CIPath path;
					RKIT_CHECK(path.Set(fileStatus.m_filePath));
					RKIT_CHECK(m_dirScan.m_paths.Append(path));

					return ResultCode::kOK;
				}

				static Result StaticApplyFileStatus(void *userdata, const FileStatusView &fileStatus)
				{
					return static_cast<ResultCallbackShim *>(userdata)->ApplyFileStatus(fileStatus);
				}

			private:
				DirectoryScan &m_dirScan;
				bool m_directoryMode;
			};

			ResultCallbackShim shim(dirScan->m_scan, directoryMode);

			RKIT_CHECK(m_fs->EnumerateDirectory(location, path, !directoryMode, directoryMode, &shim, ResultCallbackShim::StaticApplyFileStatus));

			outExists = true;
			outStatusView = dirScan->m_scan.ToView();
		}
		else
		{
			outExists = false;
			outStatusView = DirectoryScanView();
			outStatusView.m_directoryLocation = location;
			outStatusView.m_directoryMode = directoryMode;
			outStatusView.m_directoryPath = path;
		}

		DirectoryScanKey cacheKey(location, dirScan->m_scan.m_directoryPath, directoryMode);
		RKIT_CHECK(m_cachedDirScan.Set(std::move(cacheKey), std::move(dirScan)));

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::ResolveFileStatus(BuildFileLocation location, const CIPathView &path, bool allowDirectories, FileStatusView &outStatusView, bool cached, bool &outExists)
	{
		// Keep the behavior here in sync with TryOpenFileRead
		if (cached)
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

		fileStatus->m_exists = false;
		RKIT_CHECK(m_fs->ResolveFileStatusIfExists(location, path, false, fileStatus.Get(), ResolveCachedFileStatusCallback));

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

	Result BuildSystemInstance::TryOpenFileRead(BuildFileLocation location, const CIPathView &path, UniquePtr<ISeekableReadStream> &outFile)
	{
		// Keep the behavior here in sync with ResolveFileStatus
		return m_fs->TryOpenFileRead(location, path, outFile);
	}

	Result BuildSystemInstance::OpenFileWrite(BuildFileLocation location, const CIPathView &path, UniquePtr<ISeekableReadWriteStream> &outFile)
	{
		OSAbsPath fullPath;

		if (location == rkit::buildsystem::BuildFileLocation::kIntermediateDir)
		{
			RKIT_CHECK(ConstructIntermediatePath(fullPath, path));
		}
		else if (location == rkit::buildsystem::BuildFileLocation::kOutputDir)
		{
			RKIT_CHECK(ConstructOutputPath(fullPath, path));
		}
		else
			return ResultCode::kFileOpenError;


		ISystemDriver *sysDriver = GetDrivers().m_systemDriver;
		Result openResult = sysDriver->OpenFileReadWriteAbs(outFile, fullPath, true, true, true);

		if (!openResult.IsOK())
		{
			rkit::log::ErrorFmt("Failed to open output file '%s'", fullPath.CStr());
			return ResultCode::kFileOpenError;
		}

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::EnumerateFiles(BuildFileLocation location, const CIPathView &path, void *userdata, EnumerateFilesResultCallback_t resultCallback)
	{
		return InternalEnumerateFilesOrDirectories(location, path, false, userdata, resultCallback);
	}

	Result BuildSystemInstance::EnumerateDirectories(BuildFileLocation location, const CIPathView &path, void *userdata, EnumerateFilesResultCallback_t resultCallback)
	{
		return InternalEnumerateFilesOrDirectories(location, path, true, userdata, resultCallback);
	}

	Result BuildSystemInstance::InternalEnumerateFilesOrDirectories(BuildFileLocation location, const CIPathView &path, bool directoryMode, void *userdata, EnumerateFilesResultCallback_t resultCallback)
	{
		bool exists = false;
		DirectoryScanView directoryScanView;
		RKIT_CHECK(ResolveDirectoryScan(location, path, directoryMode, directoryScanView, true, exists));

		if (!exists)
			return ResultCode::kOK;

		for (const CIPathView path : directoryScanView.m_paths)
		{
			RKIT_CHECK(resultCallback(userdata, path));
		}

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::UpdateCAS(const data::ContentID &contentID)
	{
		data::ContentIDString contentIDString = contentID.ToString();

		OSAbsPath contentBasePath = m_dataDir;
		OSAbsPath contentPath;

		CIPath contentDir;
		RKIT_CHECK(contentDir.AppendComponent("content"));

		{
			CIPath pathCI = contentDir;

			RKIT_CHECK(pathCI.AppendComponent(contentIDString.ToStringView()));

			OSRelPath osRelPath;
			RKIT_CHECK(osRelPath.ConvertFrom(static_cast<CIPathView>(pathCI)));

			contentPath = contentBasePath;
			RKIT_CHECK(contentPath.Append(osRelPath));
		}

		ISystemDriver *sysDriver = GetDrivers().m_systemDriver;


		FileAttributes attribs;
		bool exists = false;
		RKIT_CHECK(sysDriver->GetFileAttributesAbs(contentPath, exists, attribs));

		if (!exists)
		{
			HashMap<data::ContentID, CASSource>::ConstIterator_t it = m_casSources.Find(contentID);
			if (it == m_casSources.end())
			{
				rkit::log::ErrorFmt("Couldn't find CAS source for content '%s'", contentIDString.ToStringView().GetChars());
				return ResultCode::kInternalError;
			}

			OSAbsPath tempPath;

			{
				CIPath pathCI = contentDir;

				String tempName;
				RKIT_CHECK(tempName.Set(contentIDString.ToStringView()));
				RKIT_CHECK(tempName.Append(".tmp"));

				RKIT_CHECK(pathCI.AppendComponent(tempName));

				OSRelPath osRelPath;
				RKIT_CHECK(osRelPath.ConvertFrom(static_cast<CIPathView>(pathCI)));

				tempPath = contentBasePath;
				RKIT_CHECK(tempPath.Append(osRelPath));
			}

			UniquePtr<ISeekableReadStream> inStream;
			RKIT_CHECK(m_fs->TryOpenFileRead(it.Value().m_location, it.Value().m_path, inStream));

			if (!inStream.IsValid())
			{
				rkit::log::ErrorFmt("Couldn't open CAS source '%s'", it.Value().m_path.CStr());
				return ResultCode::kInternalError;
			}

			UniquePtr<ISeekableWriteStream> outStream;
			RKIT_CHECK(sysDriver->OpenFileWriteAbs(outStream, tempPath, true, true, true));

			FilePos_t amountRemaining = inStream->GetSize();
			while (amountRemaining > 0)
			{
				uint8_t buffer[2048];

				size_t amountToCopy = sizeof(buffer);
				if (amountToCopy > amountRemaining)
					amountToCopy = static_cast<size_t>(amountRemaining);

				RKIT_CHECK(inStream->ReadAll(buffer, amountToCopy));
				RKIT_CHECK(outStream->WriteAll(buffer, amountToCopy));

				amountRemaining -= amountToCopy;
			}

			RKIT_CHECK(outStream->Flush());
			outStream.Reset();
			inStream.Reset();

			RKIT_CHECK(sysDriver->MoveFileFromAbsToAbs(tempPath, contentPath, true));
		}

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::RegisterNodeTypeByExtension(const StringSliceView &ext, uint32_t nodeNamespace, uint32_t nodeType)
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

	bool BuildSystemInstance::FindNodeTypeByFileExtension(const StringSliceView &ext, uint32_t &outNamespace, uint32_t &outType) const
	{
		HashMap<String, NodeTypeKey>::ConstIterator_t it = m_nodeTypesByExtension.Find(ext);
		if (it == m_nodeTypesByExtension.end())
			return false;

		outNamespace = it.Value().GetNamespace();
		outType = it.Value().GetType();

		return true;
	}

	CallbackSpan<IDependencyNode *, const IBuildSystemInstance *> BuildSystemInstance::GetBuildRelevantNodes() const
	{
		return CallbackSpan<IDependencyNode *, const IBuildSystemInstance *>(GetRelevantNodeByIndex, this, m_relevantNodes.Count());
	}

	Result BuildSystemInstance::RegisterCASSource(const data::ContentID &contentID, BuildFileLocation inputFileLocation, const CIPathView &path)
	{
		if (m_casSources.Find(contentID) == m_casSources.end())
		{
			CASSource casSource;
			RKIT_CHECK(casSource.m_path.Set(path));
			casSource.m_location = inputFileLocation;

			RKIT_CHECK(m_casSources.Set(contentID, std::move(casSource)));
		}

		return ResultCode::kOK;
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

	Result BuildSystemInstance::ConstructIntermediatePath(OSAbsPath &outStr, const CIPathView &path) const
	{
		outStr = m_intermedDir;

		OSRelPath relPath;
		RKIT_CHECK(relPath.ConvertFrom(path));

		RKIT_CHECK(outStr.Append(relPath));

		return ResultCode::kOK;
	}

	Result BuildSystemInstance::ConstructOutputPath(OSAbsPath &outStr, const CIPathView &path) const
	{
		outStr = m_dataDir;

		OSRelPath relPath;
		RKIT_CHECK(relPath.ConvertFrom(path));

		RKIT_CHECK(outStr.Append(relPath));

		return ResultCode::kOK;
	}

	BuildSystemInstance::PrintableFourCC BuildSystemInstance::FourCCToPrintable(uint32_t fourCC)
	{
		return BuildSystemInstance::PrintableFourCC(fourCC);
	}

	CIPathView BuildSystemInstance::GetCacheFileName()
	{
		return "buildsystem.cache";
	}

	IDependencyNode *BuildSystemInstance::GetRelevantNodeByIndex(const IBuildSystemInstance *const &instance, size_t index)
	{
		return static_cast<const BuildSystemInstance *>(instance)->m_relevantNodes[index];
	}

	BuildSystemInstance::PrintableFourCC::PrintableFourCC(uint32_t fourCC)
	{
		rkit::utils::ExtractFourCC(fourCC, m_chars[0], m_chars[1], m_chars[2], m_chars[3]);
		m_chars[4] = '\0';
	}

	const char *BuildSystemInstance::PrintableFourCC::GetChars() const
	{
		return m_chars;
	}

	StringView BuildSystemInstance::ContentID::GetStringView() const
	{
		return StringView(m_chars, kContentStringSize);
	}
}
