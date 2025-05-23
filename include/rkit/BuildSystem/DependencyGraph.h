#pragma once

#include "BuildFileLocation.h"

#include "rkit/Core/Result.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Timestamp.h"
#include "rkit/Core/Vector.h"

#include <cstddef>

namespace rkit
{
	struct IReadStream;
	struct IWriteStream;

	template<class T>
	class Span;

	template<class T, class TUserdata>
	class CallbackSpan;

	template<class T>
	class UniquePtr;

	namespace data
	{
		struct ContentID;
	}

	namespace buildsystem
	{
		struct IBuildSystemInstance;
		struct IDependencyNode;
		struct IDependencyNodeCompiler;

		enum class DependencyState
		{
			// All dependencies are up-to-date and this node is also up-to-date
			UpToDate,

			// No analysis or compiling has been run on the node yet
			NotAnalyzedOrCompiled,

			// Analysis has been run on the node (sub-nodes and dependencies are known) but the node itself
			// has not been processed yet.  Dependencies will be available, but
			// products will not.
			NotCompiled,
		};

		struct FileStatusView
		{
			CIPathView m_filePath;
			BuildFileLocation m_location = BuildFileLocation::kSourceDir;
			uint64_t m_fileSize = 0;
			UTCMSecTimestamp_t m_fileTime = 0;
			bool m_isDirectory = false;

			bool operator==(const FileStatusView &other) const;
			bool operator!=(const FileStatusView &other) const;
		};

		struct FileStatus
		{
			CIPath m_filePath;
			BuildFileLocation m_location = BuildFileLocation::kSourceDir;
			uint64_t m_fileSize = 0;
			UTCMSecTimestamp_t m_fileTime = 0;
			bool m_isDirectory = false;

			FileStatusView ToView() const;
			Result Set(const FileStatusView &view);
		};

		struct FileDependencyInfoView
		{
			FileStatusView m_status;
			bool m_fileExists = true;
			bool m_mustBeUpToDate = true;

			bool operator==(const FileStatusView &other) const;
			bool operator!=(const FileStatusView &other) const;
		};

		struct FileDependencyInfo
		{
			FileStatus m_status;
			bool m_fileExists = true;
			bool m_mustBeUpToDate = true;

			FileDependencyInfoView ToView() const;
			Result Set(const FileDependencyInfoView &view);
		};

		struct DirectoryScanView
		{
			CallbackSpan<CIPathView, void *> m_paths;
			CIPathView m_directoryPath;
			BuildFileLocation m_directoryLocation = BuildFileLocation::kSourceDir;
			bool m_directoryMode = false;

			bool operator==(const DirectoryScanView &other) const;
			bool operator!=(const DirectoryScanView &other) const;
		};

		struct DirectoryScan
		{
			Vector<CIPath> m_paths;
			CIPath m_directoryPath;
			BuildFileLocation m_directoryLocation = BuildFileLocation::kSourceDir;
			bool m_directoryMode = false;

			DirectoryScanView ToView() const;
			Result Set(const DirectoryScanView &view);
		};

		struct DirectoryScanDependencyInfoView
		{
			DirectoryScanView m_dirScan;
			bool m_dirExists = true;
			bool m_mustBeUpToDate = true;

			bool operator==(const DirectoryScanDependencyInfoView &other) const;
			bool operator!=(const DirectoryScanDependencyInfoView &other) const;
		};

		struct DirectoryScanDependencyInfo
		{
			DirectoryScan m_dirScan;
			bool m_dirExists = true;
			bool m_mustBeUpToDate = true;

			DirectoryScanDependencyInfoView ToView() const;
			Result Set(const DirectoryScanDependencyInfoView &view);
		};

		struct NodeDependencyInfo
		{
			IDependencyNode *m_node = nullptr;
			bool m_mustBeUpToDate = true;
		};

		struct IDeserializeResolver
		{
			virtual Result GetString(size_t index, StringView &outString) const = 0;
			virtual Result GetDependencyNode(size_t index, IDependencyNode *&outNode) const = 0;
		};

		struct IDependencyNode
		{
			virtual ~IDependencyNode() {}

			virtual void MarkOutOfDate() = 0;
			virtual bool WasCompiled() const = 0;
			virtual bool IsContentBased() const = 0;

			virtual Span<const uint8_t> GetContent() const = 0;

			virtual StringView GetIdentifier() const = 0;
			virtual DependencyState GetDependencyState() const = 0;

			virtual uint32_t GetDependencyNodeNamespace() const = 0;
			virtual uint32_t GetDependencyNodeType() const = 0;
			virtual BuildFileLocation GetInputFileLocation() const = 0;
			virtual Result RunAnalysis(IBuildSystemInstance *instance) = 0;
			virtual Result RunCompile(IBuildSystemInstance *instance) = 0;

