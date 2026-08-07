#include "noise.h"
#include "defines.h"

// Return hashed sudo random float 0.0 - 1.0
float hash2d(int x, int y, int seed) {
	uint32_t hash = (uint32_t)x * 374761393 + (uint32_t)y * 668265263 + (uint32_t)seed * 3266489917;
	hash = (hash ^ (hash >> 13)) * 1274126177;

	return (float)(hash & 0x00ffffff) / (float)0x00ffffff;
};

// Smooth interpolation using Quintic Hermite curve
// [REF] https://www.rose-hulman.edu/~finn/CCLI/Notes/day09.pdf
float smooth_interp(float s, float e, float t) {
    float f = t * t * t * (t * (6.0f * t - 15.0f) + 10); 
    return s + f * (e - s);
}

// Generate smooth 2d noise using value noise algorithm
// scale: the size of grids
float value_noise_2d(float x, float y, int seed, float scale)
{
	// Normalized
	float nx = x / scale;
	float ny = y / scale;
	
	// Grid Index
	int ix = (int)nx;
	int iy = (int)ny;
	
	// Ratio
	float tx = nx - (float)ix;
	float ty = ny - (float)iy;

	float h_top_left = hash2d(ix, iy, seed);
	float h_top_right = hash2d(ix + 1, iy, seed);
	float h_bot_left = hash2d(ix, iy + 1, seed);
	float h_bot_right = hash2d(ix + 1, iy + 1, seed);

	float top_blend = smooth_interp(h_top_left, h_top_right, tx);
	float bot_blend = smooth_interp(h_bot_left, h_bot_right, tx);

	return smooth_interp(top_blend, bot_blend, ty);
}
