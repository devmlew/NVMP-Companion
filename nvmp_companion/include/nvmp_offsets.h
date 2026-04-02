#pragma once
#include <cstdint>

// ============================================================
// NV:MP client.dll offsets (version 8.1)
// Derived from reverse engineering
// ============================================================

namespace nvmp {

// client.dll image base (default, verify at runtime)
constexpr uintptr_t CLIENT_DLL_BASE = 0x10000000;

// ---- Network Reference Struct Offsets ----
// These are offsets within NV:MP's internal network reference objects
constexpr uint32_t NETREF_CLASS_NAME     = 0x04;   // char* - class name string pointer
constexpr uint32_t NETREF_FLAGS          = 0xF4;   // byte - internal flags (bit 0x01=PVS, 0x10=synced)
constexpr uint32_t NETREF_LOCALITY       = 0x1AE;  // byte - 0=ghost/non-local, 1=local/owned

// ---- Actor Component Offsets ----
// From NetActorComponent (accessed via actor component pointer)
constexpr uint32_t COMPONENT_ENCOUNTER_MGR = 0xC0;  // pointer to encounter manager sub-object

// ---- Encounter Manager Offsets ----
// Flags at [encounter_mgr + type_index + 0x2D]
constexpr uint32_t ENC_FLAG_BASE         = 0x2D;   // base offset for encounter type flags
// encounter_mgr + 0x2D = EncounterActors (bool)
// encounter_mgr + 0x2E = EncounterReference (bool)
// encounter_mgr + 0x2F = EncounterDoor (bool)

// ---- Encounter Types ----
constexpr uint32_t ENC_TYPE_ACTORS       = 0;
constexpr uint32_t ENC_TYPE_REFERENCE    = 1;
constexpr uint32_t ENC_TYPE_DOOR         = 2;

// ---- Class Name String VAs (in client.dll .rdata) ----
// These are absolute VAs - subtract client.dll base to get RVA
constexpr uintptr_t STR_GAME_NET_CHARACTER = 0x10335058; // "GameNetPlayerClient"
// Note: server uses "GameNetCharacter" at 0x0064CB10

// ---- FNV Engine Offsets (from NVSE source) ----
// These are offsets in FalloutNV.exe's Actor class
namespace fnv {
    constexpr uint32_t ACTOR_BASE_PROCESS    = 0x68;   // Actor + 0x68 -> BaseProcess*
    constexpr uint32_t PROCESS_CACHED_VALUES = 0x2C;   // BaseProcess + 0x2C -> CachedValues*
    constexpr uint32_t CACHED_FLAGS          = 0x44;   // CachedValues + 0x44 -> flags DWORD
    constexpr uint32_t GHOST_BIT             = 0x10000000; // bit 28 in cached flags

    // Global player pointer (in FalloutNV.exe address space)
    constexpr uintptr_t G_THE_PLAYER         = 0x011DEA3C;

    // TESObjectREFR offsets
    constexpr uint32_t REFR_POS_X            = 0x30;   // float
    constexpr uint32_t REFR_POS_Y            = 0x34;   // float
    constexpr uint32_t REFR_POS_Z            = 0x38;   // float

    // Actor offsets
    constexpr uint32_t ACTOR_HEALTH          = 0x00;   // via actor value system, not direct offset
    constexpr uint32_t ACTOR_IS_DEAD         = 0x108;  // lifeState DWORD
}

// ---- Global Pointers in client.dll .data section ----
// These are RVAs (relative to client.dll base), NOT absolute VAs!
constexpr uintptr_t GLOBAL_SECURITY_TOKEN_RVA  = 0x003E8634;
constexpr uintptr_t GLOBAL_ENCOUNTER_PTR_RVA   = 0x003EE4B0; // encounter manager
constexpr uintptr_t GLOBAL_CHAR_MGR_RVA        = 0x003EC284; // GameNetCharacter manager (player characters)

// ---- Character Netref Offsets (from Ghidra decompilation) ----
constexpr uint32_t CHAR_HEALTH     = 0x558;   // float
constexpr uint32_t CHAR_MAX_HEALTH = 0x568;   // float
constexpr uint32_t CHAR_ARMOR      = 0x570;   // float
constexpr uint32_t CHAR_POS_X      = 0xEFB0;  // float
constexpr uint32_t CHAR_POS_Y      = 0xF040;  // float
constexpr uint32_t CHAR_POS_Z      = 0xF0D0;  // float

// ---- Linked List Structure ----
// Server: [manager + 0x54] = linked list head pointer
// Client: [manifest + 0x148] = circular doubly-linked list head (sentinel)
// Node layout (0x14 bytes): [next=0x00][prev=0x04][key=0x08][data_ptr=0x0C][flags=0x10]
// sentinel = manifest + 0x148 (circular list)
constexpr uint32_t MGR_LIST_HEAD_SERVER = 0x54;   // server offset
constexpr uint32_t MGR_LIST_HEAD   = 0x148;       // client manifest offset
constexpr uint32_t NODE_NEXT       = 0x00;
constexpr uint32_t NODE_PREV       = 0x04;
constexpr uint32_t NODE_KEY        = 0x08;         // VWID
constexpr uint32_t NODE_NETREF     = 0x0C;         // data pointer (GameNetCharacter*)

// ---- Key Functions in client.dll (VAs) ----
constexpr uintptr_t FN_ENCOUNTER_SET       = 0x100FE260; // void __thiscall (enc_mgr, type, value)
constexpr uintptr_t FN_GET_GAME_REF        = 0x100BC2A0; // Actor* __thiscall (game_ref_slot)
constexpr uintptr_t FN_MAIN_JOB_UPDATE     = 0x100C0430; // NetActorComponent::MainJobUpdate

// ---- Server Attack Handler (nvmp_storyserver.exe) ----
namespace server {
    constexpr uintptr_t ATTACK_HANDLER       = 0x509810;
    constexpr uintptr_t OWNERSHIP_CHECK_JE   = 0x5098E8;  // file 0x108CE8
    constexpr uintptr_t GODMODE_CHECK_JNE    = 0x509961;  // file 0x108D61
}

} // namespace nvmp
