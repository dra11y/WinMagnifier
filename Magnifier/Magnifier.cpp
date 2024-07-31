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

static auto Clamp(auto value, auto min, auto max)
{
	return max(min, min(value, max));
}

static auto GetCursorPosition()
{
	POINT pnt;
	GetCursorPos(&pnt);
	return pnt;
}

HHOOK MouseHook = nullptr;
double MouseZ = 1;

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

auto main() -> int
{
#ifdef NDEBUG
	FreeConsole();
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
	MagUninitialize();

	return 0;
}
