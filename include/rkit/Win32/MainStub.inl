#include "IncludeWindows.h"

namespace rkit { namespace boot { namespace win32
{
#if !RKIT_IS_DEBUG
	int MainCommon(HINSTANCE hInstance);
#else
	int __declspec(dllimport) MainCommon(HINSTANCE hInstance);
#endif
} } }

#ifdef _CONSOLE
int main(int argc, const char **argv)
{
	return rkit::boot::win32::MainCommon(GetModuleHandleW(nullptr));
}
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	return rkit::boot::win32::MainCommon(hInstance);
}
#endif
