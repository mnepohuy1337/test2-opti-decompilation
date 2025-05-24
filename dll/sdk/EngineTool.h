class CEngineTool
{
public:
    virtual ~CEngineTool() = 0; // Destructor

    virtual int GetToolCount() const = 0; // CEngineTool::GetToolCount(void)
    virtual const char* GetToolName(int index) const = 0; // CEngineTool::GetToolName(int)
    virtual void SwitchToTool(int index) = 0; // CEngineTool::SwitchToTool(int)
    virtual bool IsTopmostTool(const void* toolSystem) const = 0; // CEngineTool::IsTopmostTool(IToolSystem const*)
    virtual void* GetToolSystem(int index) const = 0; // CEngineTool::GetToolSystem(int)
    virtual CEngineTool* GetTopmostTool() = 0; // CEngineTool::GetTopmostTool(void)
    virtual void ShowCursor(bool show) = 0; // CEngineTool::ShowCursor(bool)
    virtual bool IsCursorVisible() const = 0; // CEngineTool::IsCursorVisible(void)
    virtual void* GetServerFactory(void* (*&func)(const char*, int*)) = 0; // CEngineTool::GetServerFactory(void* (*&)(char const*, int*))
    virtual void* GetClientFactory(void* (*&func)(const char*, int*)) = 0; // CEngineTool::GetClientFactory(void* (*&)(char const*, int*))
    virtual float GetSoundDuration(const char* soundName) const = 0; // CEngineTool::GetSoundDuration(char const*)
    virtual bool IsSoundStillPlaying(int soundID) const = 0; // CEngineTool::IsSoundStillPlaying(int)
    virtual void StartSound(int soundID, bool param1, int param2, int param3, const char* soundName, float volume, void* soundLevel, const Vector& position1, const Vector& position2, int param4, int param5, bool param6, float param7, int param8) = 0; // CEngineTool::StartSound(int, bool, int, int, char const*, float, soundlevel_t, Vector const&, Vector const&, int, int, bool, float, int)
    virtual void StopSoundByGuid(int soundID) = 0; // CEngineTool::StopSoundByGuid(int)
    virtual float GetSoundDuration(int soundID) const = 0; // CEngineTool::GetSoundDuration(int)
    virtual bool IsLoopingSound(int soundID) const = 0; // CEngineTool::IsLoopingSound(int)
    virtual void ReloadSound(const char* soundName) = 0; // CEngineTool::ReloadSound(char const*)
    virtual void StopAllSounds() = 0; // CEngineTool::StopAllSounds(void)
    virtual void GetMono16Samples(const char* soundName, void*) = 0; // CEngineTool::GetMono16Samples(char const*, CUtlVector<short, CUtlMemory<short, int>&)
    virtual void SetAudioState(const void* audioState) = 0; // CEngineTool::SetAudioState(AudioState_t const&)
    virtual void Command(const char* command) = 0; // CEngineTool::Command(char const*)
    virtual void Execute() = 0; // CEngineTool::Execute(void)
    virtual const char* GetCurrentMap() const = 0; // CEngineTool::GetCurrentMap(void)
    virtual void ChangeToMap(const char* mapName) = 0; // CEngineTool::ChangeToMap(char const*)
    virtual bool IsMapValid(const char* mapName) const = 0; // CEngineTool::IsMapValid(char const*)
    virtual void RenderView(void* viewSetup, int param1, int param2) = 0; // CEngineTool::RenderView(CViewSetup&, int, int)
    virtual bool IsInGame() const = 0; // CEngineTool::IsInGame(void)
    virtual bool IsConnected() const = 0; // CEngineTool::IsConnected(void)
    virtual int GetMaxClients() const = 0; // CEngineTool::GetMaxClients(void)
    virtual bool IsGamePaused() const = 0; // CEngineTool::IsGamePaused(void)
    virtual void SetGamePaused2(bool paused) = 0; // CEngineTool::SetGamePaused(bool)
    virtual float GetTimescale() const = 0; // CEngineTool::GetTimescale(void)
    virtual void SetTimescale(float timescale) = 0; // CEngineTool::SetTimescale(float)
    virtual float GetRealTime() const = 0; // CEngineTool::GetRealTime(void)
    virtual float GetHostFrameTime() const = 0; // CEngineTool::GetRealFrameTime(void)
    virtual float HostTime() const = 0;
    virtual float* HostTick() const = 0;
};

