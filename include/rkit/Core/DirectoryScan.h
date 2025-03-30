#pragma once

#include "FileAttributes.h"
#include "Path.h"

namespace rkit
{
	struct Result;

	struct DirectoryScanItem
	{
		OSRelPathView m_fileName;
		FileAttributes m_attribs;
	};

	struct IDirectoryScan
	{
		virtual ~IDirectoryScan() {}

		virtual Result GetNext(bool &haveItem, DirectoryScanItem &outItem) = 0;
	};
}
