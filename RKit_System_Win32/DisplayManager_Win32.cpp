#include "Win32DisplayManager.h"

#include "rkit/Win32/Display_Win32.h"
#include "rkit/Win32/IncludeWindows.h"

#include "rkit/Core/Event.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/MutexLock.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Thread.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/ResizableRingBuffer.h"
#include "rkit/Core/Vector.h"

#include "ConvUtil.h"

#include <CommCtrl.h>

namespace rkit { namespace render
{
	class BaseDisplay_Win32 : public IDisplay_Win32
	{
	protected:
		struct PrimaryMonitorSearchData
		{
			HMONITOR m_selectedMonitor = nullptr;

			RECT m_monitorRect;

			bool m_haveAnyRect = false;
			bool m_failed = false;
		};

		static BOOL CALLBACK FindPrimaryMonitor(HMONITOR hMonitor, HDC hDC, LPRECT lpRect, LPARAM lParam);

	};

	class SplashWindow_Win32 final : public BaseDisplay_Win32, public IProgressMonitor
	{
	public:
		explicit SplashWindow_Win32(IMallocDriver *alloc, HINSTANCE hInst);
		~SplashWindow_Win32();

		Result Initialize(ATOM splashWCAtom);

		DisplayMode GetDisplayMode() const override { return DisplayMode::kSplash; }
		bool CanChangeToDisplayMode(DisplayMode mode) const override { return mode == GetDisplayMode(); }
		Result ChangeToDisplayMode(DisplayMode mode) override { return ResultCode::kInternalError; }

		uint32_t GetSimultaneousImageCount() const override { return 0; }

		IProgressMonitor *GetProgressMonitor() override { return this; }

		static Result RegisterWndClass(ATOM &outAtom, HINSTANCE hInst);

		Result SetText(const StringView &text) override;
		Result SetRange(uint64_t minimum, uint64_t maximum) override;
		Result SetValue(uint64_t value) override;
		void FlushEvents() override;

		Result PostWindowThreadTask(void *userdata, void (*callback)(void *userdata)) override;
		HWND GetHWND() override;
		HINSTANCE GetHINSTANCE() override;

	private:

		LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

		static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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

	class RenderWindow_Win32 : public BaseDisplay_Win32
	{
	public:
		explicit RenderWindow_Win32(IMallocDriver *alloc, HINSTANCE hInst);
		~RenderWindow_Win32();

		Result Initialize(ISystemDriver *sysDriver, ATOM renderWCAtom, uint32_t width, uint32_t height);

		DisplayMode GetDisplayMode() const override { return m_displayMode; }
		bool CanChangeToDisplayMode(DisplayMode mode) const override;
		Result ChangeToDisplayMode(DisplayMode mode) override;

		uint32_t GetSimultaneousImageCount() const override;

		IProgressMonitor *GetProgressMonitor() override { return nullptr; }

		static Result RegisterWndClass(ATOM &outAtom, HINSTANCE hInst);

		Result PostWindowThreadTask(void *userdata, void (*callback)(void *userdata)) override;
		HWND GetHWND() override;
		HINSTANCE GetHINSTANCE() override;

	private:
		class ThreadContext final : public IThreadContext
		{
		public:
			explicit ThreadContext(RenderWindow_Win32 &window);

			Result Run() override;

		private:
			RenderWindow_Win32 &m_window;
		};

		struct ActionQueueItem
		{
			typedef Result (*RunCallback_t)(RenderWindow_Win32 &window, void *userdata);
			typedef void (*DisposeCallback_t)(void *userdata);

			void *m_userdata;
			RunCallback_t m_runCallback;
			DisposeCallback_t m_disposeCallback;
			ActionQueueItem *m_nextItem;

			ResizableRingBufferHandle<DefaultResizableRingBufferTraits> m_thisAlloc;
		};

		struct WindowCreateParams
		{
			RenderWindow_Win32 *m_window = nullptr;
		};

		template<class TParameters, Result (RenderWindow_Win32::*TFunction)(TParameters &params)>
		Result QueueAction(const TParameters &params);

