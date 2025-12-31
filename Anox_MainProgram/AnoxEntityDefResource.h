#pragma once

#include "AnoxResourceManager.h"

namespace anox { namespace data {
	struct UserEntityDef;
} }

namespace anox
{
	class AnoxEntityDefResourceBase : public AnoxResourceBase
	{
	public:
		virtual const data::UserEntityDef &GetEntityDef() const = 0;
	};

	class AnoxEntityDefResourceLoaderBase : public AnoxContentIDKeyedResourceLoader<AnoxEntityDefResourceBase>
	{
	public:
		static rkit::Result Create(rkit::RCPtr<AnoxEntityDefResourceLoaderBase> &outLoader);
	};
}
