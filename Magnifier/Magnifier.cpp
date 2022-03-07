#include <dwmapi.h>
#include <magnification.h>

#include <iostream>
#include <thread>
#include <format>

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
					auto delta = MouseZTweener - 0.25f;

					if (delta >= 1.0)
					{
						MouseZTweener = delta;
					}
				}

				return 1;
			}
		}
	}

	return CallNextHookEx(MouseHook, code, wParam, lParam);
}

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
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

		if (ModDown && wParam == WM_KEYUP)
		{
			switch (key)
			{
			case VK_Q:
				exit(0);
				break;
			case VK_E:
			{
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
				break;
			}
			}
		}
	}

	return CallNextHookEx(KeyboardHook, code, wParam, lParam);
}

int main()
{
	FreeConsole();

	std::thread MagnifierThread([]()
		{
			if (!MagInitialize())
			{
				puts("Cannot initialize magnifier");
				return 1;
			}

			while (true)
			{
				auto z = cast(float)MouseZTweener;
				auto x = cast(int)(MouseXTweener * (1.0 - (1.0 / z)));
				auto y = cast(int)(MouseYTweener * (1.0 - (1.0 / z)));

				MagSetFullscreenTransform(z, x, y);

				DwmFlush();
			}

			MagUninitialize();
		});
	MouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, NULL, NULL);
	KeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, NULL);

	while (GetMessage(&CurrentMessage, NULL, 0, 0))
	{
		TranslateMessage(&CurrentMessage);
		DispatchMessage(&CurrentMessage);
	}

	UnhookWindowsHookEx(MouseHook);
	UnhookWindowsHookEx(KeyboardHook);
	return 0;
}