		template<class TParameters, Result(RenderWindow_Win32:: *TFunction)(TParameters &params)>
		Result QueueAction(TParameters &&params);

		void PostAQIToActionQueue(ActionQueueItem *aqi);

		struct ThreadFuncQueueUtils
		{
			typedef void (*CopyOrMoveConstructParametersCallback_t)(void *userdata, void *source);

			template<class TParameters>
			static void CopyConstructParams(void *userdata, void *source);

			template<class TParameters>
			static void MoveConstructParams(void *userdata, void *source);

			template<class TParameters>
			static void DeleteParams(void *userdata);

			template<class TParameters, Result(RenderWindow_Win32:: *TFunction)(TParameters &params)>
			static Result RunThunk(RenderWindow_Win32 &window, void *userdata);
		};

		template<class TParameters, Result(RenderWindow_Win32:: *TFunction)(TParameters &params)>
		Result QueueActionBase(ThreadFuncQueueUtils::CopyOrMoveConstructParametersCallback_t paramsCallback, void *params);

		struct ChangeDisplayModeParams : public NoCopy
		{
			ChangeDisplayModeParams();
			ChangeDisplayModeParams(ChangeDisplayModeParams &&other) noexcept;
			~ChangeDisplayModeParams();

			typedef Result (RenderWindow_Win32:: *CallbackMethod_t)();

			CallbackMethod_t m_callback = nullptr;
			bool *m_completedFlag = nullptr;
			IEvent *m_signalEvent = nullptr;

			uint32_t *m_outWidth = nullptr;
			uint32_t *m_outHeight = nullptr;
		};

		struct RunCustomTaskParams
		{
			void *m_userdata = nullptr;
			void (*m_task)(void *userdata) = nullptr;
		};

		void KickThread();
		Result ThreadFunc();
		Result ThreadProcessWinMessages();
		Result SetupWindow();

		Result BecomeBorderlessFullScreen();
		Result BecomeWindowed();


		Result ThreadBecomeBorderlessFullScreen();
		Result ThreadBecomeWindowed();

		Result Action_ChangeDisplayMode(ChangeDisplayModeParams &displayModeParams);
		Result Action_RunCustomTask(RunCustomTaskParams &customTaskParams);

		LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

		static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

		static const DWORD kKickThreadMsg = (WM_APP + 1);

		DisplayMode m_displayMode = DisplayMode::kWindowed;
		LONG m_windowStyle = 0;
		HWND m_hWnd = nullptr;
		ATOM m_wcAtom = 0;

		IMallocDriver *m_alloc = nullptr;
		HINSTANCE m_hInst = nullptr;

		UniquePtr<IEvent> m_destroyWindowEvent;
		UniquePtr<IEvent> m_threadFinishedEvent;

		UniquePtr<IEvent> m_exitEvent;
		UniquePtr<IEvent> m_startEvent;
		UniquePtr<IEvent> m_changeDisplayModeEvent;
		UniqueThreadRef m_thread;

		UniquePtr<IMutex> m_actionsMutex;
		ResizableRingBuffer<DefaultResizableRingBufferTraits> m_actions;
		ActionQueueItem *m_firstItem = nullptr;
		ActionQueueItem *m_lastItem = nullptr;
		bool m_isWaitingForActions = false;
		bool m_wantsToExit = false;
		bool m_threadHasExited = false;

		// This is only changed on the task thread
		bool m_actionLoopWasKicked = false;
		Result m_msgProcResult;

		RECT m_windowedModeRect = {};
		uint32_t m_defaultWidth = 0;
		uint32_t m_defaultHeight = 0;
	};

	class DisplayManager_Win32 final : public DisplayManagerBase_Win32
	{
	public:
		explicit DisplayManager_Win32(IMallocDriver *alloc, HINSTANCE m_hInst);
		~DisplayManager_Win32();

		Result Initialize();

		Result CreateDisplay(UniquePtr<IDisplay> &display, DisplayMode displayMode) override;
		Result CreateDisplay(UniquePtr<IDisplay> &display, DisplayMode displayMode, uint32_t width, uint32_t height) override;

