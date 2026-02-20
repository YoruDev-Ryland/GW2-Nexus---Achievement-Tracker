#pragma once
#include <windows.h>
#include "Nexus.h"
#include <string>

namespace Mumble {
    struct Vector3 { float X, Y, Z; };
    struct Context {
        uint8_t  ServerAddress[28];
        uint32_t MapId;
        uint32_t MapType;
        uint32_t ShardId;
        uint32_t Instance;
        uint32_t BuildId;
        uint32_t UiState;
        uint16_t CompassWidth;
        uint16_t CompassHeight;
        float    CompassRotation;
        float    PlayerX;
        float    PlayerY;
        uint32_t MapCenterX;
        uint32_t MapCenterY;
        float    MapScale;
        uint32_t ProcessId;
        uint8_t  MountIndex;
    };
    struct LinkedMem {
        uint32_t UIVersion, UITick;
        Vector3  AvatarPosition;
        Vector3  AvatarFront;
        Vector3  AvatarTop;
        wchar_t  Name[256];
        Vector3  CameraPosition;
        Vector3  CameraFront;
        Vector3  CameraTop;
        wchar_t  Identity[256];
        uint32_t ContextLength;
        Context  Context;
        wchar_t  Description[2048];
    };
    struct Identity {
        char Name[20];
        uint8_t Profession;
        uint32_t Spec;
        uint32_t Race;
        uint32_t MapID;
        uint32_t WorldID;
        uint32_t TeamColorID;
        bool Commander;
        float FOV;
        uint8_t UISize;
    };
}

extern AddonAPI_t*        APIDefs;
extern HMODULE            Self;
extern Mumble::LinkedMem* MumbleLink;
extern Mumble::Identity*  MumbleIdent;

inline void* GetTexResource(const std::string& id)
{
    if (!APIDefs || id.empty()) return nullptr;
    Texture_t* t = APIDefs->Textures_Get(id.c_str());
    return (t && t->Resource) ? t->Resource : nullptr;
}

bool IsInGame();
