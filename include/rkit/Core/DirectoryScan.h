#pragma once

#include "FileAttributes.h"
#include "String.h"

namespace rkit
{
	struct Result;

	struct DirectoryScanItem
	{
		StringView m_fileName;
		FileAttributes m_attribs;
	};

	struct IDirectoryScan
	{
		virtual ~IDirectoryScan() {}

		virtual Result GetNext(bool &haveItem, DirectoryScanItem &outItem) = 0;
	};
}
