struct CUserCmd
{
	char pad[0x8];
	int command_number;
	int tick_count;
	QAngle viewangles;
	float forwardmove;
	float sidemove;
	float upmove;
	int	buttons;
	byte impulse;
	int weaponselect;
	int weaponsubtype;
	int random_seed;
	short mousedx;
	short mousedy;
	bool hasbeenpredicted;
};