			virtual CallbackSpan<FileStatusView, const IDependencyNode *> GetAnalysisProducts() const = 0;
			virtual CallbackSpan<FileStatusView, const IDependencyNode *> GetCompileProducts() const = 0;
			virtual CallbackSpan<data::ContentID, const IDependencyNode *> GetCompileCASProducts() const = 0;
			virtual CallbackSpan<FileDependencyInfoView, const IDependencyNode *> GetAnalysisFileDependencies() const = 0;
			virtual CallbackSpan<FileDependencyInfoView, const IDependencyNode *> GetCompileFileDependencies() const = 0;
			virtual CallbackSpan<DirectoryScanDependencyInfoView, const IDependencyNode *> GetAnalysisDirectoryScanDependencies() const = 0;
			virtual CallbackSpan<DirectoryScanDependencyInfoView, const IDependencyNode *> GetCompileDirectoryScanDependencies() const = 0;
			virtual CallbackSpan<NodeDependencyInfo, const IDependencyNode *> GetNodeDependencies() const = 0;

			virtual Result Serialize(IWriteStream &stream, StringPoolBuilder &stringPool) const = 0;
			virtual Result Deserialize(IReadStream &stream, const IDeserializeResolver &resolver) = 0;
		};

		struct IDependencyNodeCompilerFeedback
		{
			typedef Result(*EnumerateFilesResultCallback_t)(void *userdata, const CIPathView &fileName);

			virtual ~IDependencyNodeCompilerFeedback() {}

			virtual Result CheckInputExists(BuildFileLocation location, const CIPathView &path, bool &outExists) = 0;
			virtual Result OpenInput(BuildFileLocation location, const CIPathView &path, UniquePtr<ISeekableReadStream> &inputFile) = 0;
			virtual Result TryOpenInput(BuildFileLocation location, const CIPathView &path, UniquePtr<ISeekableReadStream> &inputFile) = 0;
			virtual Result OpenOutput(BuildFileLocation location, const CIPathView &path, UniquePtr<ISeekableReadWriteStream> &outputFile) = 0;
			virtual Result IndexCAS(BuildFileLocation location, const CIPathView &path, data::ContentID &outContentID) = 0;
			virtual Result AddAnonymousDeployableContent(BuildFileLocation location, const CIPathView &path) = 0;

			virtual Result AddNodeDependency(uint32_t nodeTypeNamespace, uint32_t nodeTypeID, BuildFileLocation inputFileLocation, const StringView &identifier) = 0;
			virtual bool FindNodeTypeByFileExtension(const StringSliceView &ext, uint32_t &outNamespace, uint32_t &outType) const = 0;

			virtual Result EnumerateFiles(BuildFileLocation location, const CIPathView &path, void *userdata, EnumerateFilesResultCallback_t resultCallback) = 0;
			virtual Result EnumerateDirectories(BuildFileLocation location, const CIPathView &path, void *userdata, EnumerateFilesResultCallback_t resultCallback) = 0;

			virtual IBuildSystemInstance *GetBuildSystemInstance() const = 0;

			virtual Result CheckFault() const = 0;
		};

		struct IDependencyNodeCompiler
		{
			virtual ~IDependencyNodeCompiler() {}

			virtual bool HasAnalysisStage() const = 0;
			virtual Result RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) = 0;
			virtual Result RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) = 0;

			virtual uint32_t GetVersion() const = 0;
		};

		struct INodeFactory
		{
			virtual ~INodeFactory() {}

			virtual Result CreateNode(UniquePtr<IDependencyNodeCompiler> &outNode, const StringView &identifier) const = 0;
		};

		struct IDependencyGraphFactory
		{
			virtual ~IDependencyGraphFactory() {}

			virtual Result RegisterNodeCompiler(uint32_t nodeNamespace, uint32_t nodeType, UniquePtr<IDependencyNodeCompiler> &&factory) = 0;
			virtual Result CreateNamedNode(uint32_t nodeNamespace, uint32_t nodeType, BuildFileLocation buildFileLocation, const StringView &identifier, UniquePtr<IDependencyNode> &outNode) const = 0;
			virtual Result CreateContentNode(uint32_t nodeNamespace, uint32_t nodeType, BuildFileLocation buildFileLocation, Vector<uint8_t> &&content, UniquePtr<IDependencyNode> &outNode) const = 0;
		};
	}
}

inline bool rkit::buildsystem::FileStatusView::operator==(const FileStatusView &other) const
{
	return m_filePath == other.m_filePath
		&& m_fileSize == other.m_fileSize
		&& m_fileTime == other.m_fileTime
		&& m_location == other.m_location
		&& m_isDirectory == other.m_isDirectory;
}

