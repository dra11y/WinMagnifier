/*
 * This file is part of the Magnifier distribution (https://github.com/pushqrdx/Magnifier).
 * Copyright (c) 2022 Ahmed Tarek.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Magnifier.h"

auto Clamp(auto value, auto min, auto max)
{
	return max(min, min(value, max));
}

auto GetCursorPosition()
{
	POINT pnt;
	GetCursorPos(&pnt);
	return pnt;
}

HHOOK MouseHook = nullptr;
HHOOK KeyboardHook = nullptr;

int Modifiers = Modifiers::NONE;
int CurrentKey = 0x0;

int MouseZ = 1;

auto CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (auto data = (MSLLHOOKSTRUCT*)lParam)
	{
		if (wParam == WM_MOUSEWHEEL)
		{
			MouseZ = Clamp(MouseZ + (HIWORD(data->mouseData) == 120 ? 1 : -1), 1, 20);
			return (LRESULT)0x1;
		}
	}

	return CallNextHookEx(MouseHook, code, wParam, lParam);
}

auto CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (auto data = (KBDLLHOOKSTRUCT*)lParam)
	{
		CurrentKey = data->vkCode;

		if (wParam == WM_KEYUP && (CurrentKey == VK_LWIN || CurrentKey == VK_LSHIFT))
		{
			Modifiers = 0x0;
			CurrentKey = 0x0;
			return CallNextHookEx(KeyboardHook, code, wParam, lParam);
		}

		switch (CurrentKey)
		{
		case VK_LWIN:
			Modifiers |= Modifiers::LWIN;
			break;
			//case VK_LMENU:
			//	Modifiers |= Modifiers::LALT;
			//	break;
			//case VK_LCONTROL:
			//	Modifiers |= Modifiers::LCONTROL;
			//	break;
		case VK_LSHIFT:
			Modifiers |= Modifiers::LSHIFT;
			break;
		}
	}

	return CallNextHookEx(KeyboardHook, code, wParam, lParam);
}

auto main() -> int
{
	// FreeConsole();
	if (!MagInitialize())
	{
		puts("Cannot initialize magnifier");
		return 1;
	}

	auto MainThreadID = GetCurrentThreadId();
	auto ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	auto ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
	auto SmoothMouseX = Spring();
	auto SmoothMouseY = Spring();
	auto SmoothMouseZ = Spring();
	auto CurrentMessage = MSG();
	auto VSync = std::thread([&] {
		while (true)
		{
			PostThreadMessage(MainThreadID, 0x0420, 0, 0);
			DwmFlush();
		}

		return 0;
		});
	VSync.detach();

	KeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, NULL, NULL);

	while (GetMessage(&CurrentMessage, 0, 0, 0))
	{
		if ((Modifiers ^ (Modifiers::LWIN | Modifiers::LSHIFT)) == 0)
		{
			switch (CurrentKey)
			{
			case VK_Q: {
				PostThreadMessage(MainThreadID, WM_QUIT, 0, 0);
				break;
			}
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

		// TODO(pushqrdx): Avoid doing this work on each loop by detecting idle state.
		auto [MouseX, MouseY] = GetCursorPosition();
		auto z = SmoothMouseZ(MouseZ);
		auto f = (1.0 - (1.0 / z));
		auto pw = (f * ScreenWidth);
		auto ph = (f * ScreenHeight);
		auto w = (ScreenWidth - pw);
		auto h = (ScreenHeight - ph);

		MagSetFullscreenTransform(z, Clamp(SmoothMouseX(MouseX) - (w / 2), 0, pw), Clamp(SmoothMouseY(MouseY) - (h / 2), 0, ph));

		TranslateMessage(&CurrentMessage);
		DispatchMessage(&CurrentMessage);
	}

	UnhookWindowsHookEx(KeyboardHook);
	if (MouseHook)
		UnhookWindowsHookEx(MouseHook);
	MagUninitialize();

	return 0;
}
