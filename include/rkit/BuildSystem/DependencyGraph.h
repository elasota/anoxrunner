#pragma once

#include "BuildFileLocation.h"

#include "rkit/Core/Result.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Timestamp.h"

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

	namespace buildsystem
	{
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
			StringView m_filePath;
			BuildFileLocation m_location = BuildFileLocation::kSourceDir;
			uint64_t m_fileSize = 0;
			UTCMSecTimestamp_t m_fileTime = 0;

			bool operator==(const FileStatusView &other) const;
			bool operator!=(const FileStatusView &other) const;
		};

		struct FileStatus
		{
			String m_filePath;
			BuildFileLocation m_location = BuildFileLocation::kSourceDir;
			uint64_t m_fileSize = 0;
			UTCMSecTimestamp_t m_fileTime = 0;

			FileStatusView ToView() const;
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
			BuildFileLocation m_location = BuildFileLocation::kSourceDir;
			bool m_fileExists = true;
			bool m_mustBeUpToDate = true;

			FileDependencyInfoView ToView() const;
		};

		struct NodeDependecyInfo
		{
			IDependencyNode *m_node;
			bool m_mustBeUpToDate = true;
		};

		struct IDependencyNodePrivateData
		{
			virtual ~IDependencyNodePrivateData() {}

			virtual Result Serialize(IWriteStream *stream) const = 0;
			virtual Result Deserialize(IReadStream *stream) = 0;
		};

		struct IDependencyNode
		{
			virtual ~IDependencyNode() {}

			virtual void MarkOutOfDate() = 0;

			virtual StringView GetIdentifier() const = 0;
			virtual DependencyState GetDependencyState() const = 0;

			virtual uint32_t GetDependencyNodeNamespace() const = 0;
			virtual uint32_t GetDependencyNodeType() const = 0;
			virtual BuildFileLocation GetInputFileLocation() const = 0;
			virtual Result RunAnalysis() = 0;
			virtual Result RunCompile() = 0;

			virtual CallbackSpan<FileStatusView, const IDependencyNode *> GetProducts() const = 0;
			virtual CallbackSpan<FileDependencyInfoView, const IDependencyNode *> GetFileDependencies() const = 0;
			virtual CallbackSpan<NodeDependecyInfo, const IDependencyNode *> GetNodeDependencies() const = 0;

			virtual Result Serialize(IWriteStream *stream) const = 0;
			virtual Result Deserialize(IReadStream *stream) = 0;
		};

		struct IDependencyNodeCompilerFeedback
		{
			virtual ~IDependencyNodeCompilerFeedback() {}

			virtual Result AddIntermediateProduct(const FileStatusView &file) = 0;
			virtual Result AddOutputProduct(const FileStatusView &file) = 0;
		};

		struct IDependencyNodeCompiler
		{
			virtual ~IDependencyNodeCompiler() {}

			virtual bool HasAnalysisStage() const = 0;
			virtual Result RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) = 0;
			virtual Result RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) = 0;

			virtual Result CreatePrivateData(UniquePtr<IDependencyNodePrivateData> &outPrivateData) = 0;

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

			virtual Result CreateNode(uint32_t nodeNamespace, uint32_t nodeType, BuildFileLocation inputFileLocation, const StringView &identifier, UniquePtr<IDependencyNode> &outNode) const = 0;
			virtual Result RegisterNodeCompiler(uint32_t nodeNamespace, uint32_t nodeType, UniquePtr<IDependencyNodeCompiler> &&factory) = 0;
		};
	}
}

inline bool rkit::buildsystem::FileStatusView::operator==(const FileStatusView &other) const
{
	return m_filePath == other.m_filePath && m_fileSize == other.m_fileSize && m_fileTime == other.m_fileTime && m_location == other.m_location;
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

	return view;
}

inline rkit::buildsystem::FileDependencyInfoView rkit::buildsystem::FileDependencyInfo::ToView() const
{
	rkit::buildsystem::FileDependencyInfoView result;
	result.m_fileExists = m_fileExists;
	result.m_mustBeUpToDate = m_mustBeUpToDate;
	result.m_status = m_status.ToView();

	return result;
}
