/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef BASE_MATH_H
#define BASE_MATH_H

#include <stdlib.h>
#include <random>

const float pi = 3.1415926535897932384626433f;

template <typename T>
inline T clamp(T val, T min, T max)
{
	if(val < min)
		return min;
	if(val > max)
		return max;
	return val;
}

inline float sign(float f)
{
	return f<0.0f?-1.0f:1.0f;
}

inline int round_truncate(float f)
{
	return (int)f;
}

inline int round_to_int(float f)
{
	if(f > 0)
		return (int)(f+0.5f);
	return (int)(f-0.5f);
}

template<typename T, typename TB>
inline T mix(const T a, const T b, TB amount)
{
	return a + (b-a)*amount;
}

inline float random_angle()
{
	return 2.0f * pi * (rand() / std::nextafter((float)RAND_MAX, std::numeric_limits<float>::max()));
}

float random_float();
bool random_prob(float f);
int random_int(int Min, int Max);
inline int random_int() { return (((rand() & 0xffff) << 16) | (rand() & 0xffff)) & 0x7FFFFFFF; }
int random_distribution(double* pProb, double* pProb2);
inline float frandom() { return rand()/(float)(RAND_MAX); }

// float to fixed
inline int f2fx(float v) { return (int)(v*(float)(1<<10)); }
inline float fx2f(int v) { return v*(1.0f/(1<<10)); }

inline int gcd(int a, int b)
{
	while(b != 0)
	{
		int c = a % b;
		a = b;
		b = c;
	}
	return a;
}

class fxp
{
	int value;
public:
	void set(int v) { value = v; }
	int get() const { return value; }
	fxp &operator = (int v) { value = v<<10; return *this; }
	fxp &operator = (float v) { value = (int)(v*(float)(1<<10)); return *this; }
	operator float() const { return value/(float)(1<<10); }
};

template <typename T> inline T min(T a, T b) { return a<b?a:b; }
template <typename T> inline T max(T a, T b) { return a>b?a:b; }
template <typename T> inline T mt_absolute(T a) { return a<T(0)?-a:a; }

template<typename T>
constexpr inline T minimum(T a, T b)
{
	return a < b ? a : b;
}

template<typename T>
constexpr inline T minimum(T a, T b, T c)
{
	return minimum(minimum(a, b), c);
}

template<typename T>
constexpr inline T maximum(T a, T b)
{
	return a > b ? a : b;
}

template<typename T>
constexpr inline T maximum(T a, T b, T c)
{
	return maximum(maximum(a, b), c);
}

template<typename T>
constexpr inline T absolute(T a)
{
	return a < T(0) ? -a : a;
}

#endif // BASE_MATH_H