inline bool rkit::buildsystem::FileStatusView::operator!=(const FileStatusView &other) const
{
	return !((*this) == other);
}

inline rkit::buildsystem::FileStatusView rkit::buildsystem::FileStatus::ToView() const
{
	rkit::buildsystem::FileStatusView view;
	view.m_location = m_location;
	view.m_filePath = m_filePath;
	view.m_fileSize = m_fileSize;
	view.m_fileTime = m_fileTime;
	view.m_isDirectory = m_isDirectory;

	return view;
}

inline rkit::Result rkit::buildsystem::FileStatus::Set(const FileStatusView &view)
{
	m_location = view.m_location;
	RKIT_CHECK(m_filePath.Set(view.m_filePath));
	m_fileSize = view.m_fileSize;
	m_fileTime = view.m_fileTime;
	m_isDirectory = view.m_isDirectory;

	return ResultCode::kOK;
}

inline rkit::buildsystem::FileDependencyInfoView rkit::buildsystem::FileDependencyInfo::ToView() const
{
	rkit::buildsystem::FileDependencyInfoView result;
	result.m_fileExists = m_fileExists;
	result.m_mustBeUpToDate = m_mustBeUpToDate;
	result.m_status = m_status.ToView();

	return result;
}

inline rkit::Result rkit::buildsystem::FileDependencyInfo::Set(const FileDependencyInfoView &view)
{
	m_fileExists = view.m_fileExists;
	m_mustBeUpToDate = view.m_mustBeUpToDate;
	RKIT_CHECK(m_status.Set(view.m_status));

	return ResultCode::kOK;
}

inline bool rkit::buildsystem::DirectoryScanView::operator==(const DirectoryScanView &other) const
{
	if (m_directoryLocation != other.m_directoryLocation)
		return false;

	if (m_directoryPath != other.m_directoryPath)
		return false;

	if (m_directoryMode != other.m_directoryMode)
		return false;

	const size_t numPaths = m_paths.Count();
	if (other.m_paths.Count() != numPaths)
		return false;

	for (size_t i = 0; i < numPaths; i++)
	{
		if (m_paths[i] != other.m_paths[i])
			return false;
	}

	return true;
}

inline bool rkit::buildsystem::DirectoryScanView::operator!=(const DirectoryScanView &other) const
{
	return !((*this) == other);
}

inline rkit::buildsystem::DirectoryScanView rkit::buildsystem::DirectoryScan::ToView() const
{
	typedef Vector<CIPath> PathVector_t;

	struct CIPathVectorReadCallback
	{
		typedef void *Userdata_t;

		static CIPathView GetElement(const Userdata_t &userdata, size_t index)
		{
			return (*static_cast<const PathVector_t *>(userdata))[index];
		}
	};

	DirectoryScanView result;

	const PathVector_t *pathsVector = &m_paths;

	result.m_paths = CallbackSpan<CIPathView, void *>(CIPathVectorReadCallback::GetElement, const_cast<PathVector_t *>(pathsVector), pathsVector->Count());
	result.m_directoryMode = m_directoryMode;
	result.m_directoryPath = m_directoryPath;
	result.m_directoryLocation = m_directoryLocation;

	return result;
}

inline rkit::Result rkit::buildsystem::DirectoryScan::Set(const DirectoryScanView &view)
{
	const size_t numPaths = view.m_paths.Count();

	RKIT_CHECK(m_paths.Resize(numPaths));

	for (size_t i = 0; i < numPaths; i++)
	{
		RKIT_CHECK(m_paths[i].Set(view.m_paths[i]));
	}

	m_directoryMode = view.m_directoryMode;
	RKIT_CHECK(m_directoryPath.Set(view.m_directoryPath));
	m_directoryLocation = view.m_directoryLocation;

	return ResultCode::kOK;
}

inline rkit::buildsystem::DirectoryScanDependencyInfoView rkit::buildsystem::DirectoryScanDependencyInfo::ToView() const
{
	DirectoryScanDependencyInfoView result;
	result.m_dirExists = m_dirExists;
	result.m_dirScan = m_dirScan.ToView();
	result.m_mustBeUpToDate = m_mustBeUpToDate;

	return result;
}

inline rkit::Result rkit::buildsystem::DirectoryScanDependencyInfo::Set(const DirectoryScanDependencyInfoView &view)
{
	RKIT_CHECK(m_dirScan.Set(view.m_dirScan));
	m_dirExists = view.m_dirExists;
	m_mustBeUpToDate = view.m_mustBeUpToDate;

	return ResultCode::kOK;
}