		static LPCWSTR AtomToClassName(ATOM atom);

	private:
		Result CreateSplash(UniquePtr<IDisplay> &display, DisplayMode displayMode);
		Result CreateRenderWindow(UniquePtr<IDisplay> &display, DisplayMode displayMode, uint32_t width, uint32_t height);

		IMallocDriver *m_alloc;
		HINSTANCE m_hInst;

		ATOM m_splashWCAtom = 0;
		ATOM m_renderWCAtom = 0;
	};

	BOOL CALLBACK BaseDisplay_Win32::FindPrimaryMonitor(HMONITOR hMonitor, HDC hDC, LPRECT lpRect, LPARAM lParam)
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
			HWND hwnd = CreateWindowExW(exStyle, DisplayManager_Win32::AtomToClassName(splashWCAtom), L"RKit Launch Window", style, wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top, HWND_TOP, nullptr, m_hInst, lparam);
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
			m_compactedProgressValue = 0;
			::SendMessageW(m_progressBar, PBM_SETPOS, m_compactedProgressValue, 0);
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

	Result SplashWindow_Win32::PostWindowThreadTask(void *userdata, void (*callback)(void *userdata))
	{
		return ResultCode::kInternalError;
	}

	HWND SplashWindow_Win32::GetHWND()
	{
		return m_hWnd;
	}

	HINSTANCE SplashWindow_Win32::GetHINSTANCE()
	{
		return m_hInst;
	}

	Result SplashWindow_Win32::RegisterWndClass(ATOM &outAtom, HINSTANCE hInst)
	{
		WNDCLASSW wc = {};
		wc.lpfnWndProc = StaticWndProc;
		wc.hInstance = hInst;
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW);
		wc.lpszClassName = L"RKitSplashWindow";
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

		if (iDisplayPtr)
			return static_cast<SplashWindow_Win32 *>(reinterpret_cast<IDisplay *>(iDisplayPtr))->WndProc(hWnd, msg, wParam, lParam);
		else
			return ::DefWindowProcW(hWnd, msg, wParam, lParam);
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


	RenderWindow_Win32::ChangeDisplayModeParams::ChangeDisplayModeParams()
		: m_callback(nullptr)
		, m_completedFlag(nullptr)
		, m_signalEvent(nullptr)
		, m_outWidth(nullptr)
		, m_outHeight(nullptr)
	{
	}

	RenderWindow_Win32::ChangeDisplayModeParams::ChangeDisplayModeParams(ChangeDisplayModeParams &&other) noexcept
		: m_callback(other.m_callback)
		, m_completedFlag(other.m_completedFlag)
		, m_signalEvent(other.m_signalEvent)
		, m_outWidth(other.m_outWidth)
		, m_outHeight(other.m_outHeight)
	{
		other.m_callback = nullptr;
		other.m_completedFlag = nullptr;
		other.m_signalEvent = nullptr;
		other.m_outWidth = nullptr;
		other.m_outHeight = nullptr;
	}

	RenderWindow_Win32::ChangeDisplayModeParams::~ChangeDisplayModeParams()
	{
		if (m_signalEvent)
			m_signalEvent->Signal();

		if (m_completedFlag)
			*m_completedFlag = false;
	}

	RenderWindow_Win32::RenderWindow_Win32(IMallocDriver *alloc, HINSTANCE hInst)
		: m_alloc(alloc)
		, m_hInst(hInst)
		, m_actions(alloc, alloc)
		, m_msgProcResult(ResultCode::kOK)
	{
	}

	RenderWindow_Win32::~RenderWindow_Win32()
	{
		if (m_thread.IsValid())
		{
			{
				MutexLock lock(*m_actionsMutex);
				m_wantsToExit = true;
			}

			KickThread();

			m_exitEvent->Wait();

			m_destroyWindowEvent->Signal();

			m_exitEvent->Wait();
		}

		while (m_firstItem)
		{
			ActionQueueItem *item = m_firstItem;
			m_firstItem = item->m_nextItem;

			item->m_disposeCallback(item->m_userdata);
			m_actions.Dispose(item->m_thisAlloc);
		}

		if (m_hWnd)
			::DestroyWindow(m_hWnd);
	}

