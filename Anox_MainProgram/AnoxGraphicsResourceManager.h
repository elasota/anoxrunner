#pragma once

namespace anox
{
	struct IGraphicsResourceManager
	{
		virtual ~IGraphicsResourceManager() {}

		virtual void UnloadAll() = 0;
	};
}
