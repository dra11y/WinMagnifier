#include "Magnifier.h"

HHOOK MouseHook = nullptr;
HHOOK KeyboardHook = nullptr;

double MouseZ = 1;

// Global variables to track LWin state
std::atomic<bool> isLWinPressed{ false };
std::atomic<bool> otherKeyPressedWithLWin{ false };
std::chrono::time_point<std::chrono::steady_clock> lWinPressTime;

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
				// return 1; // Trap the LWin key press
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



int APIENTRY WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd
)
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#ifdef _DEBUG
	AllocConsole();
	FILE* fDummy;
	freopen_s(&fDummy, "CONOUT$", "w", stdout);
	freopen_s(&fDummy, "CONOUT$", "w", stderr);
	freopen_s(&fDummy, "CONIN$", "r", stdin);
#endif
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

	while (GetMessage(&CurrentMessage, 0, 0, 0))
	{
		//if (GetAsyncKeyState(VK_LWIN) and GetAsyncKeyState(VK_LSHIFT))
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
			auto z = SmoothMouseZ(MouseZ) - 1.0f <= 0.005 ? 1.0f : SmoothMouseZ;
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

	return 0;
}