	Result RenderWindow_Win32::Initialize(ISystemDriver *sysDriver, ATOM renderWCAtom, uint32_t width, uint32_t height)
	{
		m_wcAtom = renderWCAtom;
		m_defaultWidth = width;
		m_defaultHeight = height;

		UniquePtr<ThreadContext> ctx;
		RKIT_CHECK(NewWithAlloc<ThreadContext>(ctx, m_alloc, *this));

		RKIT_CHECK(sysDriver->CreateEvent(m_startEvent, false, false));
		RKIT_CHECK(sysDriver->CreateEvent(m_exitEvent, true, false));
		RKIT_CHECK(sysDriver->CreateEvent(m_destroyWindowEvent, false, false));
		RKIT_CHECK(sysDriver->CreateEvent(m_changeDisplayModeEvent, true, false));
		RKIT_CHECK(sysDriver->CreateMutex(m_actionsMutex));

		RKIT_CHECK(sysDriver->CreateThread(m_thread, std::move(ctx), u8"RenderWindow"));

		m_startEvent->Wait();
		m_startEvent.Reset();

		if (!m_hWnd)
			return ResultCode::kOperationFailed;

		return ResultCode::kOK;
	}

	bool RenderWindow_Win32::CanChangeToDisplayMode(DisplayMode mode) const
	{
		return mode == DisplayMode::kWindowed || mode == DisplayMode::kBorderlessFullscreen;
	}

	Result RenderWindow_Win32::ChangeToDisplayMode(DisplayMode mode)
	{
		if (mode == m_displayMode)
			return ResultCode::kOK;

		switch (mode)
		{
		case DisplayMode::kBorderlessFullscreen:
			return BecomeBorderlessFullScreen();
		case DisplayMode::kWindowed:
			return BecomeWindowed();
		default:
			return ResultCode::kInternalError;
		}
	}

	uint32_t RenderWindow_Win32::GetSimultaneousImageCount() const
	{
		return 1;
	}

	Result RenderWindow_Win32::RegisterWndClass(ATOM &outAtom, HINSTANCE hInst)
	{
		WNDCLASSEXW wc = {};

		wc.cbSize = sizeof(wc);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = StaticWndProc;
		wc.hInstance = hInst;
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		//wc.hIcon = m_osGlobals->m_hIcon;
		//wc.hIconSm = m_osGlobals->m_hIconSm;
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW);
		wc.lpszClassName = L"RKitRenderWindow";

		ATOM atom = RegisterClassExW(&wc);
		if (!atom)
			return ResultCode::kOperationFailed;

		outAtom = atom;

		return ResultCode::kOK;
	}

	Result RenderWindow_Win32::PostWindowThreadTask(void *userdata, void (*callback)(void *userdata))
	{
		RunCustomTaskParams params = {};
		params.m_userdata = userdata;
		params.m_task = callback;

		return QueueAction<RunCustomTaskParams, &RenderWindow_Win32::Action_RunCustomTask>(params);
	}

	HWND RenderWindow_Win32::GetHWND()
	{
		return m_hWnd;
	}

	HINSTANCE RenderWindow_Win32::GetHINSTANCE()
	{
		return m_hInst;
	}


	template<class TParameters, Result(RenderWindow_Win32:: *TFunction)(TParameters &params)>
	Result RenderWindow_Win32::QueueAction(const TParameters &params)
	{
		void *vparams = const_cast<TParameters *>(&params);
		ThreadFuncQueueUtils::CopyOrMoveConstructParametersCallback_t cb = ThreadFuncQueueUtils::CopyConstructParams<TParameters>;
		return QueueActionBase<TParameters, TFunction>(cb, vparams);
	}

	template<class TParameters, Result(RenderWindow_Win32:: *TFunction)(TParameters &params)>
	Result RenderWindow_Win32::QueueAction(TParameters &&params)
	{
		void *vparams = &params;
		ThreadFuncQueueUtils::CopyOrMoveConstructParametersCallback_t cb = ThreadFuncQueueUtils::MoveConstructParams<TParameters>;
		return QueueActionBase<TParameters, TFunction>(cb, vparams);
	}

