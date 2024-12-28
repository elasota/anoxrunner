#include "rkit/Core/Result.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"

#include "Win32DisplayManager.h"

#include "rkit/Win32/IncludeWindows.h"

#include "ConvUtil.h"

#include <CommCtrl.h>

namespace rkit::render
{
	class SplashWindow_Win32 : public IDisplay, public IProgressMonitor
	{
	public:
		explicit SplashWindow_Win32(IMallocDriver *alloc, HINSTANCE hInst);
		~SplashWindow_Win32();

		Result Initialize(ATOM splashWCAtom);

		DisplayMode GetDisplayMode() const override { return DisplayMode::kSplash; }

		IProgressMonitor *GetProgressMonitor() { return this; }

		static Result RegisterWndClass(ATOM &outAtom, HINSTANCE hInst);

		Result SetText(const StringView &text) override;
		Result SetRange(uint64_t minimum, uint64_t maximum) override;
		Result SetValue(uint64_t value) override;
		void FlushEvents() override;

	private:
		struct PrimaryMonitorSearchData
		{
			HMONITOR m_selectedMonitor = nullptr;

			RECT m_monitorRect;

			bool m_haveAnyRect = false;
			bool m_failed = false;
		};

		LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

		static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

		static BOOL CALLBACK FindPrimaryMonitor(HMONITOR hMonitor, HDC hDC, LPRECT lpRect, LPARAM lParam);

		void FlushWinMessageQueue();

		HINSTANCE m_hInst = nullptr;
		HWND m_hWnd = nullptr;
		HWND m_label = nullptr;
		HWND m_progressBar = nullptr;
		HFONT m_labelFont = nullptr;

		IMallocDriver *m_alloc = nullptr;

		uint64_t m_progressMin = 0;
		uint32_t m_compactedProgressRange = 100;
		uint32_t m_compactedProgressValue = 100;

		int m_progressBitsShift = 0;
	};

	class DisplayManager_Win32 final : public DisplayManagerBase_Win32
	{
	public:
		explicit DisplayManager_Win32(IMallocDriver *alloc, HINSTANCE m_hInst);
		~DisplayManager_Win32();

		Result Initialize();

		Result CreateDisplay(UniquePtr<IDisplay> &display, DisplayMode displayMode) override;

		static LPCWSTR AtomToClassName(ATOM atom);

	private:
		Result CreateSplash(UniquePtr<IDisplay> &display, DisplayMode displayMode);
		Result CreateRenderWindow(UniquePtr<IDisplay> &display, DisplayMode displayMode);

		IMallocDriver *m_alloc;
		HINSTANCE m_hInst;

		ATOM m_splashWCAtom = 0;
	};

	SplashWindow_Win32::SplashWindow_Win32(IMallocDriver *alloc, HINSTANCE hInst)
		: m_hWnd(nullptr)
		, m_hInst(hInst)
		, m_alloc(alloc)
	{
	}

	SplashWindow_Win32::~SplashWindow_Win32()
	{
		if (m_label)
			::DestroyWindow(m_label);
		if (m_progressBar)
			::DestroyWindow(m_progressBar);
		if (m_labelFont)
			::DeleteObject(m_labelFont);
		if (m_hWnd)
			::DestroyWindow(m_hWnd);
	}

	Result SplashWindow_Win32::Initialize(ATOM splashWCAtom)
	{
		LONG width = 320;
		LONG height = 60;

		{
			DWORD style = (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE);
			DWORD exStyle = 0;
			HMENU menu = nullptr;
			BOOL haveMenu = (menu != nullptr) ? TRUE : FALSE;

			PrimaryMonitorSearchData searchData = {};

			if (!EnumDisplayMonitors(nullptr, nullptr, FindPrimaryMonitor, reinterpret_cast<LPARAM>(&searchData)))
				return ResultCode::kOperationFailed;

			if (!searchData.m_haveAnyRect)
				return ResultCode::kOperationFailed;

			LONG doubleMidpointX = searchData.m_monitorRect.left + searchData.m_monitorRect.right;
			LONG doubleMidpointY = searchData.m_monitorRect.top + searchData.m_monitorRect.bottom;

			LONG topLeftX = (doubleMidpointX - width) / 2;
			LONG topLeftY = (doubleMidpointY - height) / 2;

			RECT wr = { topLeftX, topLeftY, topLeftX + width, topLeftY + height };

			IDisplay *lparam = this;

			if (!::AdjustWindowRectEx(&wr, style, haveMenu, exStyle))
				return ResultCode::kOperationFailed;

			// FIXME: Configurable title
			HWND hwnd = CreateWindowExW(exStyle, DisplayManager_Win32::AtomToClassName(splashWCAtom), L"RKit Launch Window", style, wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr, m_hInst, lparam);
			if (!hwnd)
				return ResultCode::kOperationFailed;

			m_hWnd = hwnd;
		}

		// Label
		{
			DWORD style = WS_CHILD | SS_ENDELLIPSIS | SS_NOPREFIX;
			DWORD exStyle = 0;

			// FIXME: Localize
			HWND label = ::CreateWindowExW(exStyle, L"Static", L"Starting up...", style, 10, 10, width - 20, 16, m_hWnd, nullptr, nullptr, nullptr);

			if (!label)
				return ResultCode::kOperationFailed;

			m_label = label;
		}

		NONCLIENTMETRICSW nonClientMetrics = {};
		nonClientMetrics.cbSize = sizeof(nonClientMetrics);
		if (!::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, 0, &nonClientMetrics, 0))
			return ResultCode::kOperationFailed;

