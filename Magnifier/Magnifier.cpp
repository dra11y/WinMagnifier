#include <dwmapi.h>
#include <magnification.h>

#include <iostream>
#include <thread>

#include "Magnifier.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "magnification.lib")

HHOOK MouseHook;
HHOOK KeyboardHook;
MSG CurrentMessage;

Spring MouseZTweener;
Spring MouseXTweener;
Spring MouseYTweener;

int MouseX;
int MouseY;
bool ModDown = false;
int Modifiers = Modifiers::NONE;
float State = 1.0f;
bool Running = true;

LRESULT CALLBACK MouseProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (ModDown || MouseZTweener > 1.0f)
	{
		if (auto mData = cast(MSLLHOOKSTRUCT*)lParam)
		{
			MouseX = mData->pt.x;
			MouseY = mData->pt.y;

			MouseXTweener = cast(float)MouseX;
			MouseYTweener = cast(float)MouseY;

			if (ModDown && wParam == WM_MOUSEWHEEL)
			{
				if (HIWORD(mData->mouseData) == 120)
				{
					MouseZTweener += 0.25f;
				}
				else
				{
					MouseZTweener = max(1.0f, MouseZTweener - 0.25f);
				}

				return 1;
			}
		}
	}

	return CallNextHookEx(MouseHook, code, wParam, lParam);
}

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	// TODO(pushqrdx) Refactor this shit out, also keyup/down handling is so messy
	if (auto kData = cast(KBDLLHOOKSTRUCT*)lParam)
	{
		auto key = kData->vkCode;

		if (wParam == WM_KEYUP && (key == VK_LWIN || key == VK_LSHIFT))
		{
			Modifiers = 0x0;
			ModDown = false;
			return CallNextHookEx(KeyboardHook, code, wParam, lParam);
		}

		switch (key)
		{
		case VK_LWIN:
			Modifiers |= Modifiers::LWIN;
			break;
		case VK_LMENU:
			Modifiers |= Modifiers::LALT;
			break;
		case VK_LCONTROL:
			Modifiers |= Modifiers::LCONTROL;
			break;
		case VK_LSHIFT:
			Modifiers |= Modifiers::LSHIFT;
			break;
		}

		ModDown = (Modifiers ^ (Modifiers::LWIN | Modifiers::LSHIFT)) == 0;

		if (ModDown)
		{
			switch (key)
			{
			case VK_Q:
				Running = false;
				return 1;
			case VK_E:
			{
				if (wParam == WM_KEYUP)
					if (MouseZTweener > 1.0f)
					{
						State = MouseZTweener;
						MouseZTweener = 1.0f;
					}
					else
					{
						POINT pt;
						if (GetCursorPos(&pt))
						{
							MouseX = pt.x;
							MouseY = pt.y;
							MouseXTweener.reset(cast(float)MouseX);
							MouseYTweener.reset(cast(float)MouseY);
						}

						MouseZTweener = State;
					}
				return 1;
			}
			}
		}
	}

	return CallNextHookEx(KeyboardHook, code, wParam, lParam);
}

int main()
{
	FreeConsole();

	DWORD MainThreadID = GetCurrentThreadId();
	std::thread MagnifierThread([&]()
		{
			if (!MagInitialize())
			{
				puts("Cannot initialize magnifier");
				return 1;
			}

			auto sw = GetSystemMetrics(SM_CXSCREEN);
			auto sh = GetSystemMetrics(SM_CYSCREEN);
			auto clamp = [](auto value, auto min, auto max)
			{
				return max(min, min(value, max));
			};

			while (Running)
			{
				auto x = cast(float)MouseXTweener;
				auto y = cast(float)MouseYTweener;
				auto z = cast(float)MouseZTweener;

				auto f = (1.0 - (1.0 / z));
				auto pw = (f * sw);
				auto ph = (f * sh);
				auto w = (sw - pw);
				auto h = (sh - ph);

				auto cx = cast(int)clamp(x - (w / 2), 0, pw);
				auto cy = cast(int)clamp(y - (h / 2), 0, ph);

				MagSetFullscreenTransform(z, cx, cy);

				DwmFlush();
			}

			MagUninitialize();
			PostThreadMessage(MainThreadID, WM_QUIT, 0, 0);
			return 0;
		});
	MouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, NULL, NULL);
	KeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, NULL);
	MagnifierThread.detach();

	while (GetMessage(&CurrentMessage, NULL, 0, 0))
	{
		TranslateMessage(&CurrentMessage);
		DispatchMessage(&CurrentMessage);
	}

	UnhookWindowsHookEx(MouseHook);
	UnhookWindowsHookEx(KeyboardHook);
	return 0;
}

