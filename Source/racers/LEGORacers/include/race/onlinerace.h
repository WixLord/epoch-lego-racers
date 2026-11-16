#ifndef ONLINERACE_H
#define ONLINERACE_H

class Racer;
class RaceState;
class GolCamera;
class GolFontBase;
class GolD3DRenderDevice;

// Opt-in six-player prototype. No pointers or native game objects cross the wire.
namespace OnlineRace
{
bool Requested();
bool Active();
bool IsHost();
bool IsRemote(const Racer* p_racer);
unsigned Count();
unsigned GridSlot(unsigned index);
bool ExchangeBuilds(const unsigned char* p_local, unsigned char p_remote[5][0x22d]);
const char* Error();
void Attach(RaceState* p_race);
void Update();
bool Ready();
void DrawNames(GolCamera* p_camera, GolFontBase* p_font, GolD3DRenderDevice* p_renderer);
void Close();
} // namespace OnlineRace

#endif
