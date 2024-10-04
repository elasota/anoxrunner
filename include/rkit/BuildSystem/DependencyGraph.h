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
		enum class DependencyState
		{
			// All dependencies are up-to-date and this node is also up-to-date
			UpToDate,

			// No analysis or processing has been run on the node yet
			NotAnalyzedOrProcessed,

			// Analysis has been run on the node (sub-nodes and dependencies are known) but the node itself
			// has not been processed yet.
			NotProcessed,
		};

		struct FileDependencyInfo
		{
			StringView m_filePath;
			bool m_fileExists;
			uint64_t m_fileSize;
			UTCMSecTimestamp_t m_fileTime;
		};

		struct IDependencyNode
		{
			virtual ~IDependencyNode() {}

			virtual void Reset() = 0;

			virtual StringView GetIdentifier() const = 0;
			virtual DependencyState GetDependencyState() const = 0;

			virtual uint32_t GetDependencyNodeType() const = 0;
			virtual Result RunAnalysis() = 0;
			virtual Result RunProcess() = 0;

			virtual Span<StringView> GetProducts() const = 0;
			virtual Span<FileDependencyInfo> GetFileDependencies() const = 0;
			virtual CallbackSpan<IDependencyNode*, IDependencyNode*> GetNodeDependencies() const = 0;
		};

		struct INodeCreationParameters
		{
			virtual ~INodeCreationParameters() {}

			virtual Result CreateNode(UniquePtr<IDependencyNode> &outNode) = 0;
		};

		struct IDependencyGraphFactory
		{
			virtual ~IDependencyGraphFactory() {}

			virtual Result CreateNode(uint32_t nodeType, const StringView &identifier, UniquePtr<IDependencyNode> &outNode) const = 0;
		};
	}
}
