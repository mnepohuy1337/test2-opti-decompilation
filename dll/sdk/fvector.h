class FVector
{
public:
	float x, y, z;
	float Length2D() { return sqrt(x * x + y * y); }
	float Length() { return sqrt(x * x + y * y + z * z); };
};

