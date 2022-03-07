#pragma once

#define cast(...) (__VA_ARGS__)
#define VK_E 0x45
#define VK_Q 0x51
#define VK_T 0x54

struct Spring
{
	operator float()
	{
		velocity += (target - current) * acceleration;
		velocity *= friction;
		return current += velocity;
	}

	auto reset(float destination)
	{
		current = target = destination;
		velocity = 0.0;
	}

	auto operator=(float destination)
	{
		target = destination;
	}

	auto operator-(float other)
	{
		return target - other;
	}

	auto operator+=(float other)
	{
		target += other;
	}

	auto operator-=(float other)
	{
		target -= other;
	}

	auto operator<=>(float other)
	{
		return target <=> other;
	}

private:
	float target = 1.0;
	float current = 1.0;
	float velocity = 0.0;
	float acceleration = 0.4f;
	float friction = 0.3f;
};

enum Modifiers
{
	NONE = 0,
	LWIN = 1,
	LCONTROL = 2,
	LALT = 4,
	LSHIFT = 8
};
