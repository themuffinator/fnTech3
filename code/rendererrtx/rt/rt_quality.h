/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef RTX_RT_QUALITY_H
#define RTX_RT_QUALITY_H

/* The controller is applied only after a cooldown and fresh timestamp data.
 * It controls diffuse bounce density, never geometry or direct visibility. */
static int RT_BudgetDivisor( int current, int minimum, float milliseconds, float budget )
{
	if ( !( budget > 0.0f ) || !( milliseconds > 0.0f ) ) return minimum;
	if ( milliseconds > budget * 1.15f && current < 4 ) return current + 1;
	if ( milliseconds < budget * 0.70f && current > minimum ) return current - 1;
	return current;
}

#endif
