class CEngineClient
{
public:
	/*0*/	virtual void* GetIntersectingSurfaces(void*) = 0;
	/*1*/	virtual void* GetLightForPoint(void*) = 0;
	/*2*/	virtual void* TraceLineMaterialAndLighting(void*) = 0;
	/*3*/	virtual void* ParseFile(char const*, char*, int) = 0;
	/*4*/	virtual void* CopyLocalFile(char const*, char const*) = 0;
	/*5*/	virtual void* GetScreenSize(int&, int&) = 0;
	/*6*/	virtual void ServerCmd(char const*, bool) = 0;
	/*7*/	virtual void ClientCmd(char const*) = 0;
	/*8*/	virtual bool GetPlayerInfo(int, void*) = 0;
	/*9*/	virtual int GetPlayerForUserID(int) = 0;
	/*10*/	virtual void* TextMessageGet(char const*) = 0;
	/*11*/	virtual bool Con_IsVisible(void) = 0;
	/*12*/	virtual int GetLocalPlayer(void) = 0;
};
