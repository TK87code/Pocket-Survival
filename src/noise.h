#ifndef NOISE_H
#define NOISE_H

/**
 * @brief  Generate hashed sudo random float that is unique to the two inputs and seed. 
 *
 * @param  x x coordinate
 * @param  y y coordinate
 * @param  seed seed number
 *
 * @return float 0.0 - 1.0
 */
float hash2d(int x, int y, int seed);

/**
 * @brief  Smooth interpolation using Quintic Hermite Curve
 *
 * @param  s The start of the path 
 * @param  e The end of the path
 * @param  t progress parameter(normalization factor) where 0 is the start of the path, and 1 is the end. 
 *
 * @return Smooth interpolated float value according to input.
 */
float smooth_interp(float s, float e, float t);

/**
 * @brief  Generate smooth interpolated 2d noise using value noise algorithm. 
 *
 * @param  x x position
 * @param  y y position
 * @param  seed A seed to generate hashed number
 * @param  scale the size of grids
 *
 * @return  noise value
 */
float value_noise_2d(float x, float y, int seed, float scale);

#endif //NOISE_H
