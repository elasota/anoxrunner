#pragma once

#include "rkit/Core/StringView.h"

namespace rkit::data
{
	struct ContentID;
};

namespace anox::game
{
	struct ScriptNamedResourceRefBase
	{
		rkit::ByteStringView m_name;
		const rkit::data::ContentID *m_contentID = nullptr;
	};

	template<uint32_t TNamespace, uint32_t TType>
	struct ScriptNamedResourceRef : public ScriptNamedResourceRefBase
	{
	};
}

#include "anox/Data/ResourceTypeCodes.h"
#include "anox/AnoxModule.h"

namespace anox::game
{
	using ScriptFileResourceRef = ScriptNamedResourceRef<kAnoxNamespaceID, resloaders::kContentIDRawFileResourceTypeCode>;
}
