#include "Magnifier.h"

#include "resource.h"

HHOOK MouseHook = nullptr;
HHOOK KeyboardHook = nullptr;

double MouseZ = 1;

// Global variables to track LWin state
std::atomic<bool> isLWinPressed{ false };
std::atomic<bool> otherKeyPressedWithLWin{ false };
std::chrono::time_point<std::chrono::steady_clock> lWinPressTime;

// Tray icon
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001

NOTIFYICONDATA nid;
HMENU hMenu;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_TRAYICON:
		if (lParam == WM_RBUTTONUP)
		{
			POINT p;
			GetCursorPos(&p);
			SetForegroundWindow(hwnd);
			TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, p.x, p.y, 0, hwnd, NULL);
			PostMessage(hwnd, WM_NULL, 0, 0);
		}
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == ID_TRAY_EXIT)
		{
			PostQuitMessage(0);
		}
		break;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

HWND CreateHiddenWindow(HINSTANCE hInstance)
{
	WNDCLASSEX wc = { 0 };

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"HiddenWindowClass";

	RegisterClassEx(&wc);

	return CreateWindowEx(
		0,
		L"HiddenWindowClass",
		L"Hidden Window",
		0, 0, 0, 0, 0,
		HWND_MESSAGE, // Message-only window
		NULL,
		hInstance,
		NULL);
}

static auto Clamp(auto value, auto min, auto max)
{
	return max(min, min(value, max));
}

LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (code >= 0)
	{
		auto key = ((KBDLLHOOKSTRUCT*)lParam)->vkCode;
		if (wParam == WM_KEYDOWN)
		{
			if (key == VK_LWIN)
			{
				isLWinPressed = true;
				otherKeyPressedWithLWin = false;
				lWinPressTime = std::chrono::steady_clock::now();
			}
			else if (isLWinPressed)
			{
				otherKeyPressedWithLWin = true;
			}

			if ((GetAsyncKeyState(VK_LWIN) & 0x8000) && isLWinPressed) // Check if Left Win key is pressed
			{
				if (key == VK_OEM_PLUS)
				{
					MouseZ *= 1.25;
					MouseZ = Clamp(MouseZ, 1.0, 20.0);
					return 1; // Trap the key
				}
				else if (key == VK_OEM_MINUS)
				{
					MouseZ /= 1.25;
					MouseZ = Clamp(MouseZ, 1.0, 20.0);
					return 1; // Trap the key
				}
			}
		}
		else if (wParam == WM_KEYUP)
		{
			if (key == VK_LWIN)
			{
				if (isLWinPressed && !otherKeyPressedWithLWin)
				{
					auto duration = std::chrono::steady_clock::now() - lWinPressTime;
					if (duration < std::chrono::milliseconds(500))
					{
						// If LWin was pressed and released quickly without any other key press, trap it
						isLWinPressed = false;

						// Simulate left control key press
						keybd_event(VK_LCONTROL, 0, 0, 0);

						// Simulate left control key release
						keybd_event(VK_LCONTROL, 0, KEYEVENTF_KEYUP, 0);
					}
				}
				isLWinPressed = false;
			}
		}
	}

	return CallNextHookEx(KeyboardHook, code, wParam, lParam);
}

static auto GetCursorPosition()
{
	POINT pnt;
	GetCursorPos(&pnt);
	return pnt;
}

static auto CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (auto data = (MSLLHOOKSTRUCT*)lParam)
	{
		if (wParam == WM_MOUSEWHEEL)
		{
			auto isOut = HIWORD(data->mouseData) == 120;
			if (isOut) {
				MouseZ /= 1.25;
			} else {
				MouseZ *= 1.25;
			}
			MouseZ = Clamp(MouseZ, 1, 20);
			return (LRESULT)0x1;
		}
	}

	return CallNextHookEx(MouseHook, code, wParam, lParam);
}

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
#ifdef _DEBUG
	AllocConsole();
	FILE* fDummy;
	freopen_s(&fDummy, "CONOUT$", "w", stdout);
	freopen_s(&fDummy, "CONOUT$", "w", stderr);
	freopen_s(&fDummy, "CONIN$", "r", stdin);
