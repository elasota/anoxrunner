#pragma once

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

			// No analysis or processing has been run on the node yet
			NotAnalyzedOrProcessed,

			// Analysis has been run on the node (sub-nodes and dependencies are known) but the node itself
			// has not been processed yet.  Dependencies will be available, but
			// products will not.
			NotCompiled,
		};

		struct FileStatusView
		{
			StringView m_filePath;
			uint64_t m_fileSize;
			UTCMSecTimestamp_t m_fileTime;
		};

		struct FileStatus
		{
			String m_filePath;
			uint64_t m_fileSize;
			UTCMSecTimestamp_t m_fileTime;
		};

		struct FileDependencyInfoView
		{
			FileStatusView m_status;
			bool m_fileExists;
			bool m_mustBeUpToDate;
		};

		struct FileDependencyInfo
		{
			FileStatus m_status;
			bool m_fileExists = true;
			bool m_mustBeUpToDate = true;
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

			virtual uint32_t GetDependencyNodeType() const = 0;
			virtual Result RunAnalysis() = 0;
			virtual Result RunCompile() = 0;

			virtual CallbackSpan<FileStatusView, IDependencyNode *> GetIntermediateProducts() const = 0;
			virtual CallbackSpan<FileStatusView, IDependencyNode *> GetOutputProducts() const = 0;
			virtual CallbackSpan<FileDependencyInfoView, IDependencyNode *> GetFileDependencies() const = 0;
			virtual CallbackSpan<NodeDependecyInfo, IDependencyNode*> GetNodeDependencies() const = 0;

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

			virtual Result Initialize() = 0;

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

			virtual Result CreateNode(uint32_t nodeNamespace, uint32_t nodeType, BuildFileLocation buildFileLocation, const StringView &identifier, UniquePtr<IDependencyNode> &outNode) const = 0;
			virtual Result RegisterNodeCompiler(uint32_t nodeNamespace, uint32_t nodeType, UniquePtr<IDependencyNodeCompiler> &&factory) = 0;
		};
	}
}
