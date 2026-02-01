#pragma once

#include "rkit/BuildSystem/DependencyGraph.h"

namespace rkit
{
	template<class T>
	class Span;

	template<class T>
	class Vector;
}

namespace rkit { namespace buildsystem
{
	struct IDependencyNode;

	class DepsNodeCompiler final : public IDependencyNodeCompiler
	{
	public:
		DepsNodeCompiler();

		bool HasAnalysisStage() const override;
		Result RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;
		Result RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;

	private:
		static Result HandlePathChunk(Vector<Utf8Char_t> &constructedPath, bool &outOK, size_t chunkIndex, IDependencyNode *node, const Span<const Utf8Char_t> &chunk);
	};
} } // rkit::buildsystem