	void RenderWindow_Win32::PostAQIToActionQueue(ActionQueueItem *aqi)
	{
		MutexLock lock(*m_actionsMutex);
		if (m_lastItem)
			m_lastItem->m_nextItem = aqi;
		else
		{
			m_firstItem = aqi;

			if (m_isWaitingForActions)
			{
				m_isWaitingForActions = false;
				::PostMessageW(m_hWnd, kKickThreadMsg, 0, 0);
			}
		}

		m_lastItem = aqi;
	}


	template<class TParameters>
	void RenderWindow_Win32::ThreadFuncQueueUtils::CopyConstructParams(void *userdata, void *source)
	{
		const TParameters *params = static_cast<const TParameters *>(source);
		new (userdata) TParameters(*params);
	}

	template<class TParameters>
	void RenderWindow_Win32::ThreadFuncQueueUtils::MoveConstructParams(void *userdata, void *source)
	{
		TParameters *params = static_cast<TParameters *>(source);
		new (userdata) TParameters(std::move(*params));
	}

	template<class TParameters>
	void RenderWindow_Win32::ThreadFuncQueueUtils::DeleteParams(void *userdata)
	{
		TParameters *params = static_cast<TParameters *>(userdata);
		params->~TParameters();
	}

	template<class TParameters, Result(RenderWindow_Win32:: *TFunction)(TParameters &params)>
	Result RenderWindow_Win32::ThreadFuncQueueUtils::RunThunk(RenderWindow_Win32 &window, void *userdata)
	{
		TParameters *params = static_cast<TParameters *>(userdata);
		return (window.*TFunction)(*params);
	}

	template<class TParameters, Result(RenderWindow_Win32:: *TFunction)(TParameters &params)>
	Result RenderWindow_Win32::QueueActionBase(ThreadFuncQueueUtils::CopyOrMoveConstructParametersCallback_t paramsCallback, void *params)
	{
		TParameters &p = *static_cast<TParameters *>(params);

		const size_t alignment = (alignof(TParameters) > alignof(ActionQueueItem)) ? alignof(TParameters) : alignof(ActionQueueItem);
		static_assert(alignment <= DefaultResizableRingBufferTraits::kMaxAlignment);

		size_t baseSize = sizeof(ActionQueueItem);
		if (baseSize % alignment != 0)
			baseSize += (alignment - (baseSize % alignment));

		size_t totalSize = baseSize + sizeof(TParameters);

		ResizableRingBufferHandle<DefaultResizableRingBufferTraits> allocHandle;
		DefaultResizableRingBufferTraits::AddrOffset_t offset = 0;
		const DefaultResizableRingBufferTraits::MemChunk_t *memChunk = nullptr;

		{
			MutexLock lock(*m_actionsMutex);
			RKIT_CHECK(m_actions.Allocate(totalSize, alignment, allocHandle, memChunk, offset));
		}

		uint8_t *placementMem = static_cast<uint8_t *>(memChunk->GetDataAtPosition(offset));
		uint8_t *paramsMem = placementMem + baseSize;

		ActionQueueItem *aqi = new (placementMem) ActionQueueItem();
		aqi->m_userdata = paramsMem;
		aqi->m_disposeCallback = ThreadFuncQueueUtils::DeleteParams<TParameters>;
		aqi->m_nextItem = nullptr;
		aqi->m_runCallback = ThreadFuncQueueUtils::RunThunk<TParameters, TFunction>;
		aqi->m_thisAlloc = allocHandle;

		paramsCallback(paramsMem, params);

		PostAQIToActionQueue(aqi);

		return ResultCode::kOK;
	}



	struct ThreadFuncQueueUtils
	{

		template<class TParameters>
		static void CopyConstructParams(void *userdata, void *source);

		template<class TParameters>
		static void MoveConstructParams(void *userdata, void *source);

		template<class TParameters>
		static void DeleteParams(void *userdata);

