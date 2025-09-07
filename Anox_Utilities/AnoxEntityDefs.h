#pragma once

namespace rkit
{
	template<class T>
	class Span;
}

namespace anox { namespace data {
	struct EntityDefsSchema;
} }

namespace anox { namespace utils {
	const data::EntityDefsSchema &GetEntityDefs();
} }
