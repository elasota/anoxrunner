#pragma once

#include "rkit/Core/FourCC.h"
#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/Core/PathProto.h"
#include "rkit/Core/StringProto.h"

#include "anox/Data/MaterialResourceType.h"

namespace rkit
{
	template<class T>
	class Span;
}

namespace anox
{
	struct UserEntityDef2;
}

namespace anox { namespace buildsystem
{
	class UserEntityDictionaryBase
	{
	public:
		virtual ~UserEntityDictionaryBase() {}

		virtual bool FindEntityDef(const rkit::AsciiStringSliceView &name, uint32_t &outEDefID) const = 0;
		virtual rkit::AsciiStringView GetEDefType(uint32_t edefID) const = 0;
		virtual uint32_t GetEDefCount() const = 0;

		virtual rkit::Result WriteEDef(rkit::IWriteStream &stream, uint32_t edefID) const = 0;
	};

	static const uint32_t kEntityDefNodeID = RKIT_FOURCC('E', 'D', 'E', 'F');

	class EntityDefCompilerBase : public rkit::buildsystem::IDependencyNodeCompiler
	{
	public:
		static rkit::Result FormatEDef(rkit::String &edefIdentifier, uint32_t edefID);
		static rkit::Result LoadUserEntityDictionary(rkit::UniquePtr<UserEntityDictionaryBase> &dictionary, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

		static rkit::Result Create(rkit::UniquePtr<EntityDefCompilerBase> &outCompiler);
	};
} } // anox::buildsystem