		template<class TParameters, Result(RenderWindow_Win32:: *TFunction)(TParameters &params)>
		static Result RunThunk(RenderWindow_Win32 &window, void *userdata);
	};

	void RenderWindow_Win32::KickThread()
	{
		if (m_hWnd)
		{
			MutexLock lock(*m_actionsMutex);
			if (m_isWaitingForActions && !m_firstItem)
			{
				m_isWaitingForActions = false;
				::PostMessageW(m_hWnd, kKickThreadMsg, 0, 0);
			}
		}
	}

	Result RenderWindow_Win32::ThreadFunc()
	{
		Result result = Result(ResultCode::kOK);

		while (utils::ResultIsOK(result))
		{
			ActionQueueItem *aqi = nullptr;
			{
				MutexLock lock(*m_actionsMutex);
				aqi = m_firstItem;
				if (aqi)
				{
					ActionQueueItem *nextItem = aqi->m_nextItem;
					m_firstItem = nextItem;
					if (!nextItem)
						m_lastItem = nullptr;

					m_isWaitingForActions = false;
				}
				else
				{
					if (m_wantsToExit)
						break;

					m_isWaitingForActions = true;
				}
			}

			if (aqi)
			{
				// Handle action
				result = aqi->m_runCallback(*this, aqi->m_userdata);

				aqi->m_disposeCallback(aqi->m_userdata);
				m_actions.Dispose(aqi->m_thisAlloc);
			}
			else
			{
				result = ThreadProcessWinMessages();
			}

			if (!utils::ResultIsOK(result))
				break;
		}

		{
			MutexLock lock(*m_actionsMutex);
			m_threadHasExited = true;
		}

		return result;
	}

