#include "IncludeWindows.h"

namespace rkit { namespace boot { namespace win32
{
	int __declspec(dllimport) MainCommon(HINSTANCE hInstance);
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
