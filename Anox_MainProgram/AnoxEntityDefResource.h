#pragma once

#include "rkit/Core/EnumMask.h"
#include "rkit/Core/String.h"
#include "rkit/Math/Vec.h"

#include "anox/Game/SpawnDef.h"
#include "anox/Game/UserEntityDefValues.h"
#include "anox/Data/EntityDef.h"
#include "anox/Label.h"

#include "AnoxDataReader.h"
#include "AnoxResourceManager.h"

namespace anox { namespace data {
	struct UserEntityDef;
} }

namespace anox
{
	class AnoxEntityDefResourceLoader;

	class AnoxEntityDefResourceBase : public AnoxResourceBase
	{
	public:
		virtual const game::UserEntityDefValues &GetValues() const = 0;
		virtual const rkit::ByteString &GetDescription() const = 0;
	};

	class AnoxEntityDefResourceLoaderBase : public AnoxContentIDKeyedResourceLoader<AnoxEntityDefResourceBase>
	{
	public:
		static rkit::Result Create(rkit::RCPtr<AnoxEntityDefResourceLoaderBase> &outLoader);
	};
}