	Result RenderWindow_Win32::ThreadProcessWinMessages()
	{
		while (!m_actionLoopWasKicked && utils::ResultIsOK(m_msgProcResult))
		{
			MSG msg;
			BOOL msgGetResult = GetMessageW(&msg, m_hWnd, 0, 0);

			if (msgGetResult == -1)
				return ResultCode::kOperationFailed;

			if (msgGetResult == 0)
			{
				// WM_QUIT
				return ResultCode::kNotYetImplemented;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}

		m_actionLoopWasKicked = false;

		return m_msgProcResult;
	}

	Result RenderWindow_Win32::SetupWindow()
	{
		m_windowStyle = WS_OVERLAPPEDWINDOW;

		PrimaryMonitorSearchData searchData = {};

		if (!EnumDisplayMonitors(nullptr, nullptr, FindPrimaryMonitor, reinterpret_cast<LPARAM>(&searchData)))
			return ResultCode::kOperationFailed;

		if (!searchData.m_haveAnyRect)
			return ResultCode::kOperationFailed;

		RECT monitorRect = searchData.m_monitorRect;

		LONG halfPointX = (monitorRect.left + monitorRect.right) / 2;
		LONG halfPointY = (monitorRect.top + monitorRect.bottom) / 2;

		LONG width = static_cast<LONG>(m_defaultWidth);
		LONG height = static_cast<LONG>(m_defaultHeight);

		if (width == 0)
			width = 640;
		if (height == 0)
			height = 480;

		RECT wr = {};
		wr.left = (monitorRect.left + monitorRect.right - width) / 2;
		wr.top = (monitorRect.top + monitorRect.bottom - height) / 2;
		wr.right = wr.left + width;
		wr.bottom = wr.top + height;

		HMENU menus = nullptr;

		if (!::AdjustWindowRect(&wr, m_windowStyle, (menus != nullptr)))
			return ResultCode::kOperationFailed;

		DWORD exStyle = 0;
		DWORD style = m_windowStyle;

		WindowCreateParams wcParams = {};
		wcParams.m_window = this;

		m_hWnd = CreateWindowExW(exStyle, DisplayManager_Win32::AtomToClassName(m_wcAtom), L"RKit Rendering Window", m_windowStyle, wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top, nullptr, menus, m_hInst, &wcParams);

		if (!m_hWnd)
			return ResultCode::kOperationFailed;

		ShowWindow(m_hWnd, SW_SHOWDEFAULT);

		return ResultCode::kOK;
	}

	Result RenderWindow_Win32::BecomeBorderlessFullScreen()
	{
		bool completed = false;
		uint32_t newWidth = 0;
		uint32_t newHeight = 0;

		ChangeDisplayModeParams cdmParams = {};
		cdmParams.m_callback = &RenderWindow_Win32::ThreadBecomeBorderlessFullScreen;
		cdmParams.m_completedFlag = &completed;
		cdmParams.m_signalEvent = m_changeDisplayModeEvent.Get();
		cdmParams.m_outWidth = &newWidth;
		cdmParams.m_outHeight = &newHeight;

		RKIT_CHECK((QueueAction<ChangeDisplayModeParams, &RenderWindow_Win32::Action_ChangeDisplayMode>(std::move(cdmParams))));

		m_changeDisplayModeEvent->Wait();

		if (!completed)
			return ResultCode::kOperationFailed;

		return ResultCode::kOK;
	}

	Result RenderWindow_Win32::ThreadBecomeBorderlessFullScreen()
	{
		RECT windowRect;
		if (!GetWindowRect(m_hWnd, &windowRect))
			return ResultCode::kOperationFailed;

		HMONITOR monitor = MonitorFromRect(&windowRect, MONITOR_DEFAULTTONULL);
		if (!monitor)
		{
			// If the window is off-screen, use the primary monitor
			monitor = MonitorFromRect(&windowRect, MONITOR_DEFAULTTOPRIMARY);
		}
		else
		{
			// Otherwise, use the nearest
			monitor = MonitorFromRect(&windowRect, MONITOR_DEFAULTTONEAREST);
		}

		if (!monitor)
			return ResultCode::kOperationFailed;	// No monitor?

		MONITORINFO monitorInfo;
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (!GetMonitorInfoW(monitor, &monitorInfo))
			return ResultCode::kOperationFailed;

		m_windowStyle = WS_POPUP;

		m_windowedModeRect = windowRect;
		SetWindowLongPtrW(m_hWnd, GWL_STYLE, WS_VISIBLE | m_windowStyle);

		if (!SetWindowPos(m_hWnd, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top, SWP_FRAMECHANGED))
		{
			rkit::log::Warning(u8"BecomeBorderlessFullScreen: SetWindowPos failed");
		}

		return ResultCode::kOK;
	}

	Result RenderWindow_Win32::Action_ChangeDisplayMode(ChangeDisplayModeParams &displayModeParams)
	{
		*displayModeParams.m_completedFlag = false;
		Result result = (this->*(displayModeParams.m_callback))();

		*displayModeParams.m_completedFlag = utils::ResultIsOK(result);

		if (utils::ResultIsOK(result))
		{
			RECT clRect = {};
			::GetClientRect(m_hWnd, &clRect);
			*displayModeParams.m_outWidth = static_cast<uint32_t>(clRect.right - clRect.left);
			*displayModeParams.m_outHeight = static_cast<uint32_t>(clRect.bottom - clRect.top);
		}

		displayModeParams.m_signalEvent->Signal();

		displayModeParams.m_signalEvent = nullptr;
		displayModeParams.m_completedFlag = nullptr;

		return result;
	}

	Result RenderWindow_Win32::Action_RunCustomTask(RunCustomTaskParams &customTaskParams)
	{
		customTaskParams.m_task(customTaskParams.m_userdata);
		return ResultCode::kOK;
	}

	Result RenderWindow_Win32::BecomeWindowed()
	{
		return ResultCode::kNotYetImplemented;
	}

	LRESULT RenderWindow_Win32::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
		case kKickThreadMsg:
			m_actionLoopWasKicked = true;
			return 0;
		default:
			return ::DefWindowProcW(hWnd, msg, wParam, lParam);
		}
	}

	LRESULT CALLBACK RenderWindow_Win32::StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		RenderWindow_Win32 *rwindow = nullptr;

