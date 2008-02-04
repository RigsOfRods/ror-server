// this is a very simple implementation of a Vector class!

#ifndef __Vector3_H__
#define __Vector3_H__

#include <cmath>

class Vector3
{
public:
	float x, y, z;

	Vector3() : x(0), y(0), z(0)
	{
	}

	Vector3(float _x, float _y, float _z): x(_x), y(_y), z(_z)
	{
	}

	float length()
	{
		return sqrt(x*x + y*y + z*z);
	}

	float distance(Vector3 v)
	{
		return (*this-v).length();
	}

	inline Vector3 operator - ( const Vector3& v ) const
	{
		return Vector3(x-v.x, y-v.y, z-v.z);
	}

	void toString(char *str)
	{
		sprintf(str, "%.4f,%.4f,%.4f", x, y, z);
	}
};

#endif
