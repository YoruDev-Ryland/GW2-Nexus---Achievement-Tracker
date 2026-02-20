#include "Shared.h"

AddonAPI_t*        APIDefs    = nullptr;
HMODULE            Self       = nullptr;
Mumble::LinkedMem* MumbleLink  = nullptr;
Mumble::Identity*  MumbleIdent = nullptr;

bool IsInGame()
{
    return MumbleLink && MumbleLink->Context.MapId != 0;
}