		// Label font
		m_labelFont = ::CreateFontIndirectW(&nonClientMetrics.lfCaptionFont);
		if (m_labelFont)
			::SendMessageW(m_label, WM_SETFONT, reinterpret_cast<WPARAM>(m_labelFont), 0);

		::ShowWindow(m_label, SW_SHOW);

		// Progress bar
		{
			DWORD style = WS_CHILD | WS_VISIBLE | PBS_SMOOTH;
			DWORD exStyle = 0;

			HWND pbar = ::CreateWindowExW(exStyle, PROGRESS_CLASS, nullptr, style, 10, 30, width - 20, 20, m_hWnd, nullptr, nullptr, nullptr);

			if (!pbar)
				return ResultCode::kOperationFailed;

			m_progressBar = pbar;

			::SendMessageW(m_progressBar, PBM_SETRANGE, 0, MAKELPARAM(0, m_compactedProgressRange));
			::SendMessageW(m_progressBar, PBM_SETSTEP, 1, 0);
		}

		return ResultCode::kOK;
	}


	Result SplashWindow_Win32::SetText(const StringView &text)
	{
		Vector<wchar_t> wtext(m_alloc);
		RKIT_CHECK(ConvUtil_Win32::UTF8ToUTF16(text.GetChars(), wtext));

		::SendMessageW(m_label, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(wtext.GetBuffer()));

		FlushWinMessageQueue();

		return ResultCode::kOK;
	}

	Result SplashWindow_Win32::SetRange(uint64_t minimum, uint64_t maximum)
	{
		int bitsShift = 0;

		uint64_t progressRange = maximum - minimum;

		if (maximum <= minimum)
			progressRange = 1;

		m_progressMin = minimum;

		while (progressRange > 0xffffu)
		{
			progressRange >>= 1;
			bitsShift++;
		}

		if (m_compactedProgressValue != 0)
		{
			::SendMessageW(m_progressBar, PBM_SETPOS, m_compactedProgressValue, 0);
			m_compactedProgressValue = 0;
		}

		m_progressBitsShift = bitsShift;

		if (progressRange != m_compactedProgressRange)
		{
			m_compactedProgressRange = static_cast<uint32_t>(progressRange);

			::SendMessageW(m_progressBar, PBM_SETRANGE, 0, MAKELPARAM(0, m_compactedProgressRange));
		}

		FlushWinMessageQueue();

		return ResultCode::kOK;
	}

	Result SplashWindow_Win32::SetValue(uint64_t value)
	{
		if (value < m_progressMin)
			value = 0;
		else
		{
			value = (value - m_progressMin) >> m_progressBitsShift;
			if (value > m_compactedProgressRange)
				value = m_compactedProgressRange;
		}

		if (value != m_compactedProgressValue)
		{
			m_compactedProgressValue = static_cast<uint32_t>(value);
			::SendMessageW(m_progressBar, PBM_SETPOS, m_compactedProgressValue, 0);
		}

		FlushWinMessageQueue();

		return ResultCode::kOK;
	}

	void SplashWindow_Win32::FlushEvents()
	{
		FlushWinMessageQueue();
	}


	Result SplashWindow_Win32::RegisterWndClass(ATOM &outAtom, HINSTANCE hInst)
	{
		WNDCLASSW wc = {};
		wc.lpfnWndProc = StaticWndProc;
		wc.hInstance = hInst;
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW);
		wc.lpszClassName = L"RKit Splash Window";
		wc.style = CS_NOCLOSE;

		ATOM atom = RegisterClassW(&wc);
		if (!atom)
			return ResultCode::kOperationFailed;

		outAtom = atom;

		return ResultCode::kOK;
	}

	LRESULT SplashWindow_Win32::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		return ::DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	LRESULT CALLBACK SplashWindow_Win32::StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		LONG_PTR iDisplayPtr = 0;

		if (msg == WM_NCCREATE || msg == WM_CREATE)
		{
			const CREATESTRUCTW *createInfo = reinterpret_cast<const CREATESTRUCTW *>(lParam);

			iDisplayPtr = reinterpret_cast<LONG_PTR>(createInfo->lpCreateParams);
			SetWindowLongPtrW(hWnd, GWLP_USERDATA, iDisplayPtr);
		}
		else
			iDisplayPtr = GetWindowLongPtrW(hWnd, GWLP_USERDATA);

		return static_cast<SplashWindow_Win32 *>(reinterpret_cast<IDisplay *>(iDisplayPtr))->WndProc(hWnd, msg, wParam, lParam);
	}

	BOOL CALLBACK SplashWindow_Win32::FindPrimaryMonitor(HMONITOR hMonitor, HDC hDC, LPRECT lpRect, LPARAM lParam)
	{
		PrimaryMonitorSearchData *searchData = reinterpret_cast<PrimaryMonitorSearchData *>(lParam);

		MONITORINFOEXW monitorInfo;
		monitorInfo.cbSize = sizeof(monitorInfo);

		if (!::GetMonitorInfoW(hMonitor, &monitorInfo))
		{
			searchData->m_failed = true;
			return FALSE;
		}

		if ((searchData->m_selectedMonitor == nullptr) || (monitorInfo.dwFlags & MONITORINFOF_PRIMARY))
		{
			searchData->m_selectedMonitor = hMonitor;
			searchData->m_haveAnyRect = true;
			searchData->m_monitorRect = monitorInfo.rcWork;
		}

		return TRUE;
	}

	void SplashWindow_Win32::FlushWinMessageQueue()
	{
		BOOL bRet = FALSE;
		MSG msg;
		while ((bRet = PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) != 0)
		{
			if (bRet == -1)
			{
				return;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	DisplayManager_Win32::DisplayManager_Win32(IMallocDriver *alloc, HINSTANCE hInst)
		: m_alloc(alloc)
		, m_hInst(hInst)
	{
	}

	DisplayManager_Win32::~DisplayManager_Win32()
	{
		if (m_splashWCAtom)
			::UnregisterClassW(AtomToClassName(m_splashWCAtom), m_hInst);
	}

	Result DisplayManager_Win32::Initialize()
	{
		RKIT_CHECK(SplashWindow_Win32::RegisterWndClass(m_splashWCAtom, m_hInst));

		return ResultCode::kOK;
	}

	Result DisplayManager_Win32::CreateDisplay(UniquePtr<IDisplay> &display, DisplayMode displayMode)
	{
		if (displayMode == DisplayMode::kSplash)
			return CreateSplash(display, displayMode);
		else
			return CreateRenderWindow(display, displayMode);
	}

	Result DisplayManager_Win32::CreateSplash(UniquePtr<IDisplay> &display, DisplayMode displayMode)
	{
		UniquePtr<SplashWindow_Win32> splashWindow;
		RKIT_CHECK(NewWithAlloc<SplashWindow_Win32>(splashWindow, m_alloc, m_alloc, m_hInst));

		RKIT_CHECK(splashWindow->Initialize(m_splashWCAtom));

		display = std::move(splashWindow);

		return ResultCode::kOK;
	}

	Result DisplayManager_Win32::CreateRenderWindow(UniquePtr<IDisplay> &display, DisplayMode displayMode)
	{
		return ResultCode::kNotYetImplemented;
	}

	LPCWSTR DisplayManager_Win32::AtomToClassName(ATOM atom)
	{
		uintptr_t asPtr = atom;
		return reinterpret_cast<LPCWSTR>(asPtr);
	}

	Result DisplayManagerBase_Win32::Create(UniquePtr<DisplayManagerBase_Win32> &outDisplayManager, IMallocDriver *alloc, HINSTANCE hInst)
	{
		UniquePtr<DisplayManager_Win32> displayManager;
		RKIT_CHECK(NewWithAlloc<DisplayManager_Win32>(displayManager, alloc, alloc, hInst));
		RKIT_CHECK(displayManager->Initialize());

		outDisplayManager = std::move(displayManager);

		return ResultCode::kOK;
	}
}
