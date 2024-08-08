#pragma once

#include <atomic>
#include <dwmapi.h>
#include <magnification.h>
#include <thread>

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
	float acceleration = 0.9f;
	float friction = 0.3f;
};