		if (msg == WM_NCCREATE || msg == WM_CREATE)
		{
			const CREATESTRUCTW *createInfo = reinterpret_cast<const CREATESTRUCTW *>(lParam);

			WindowCreateParams *createParams = static_cast<WindowCreateParams *>(createInfo->lpCreateParams);

			rwindow = createParams->m_window;

			SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(rwindow));
		}
		else
			rwindow = reinterpret_cast<RenderWindow_Win32 *>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

		if (rwindow)
			return rwindow->WndProc(hWnd, msg, wParam, lParam);
		else
			return ::DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	RenderWindow_Win32::ThreadContext::ThreadContext(RenderWindow_Win32 &window)
		: m_window(window)
	{
	}

	Result RenderWindow_Win32::ThreadContext::Run()
	{
		Result result = m_window.SetupWindow();

		m_window.m_startEvent->Signal();

		if (utils::ResultIsOK(result))
		{
			result = m_window.ThreadFunc();
		}
		else
		{
			MutexLock lock(*m_window.m_actionsMutex);
			m_window.m_threadHasExited = true;
		}

		m_window.m_exitEvent->Signal();

		m_window.m_destroyWindowEvent->Wait();

		if (m_window.m_hWnd)
		{
			::DestroyWindow(m_window.m_hWnd);
			m_window.m_hWnd = nullptr;
		}

		m_window.m_exitEvent->Signal();

		return result;
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
		
		if (m_renderWCAtom)
			::UnregisterClassW(AtomToClassName(m_renderWCAtom), m_hInst);
	}

	Result DisplayManager_Win32::Initialize()
	{
		RKIT_CHECK(SplashWindow_Win32::RegisterWndClass(m_splashWCAtom, m_hInst));
		RKIT_CHECK(RenderWindow_Win32::RegisterWndClass(m_renderWCAtom, m_hInst));

		return ResultCode::kOK;
	}

	Result DisplayManager_Win32::CreateDisplay(UniquePtr<IDisplay> &display, DisplayMode displayMode)
	{
		if (displayMode == DisplayMode::kSplash)
			return CreateSplash(display, displayMode);
		else
			return CreateRenderWindow(display, displayMode, 0, 0);
	}

	Result DisplayManager_Win32::CreateDisplay(UniquePtr<IDisplay> &display, DisplayMode displayMode, uint32_t width, uint32_t height)
	{
		if (displayMode == DisplayMode::kSplash)
			return CreateSplash(display, displayMode);
		else
		{
			if (width < 160)
				width = 160;
			else if (width > 16384)
				width = 16384;

			if (height < 120)
				height = 120;
			else if (height > 16384)
				height = 16384;

			return CreateRenderWindow(display, displayMode, width, height);
		}
	}

	Result DisplayManager_Win32::CreateSplash(UniquePtr<IDisplay> &display, DisplayMode displayMode)
	{
		UniquePtr<SplashWindow_Win32> splashWindow;
		RKIT_CHECK(NewWithAlloc<SplashWindow_Win32>(splashWindow, m_alloc, m_alloc, m_hInst));

		RKIT_CHECK(splashWindow->Initialize(m_splashWCAtom));

		display = std::move(splashWindow);

		return ResultCode::kOK;
	}

	Result DisplayManager_Win32::CreateRenderWindow(UniquePtr<IDisplay> &display, DisplayMode displayMode, uint32_t width, uint32_t height)
	{
		UniquePtr<RenderWindow_Win32> renderWindow;
		RKIT_CHECK(NewWithAlloc<RenderWindow_Win32>(renderWindow, m_alloc, m_alloc, m_hInst));

		RKIT_CHECK(renderWindow->Initialize(GetDrivers().m_systemDriver, m_renderWCAtom, width, height));

		if (renderWindow->GetDisplayMode() != displayMode)
		{
			if (renderWindow->CanChangeToDisplayMode(displayMode))
			{
				RKIT_CHECK(renderWindow->ChangeToDisplayMode(displayMode));
			}
			else
			{
				return ResultCode::kOperationFailed;
			}
		}

		display = std::move(renderWindow);

		return ResultCode::kOK;
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
} } // rkit::render
