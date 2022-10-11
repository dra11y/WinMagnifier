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

#pragma once

#include <thread>

#include <dwmapi.h>
#include <magnification.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "magnification.lib")

#define VK_E 0x45
#define VK_Q 0x51
#define VK_T 0x54

struct Spring
{
	auto operator()(float target)
	{
		this->target = target;
		velocity += (target - current) * acceleration;
		velocity *= friction;
		return current += velocity;
	}

	operator float()
	{
		return current;
	}

private:
	float target = 1.0;
	float current = 1.0;
	float velocity = 0.0;
	float acceleration = 0.4f;
	float friction = 0.3f;
};

