// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
#ifndef RT_LIGHT_PROPOSAL_GLSL
#define RT_LIGHT_PROPOSAL_GLSL

// The bounded cell list is an efficient proposal, not the set of lights
// that exists. A weighted global component gives every emitter support on
// both sides of a cell boundary. The CDF and its quantized probabilities
// follow the cells in the same immutable buffer.
uint rt_global_light_offset()
{
	return u_frame.lightGridDims.x * u_frame.lightGridDims.y * u_frame.lightGridDims.z;
}

float rt_global_light_pdf(uint index)
{
	return index < u_frame.counts.w ? uintBitsToFloat(u_lightGrid.cells[rt_global_light_offset() + index].y) : 0.0;
}

uint rt_global_light_pick(float u)
{
	uint lo = 0u, hi = u_frame.counts.w;
	uint offset = rt_global_light_offset();
	while (lo + 1u < hi) {
		uint mid = (lo + hi) / 2u;
		if (u < uintBitsToFloat(u_lightGrid.cells[offset + mid - 1u].x)) hi = mid;
		else lo = mid;
	}
	return lo;
}

uint rt_local_light_slot(uvec2 cell, uint index)
{
	if (index >= u_frame.counts.w && index < u_frame.counts.w + u_frame.flags.w) {
		return cell.y + index - u_frame.counts.w;
	}
	uint lo = 0u, hi = cell.y;
	while (lo < hi) {
		uint mid = (lo + hi) / 2u;
		uint id = u_lightIndices.indices[cell.x + mid];
		if (id < index) lo = mid + 1u;
		else hi = mid;
	}
	return lo < cell.y && u_lightIndices.indices[cell.x + lo] == index ? lo : ~0u;
}

float rt_light_proposal_pdf(uvec2 cell, uint total, float globalShare, uint index)
{
	float local = total > 0u && rt_local_light_slot(cell, index) != ~0u ? 1.0 / float(total) : 0.0;
	return globalShare * rt_global_light_pdf(index) + (1.0 - globalShare) * local;
}

#endif