#endif

	// Use mutex to ensure only one concurrent instance
	const char* mutexName = "MagnifierMutex";
	HANDLE hMutex = CreateMutexA(NULL, TRUE, mutexName);
	if (hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS)
	{
		if (hMutex)
		{
			CloseHandle(hMutex);
		}
		return 0;
	}

	if (not MagInitialize() or not SetProcessDPIAware())
	{
		puts("Cannot initialize Magnifier");
		return 1;
	}

	auto MainThreadID = GetCurrentThreadId();
	auto ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	auto ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
	auto SmoothMouseZ = Spring();
	auto CurrentMessage = MSG();
	std::thread([&] {
		while (true)
		{
			PostThreadMessage(MainThreadID, 0x0420, 0, 0);
			DwmFlush();
		}

		return 0;
	}).detach();

	KeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, NULL, NULL);

	// Create a hidden window for tray icon interaction
	HWND hHiddenWnd = CreateHiddenWindow(hInstance);

	if (!hHiddenWnd)
	{
		puts("Cannot create hidden window");
		return 1;
	}

	// Initialize the NOTIFYICONDATA structure
	ZeroMemory(&nid, sizeof(nid));
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hHiddenWnd;  // Correctly set the window handle here
	nid.uID = 1;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON;
	nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wcsncpy_s(nid.szTip, sizeof(nid.szTip) / sizeof(WCHAR), L"Magnifier App", _TRUNCATE);

	// Create the tray menu
	hMenu = CreatePopupMenu();
	AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Quit");

	// Display the tray icon
	Shell_NotifyIcon(NIM_ADD, &nid);

	while (GetMessage(&CurrentMessage, 0, 0, 0))
	{
		if (CurrentMessage.lParam == WM_RBUTTONUP)  // Right-click event
		{
			POINT p;
			GetCursorPos(&p);
			SetForegroundWindow(hHiddenWnd);  // Bring the hidden window to the foreground
			TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, p.x, p.y, 0, hHiddenWnd, NULL);
		}
		else if (CurrentMessage.message == WM_COMMAND)
		{
			if (LOWORD(CurrentMessage.wParam) == ID_TRAY_EXIT)
			{
				PostQuitMessage(0);
			}
		}

		if (GetAsyncKeyState(VK_LWIN))
			{
			if (GetAsyncKeyState(VK_Q) < 0) 
			{
				PostThreadMessage(MainThreadID, WM_QUIT, 0, 0);
			}

			if (not MouseHook)
			{
				MouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, NULL, NULL);
			}
		}

		else
		{
			if (MouseHook)
			{
				UnhookWindowsHookEx(MouseHook);
				MouseHook = nullptr;
			}
		}

		if (MouseZ > 1 or SmoothMouseZ - MouseZ > 0.005)
		{
			auto [MouseX, MouseY] = GetCursorPosition();
			//auto z = SmoothMouseZ(MouseZ) - 1.0f <= 0.005 ? 1.0f : SmoothMouseZ;
			auto z = (static_cast<float>(SmoothMouseZ(MouseZ)) - 1.0f <= 0.005f) ? 1.0f : static_cast<float>(SmoothMouseZ);
			printf("z: %f\n", z);
			auto f = (1.0 - (1.0 / z));
			auto pw = (f * ScreenWidth);
			auto ph = (f * ScreenHeight);
			auto w = (ScreenWidth - pw);
			auto h = (ScreenHeight - ph);

			float mouseRelX = float(MouseX) / float(ScreenWidth);
			float mouseRelY = float(MouseY) / float(ScreenHeight);

			auto x = int(Clamp(MouseX - (w * mouseRelX), 0, pw));
			auto y = int(Clamp(MouseY - (h * mouseRelY), 0, ph));

			MagSetFullscreenTransform(z, x, y);
		}

		TranslateMessage(&CurrentMessage);
		DispatchMessage(&CurrentMessage);
	}

	if (MouseHook)
		UnhookWindowsHookEx(MouseHook);

	if (KeyboardHook)
		UnhookWindowsHookEx(KeyboardHook);

	MagUninitialize();

	Shell_NotifyIcon(NIM_DELETE, &nid);
	DestroyMenu(hMenu);

	if (hMutex)
	{
		ReleaseMutex(hMutex);
		CloseHandle(hMutex);
	}

	return 0;
}
