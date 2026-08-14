#pragma once

#include "rkit/Core/RefCounted.h"
#include "rkit/Core/Opaque.h"
#include "rkit/Core/SpanProtos.h"
#include "rkit/Core/StringProto.h"

#include "rkit/Math/Vec.h"

#include "anox/Data/Scene.h"
#include "anox/Data/SceneCommandOpcodes.generated.h"

#include "SandboxResourceLoader.h"

namespace anox::game
{
	class SceneManagerImpl;
	class ScenePackageImpl;

	enum class SceneFocusType : uint8_t
	{
		kNone,
		kScaledSource,
		kScaledTarget,
		kInscribed,
		kCircumscribed,
		kFixedSource,
		kFixedTarget,
		kFixedInscribed,

		kCount,
	};

	class ScenePackage final : public rkit::RefCounted, public rkit::Opaque<ScenePackageImpl>
	{
		friend class SceneManagerImpl;

	public:
		void RCTrackerZero() override;

		static constexpr size_t kNumPathTypes = static_cast<size_t>(data::ScenePathType::kCount);
		static constexpr size_t kNumContentTypes = static_cast<size_t>(data::SceneContentRefType::kCount);

		struct Path
		{
			data::ScenePathType m_pathType = data::ScenePathType::kCount;

			const void *m_firstNode = nullptr;

			uint32_t m_timeOffs = 0;
			uint32_t m_maxLen = 0;
			size_t m_numNodes = 0;
			rkit::Optional<size_t> m_group;
		};

		struct Block
		{
			rkit::ConstSpan<Path> m_paths;
			size_t m_numGroups = 0;
		};

		struct Resource
		{
			rkit::data::ContentID m_contentID;
			SandboxResourceHandle m_resHandle;
		};

		struct NodeBase
		{
			uint32_t m_flags = 0;
			uint32_t m_timeLen = 0;
		};

		struct CubicNode : public NodeBase
		{
			rkit::math::Vec3 m_position;
			rkit::math::Vec3 m_velocity;
			SceneFocusType m_relativeMode = SceneFocusType::kCount;
		};

		struct FocusNode : public NodeBase
		{
			uint32_t m_focusTarget = 0;
		};

		struct CommandNode : public NodeBase
		{
			rkit::ConstSpan<data::SceneCommandOpcode> m_commandOpcodes;
			rkit::ConstSpan<uint32_t> m_commandParamDWords;
		};

		struct ScaleNode : public NodeBase
		{
			rkit::math::Vec3 m_scale;
			rkit::math::Vec3 m_delta;
		};

		struct RollNode : public NodeBase
		{
			float m_value = 0.f;
			float m_rate = 0.f;
		};

		struct FOVNode : public NodeBase
		{
			float m_value = 0.f;
			float m_rate = 0.f;
		};

		uint32_t GetCineID() const;
		bool IsInterrupt() const;

		rkit::ConstSpan<Block> GetBlocks() const;
	};
}
