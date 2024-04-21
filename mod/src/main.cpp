#define WIN32_LEAN_AND_MEAN
#include <Mod/CppUserModBase.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/UClass.hpp>
#include <Unreal/UFunction.hpp>
#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include <Unreal/Core/Templates/TypeCompatibleBytes.hpp>
//#include <Unreal/FWeakObjectPtr.hpp>

#include <masesk/EasySocket.hpp>

#include <nlohmann/json.hpp>

#include <unordered_map>

#include <TlHelp32.h>
#include <Psapi.h>

#define NEW_PD3

//#define MOOLAHNET_DEVELOPMENT
const char* MOOLAHNET_VERSION = "0.1";
#define MOOLAHNET_SERVER_PORT 20250

#include <MinHook.h>
#ifdef MOOLAHNET_DEVELOPMENT
#include <fstream>
#endif

int InternalHeistNameToLevelIdx(std::string InternalName) {
  /*
  * DefaultGame.ini
  * +Levels=/Game/Prototype/Maps/BranchBank/BranchBank.BranchBank
    +Levels=/Game/Prototype/Maps/ArmoredTransport/ArmoredTransport.ArmoredTransport
    +Levels=/Game/Prototype/Maps/JewelryStore/JewelryStore.JewelryStore
    +Levels=/Game/Prototype/Maps/NightClub/NightClub.NightClub
    +Levels=/Game/Prototype/Maps/ArtGallery/ArtGallery.ArtGallery
    +Levels=/Game/Prototype/Maps/FirstPlayable/FirstPlayable.FirstPlayable
    +Levels=/Game/Prototype/Maps/CargoDock/CargoDock.CargoDock
    +Levels=/Game/Prototype/Maps/Penthouse/Penthouse.Penthouse
    +Levels=/Game/DLCs/00004-DLCREMA/Prototype/Maps/Station/Station.Station
    +Levels=/Game/DLCs/00004-DLCREMA/Prototype/Maps/Villa/Villa.Villa
    +Levels=/Game/DLCs/00005-DLCHEIST0001/Prototype/Maps/DataCenter.DataCenter
  */
  static std::unordered_map<std::string, int> internalname_levelidx_map = {
    {"BranchBank", 0},
    {"ArmoredTransport", 1},
    {"JewelryStore", 2},
    {"NightClub", 3},
    {"ArtGallery", 4},
    {"FirstPlayable", 5},
    {"CargoDock", 6},
    {"Penthouse", 7},
    {"Station", 8},
    {"Villa", 9},
    {"DataCenter", 10},
  };
  return internalname_levelidx_map[InternalName];
}

#pragma region Unreal Types

constexpr static int Mode_ThreadSafe = 1;

template<int Mode = 0> class TReferenceControllerBase {
public:
  using RefCountType = std::conditional_t<Mode == Mode_ThreadSafe, std::atomic<int32_t>, int32_t>;

  RefCountType SharedReferenceCount{ 1 };
  RefCountType WeakReferenceCount{ 1 };

  TReferenceControllerBase() :SharedReferenceCount(1), WeakReferenceCount(1){}
  TReferenceControllerBase(RefCountType Shared, RefCountType Weak) : SharedReferenceCount(Shared), WeakReferenceCount(Weak) {}
};

template <int Mode> class FSharedReferencer {
public:
  TReferenceControllerBase<Mode>* ReferenceController;

  FSharedReferencer() : ReferenceController(new TReferenceControllerBase<Mode>()) {}
};

template<class Type, int Mode = 0>
class TSharedPtr {
public:
  Type* Object;
  FSharedReferencer<Mode> SharedReferenceCount;

  TSharedPtr(Type* Obj) : Object(Obj), SharedReferenceCount(FSharedReferencer<Mode>()) {}
  TSharedPtr(Type* Obj, FSharedReferencer<Mode> Referencer) : Object(Obj), SharedReferenceCount(Referencer) {}
};

template<class Type, int Mode = 0>
class TSharedRef {
public:
  Type* Object;
  FSharedReferencer<Mode> SharedReferenceCount;

  TSharedRef(Type* Obj) : Object(Obj), SharedReferenceCount(FSharedReferencer<Mode>()) { }
  TSharedRef(Type* Obj, FSharedReferencer<Mode> Referencer) : Object(Obj), SharedReferenceCount(Referencer) { }
};

// Convert a TSharedRef to a TSharedPtr
template <class Type, int Mode>
TSharedPtr<Type, Mode>* FromRef(TSharedRef<Type, Mode>* Ref) {
  return new TSharedPtr<Type, Mode>(Ref->Object, Ref->SharedReferenceCount);
}
// Convert a TSharedPtr to a TSharedRef
template <class Type, int Mode>
TSharedRef<Type, Mode>* FromPtr(TSharedPtr<Type, Mode>* Ptr) {
  return new TSharedRef<Type, Mode>(Ptr->Object, Ptr->SharedReferenceCount);
}


template <class OptionalType> class TOptional {
public:
  TOptional(const OptionalType& In) {
    bIsSet = true;

    new (&Value) OptionalType(In);
    //Value = RC::Unreal::TTypeCompatibleBytes<OptionalType>{ In };
  }

  TOptional() {
    bIsSet = false;
  }

  bool IsSet() const { return bIsSet; }
  RC::Unreal::TTypeCompatibleBytes<OptionalType> Value;
  bool bIsSet;
};


// TSharedPtr<FUniqueNetId> size = 16 bytes
// TSharedRef<FUniqueNetId> size = 16 bytes

class FUniqueNetId {};
class FUniqueNetIdString : public FUniqueNetId {
public:

  RC::Unreal::FString UniqueNetIdStr;
  RC::Unreal::FName Type;
};
#pragma endregion
#define SIZEOF_FUniqueNetIdAccelbyteResource 0x38
class FUniqueNetIdAccelbyteResource : public FUniqueNetIdString {};

using FString = RC::Unreal::FString;
using FName = RC::Unreal::FName;

//const char* FUniqueNetIdAccelbyteResource__ctor_Shipping_pattern = "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x00\x48\x8B\xFA\x48\x8B\xF1\x48\x8D\x15\x00\x00\x00\x00\x41\xB8\x00\x00\x00\x00\x48\x8D\x4C\x24\x00\xE8\x00\x00\x00\x00\x48\x8D\x4E\x00\x48\x8B\xD7\xF2\x0F\x10\x00\x8B\x58\x00\x33\xC0\x48\x89\x46\x00\x48\x89\x46\x00\x48\x8D\x05\x00\x00\x00\x00\x48\x89\x06\xF2\x0F\x11\x44\x24\x00\xE8\x00\x00\x00\x00\xF2\x0F\x10\x44\x24\x00\x48\x8D\x05\x00\x00\x00\x00\xF2\x0F\x11\x46\x00\x48\x89\x06\x48\x8B\xC6\x89\x5E\x00\x48\x8B\x5C\x24\x00\x48\x8B\x74\x24\x00\x48\x83\xC4\x00\x5F\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x48\x8D\x05";
//const char* FUniqueNetIdAccelbyteResource__ctor_Shipping_mask = "xxxx?xxxx?xxxx?xxxxxxxxx????xx????xxxx?x????xxx?xxxxxxxxx?xxxxx?xxx?xxx????xxxxxxxx?x????xxxxx?xxx????xxxx?xxxxxxxx?xxxx?xxxx?xxx?xxxxxxxxxxxxxxxxx";

// Known to change
/*const char* USBZLobby__JoinSessionById_Shipping_pattern = "\x48\x89\x5C\x24\x00\x48\x89\x6C\x24\x00\x56\x57\x41\x56\x48\x81\xEC\x00\x00\x00\x00\x4D\x8B\xF0\x48\x8B\xEA\x48\x8B\xF9\xE8\x00\x00\x00\x00\x48\x8B\x4F\x00\x48\x8D\x94\x24\x00\x00\x00\x00\x48\x8B\x01\xFF\x90\x00\x00\x00\x00\x48\x8B\xF0\x48\x83\x38\x00\x74\x00\xE8\x00\x00\x00\x00\x48\x85\xC0\x74\x00\x48\x8B\x36\x48\x8D\x50\x00\x48\x63\x40\x00\x3B\x46\x00\x7F\x00\x48\x8B\xC8\x48\x8B\x46\x00\x48\x39\x14\xC8\x74\x00\x33\xF6\x48\x8D\x4C\x24\x00\xE8\x00\x00\x00\x00\x4C\x8B\x07\x48\x8B\xCF\x48\x8B\xD8\x41\xFF\x90\x00\x00\x00\x00\x45\x33\xC9\x48\x89\x5C\x24\x00\x48\x8B\xC8\x45\x33\xC0\x48\x8B\xD6\xE8\x00\x00\x00\x00\x48\x89\x87\x00\x00\x00\x00\x48\x85\xC0\x0F\x84\x00\x00\x00\x00\x48\x8B\xD5\x48\x8B\xC8\xE8\x00\x00\x00\x00\x48\x8B\x4F\x00\x48\x8D\x54\x24\x00\x48\x8B\x89\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x48\x8B\x08\x48\x89\x4C\x24\x00\x48\x8B\x48\x00\x48\x89\x4C\x24\x00\x48\x85\xC9\x74\x00\xFF\x41\x00\x48\x8B\x5C\x24\x00\x48\x8D\x35\x00\x00\x00\x00\x48\x89\x74\x24\x00\x48\xC7\x44\x24\x00\x00\x00\x00\x00\x48\xC7\x44\x24\x00\x00\x00\x00\x00\x48\x85\xDB\x74\x00\x83\x6B\x00\x00\x75\x00\x48\x8B\x03\x48\x8B\xCB\xFF\x10\x83\x6B\x00\x00\x75\x00\x48\x8B\x03\xBA\x00\x00\x00\x00\x48\x8B\xCB\xFF\x50\x00\x48\x8B\x8F\x00\x00\x00\x00\x48\x8D\x54\x24\x00\x4D\x8B\xC6\x48\x8B\x01\xFF\x90\x00\x00\x00\x00\x48\x8B\x4C\x24\x00\x48\x89\x74\x24\x00\x48\x85\xC9\x74\x00\xE8\x00\x00\x00\x00\x48\x8B\x5C\x24\x00\x48\x85\xDB\x74\x00\x83\x6B\x00\x00\x75\x00\x48\x8B\x03\x48\x8B\xCB\xFF\x10\x83\x6B\x00\x00\x75\x00\x48\x8B\x03\xBA\x00\x00\x00\x00\x48\x8B\xCB\xFF\x50\x00\x48\x8B\x87";
const char* USBZLobby__JoinSessionById_Shipping_mask = "xxxx?xxxx?xxxxxxx????xxxxxxxxxx????xxx?xxxx????xxxxx????xxxxxx?x?x????xxxx?xxxxxx?xxx?xx?x?xxxxxx?xxxxx?xxxxxx?x????xxxxxxxxxxxx????xxxxxxx?xxxxxxxxxx????xxx????xxxxx????xxxxxxx????xxx?xxxx?xxx????x????xxxxxxx?xxx?xxxx?xxxx?xx?xxxx?xxx????xxxx?xxxx?????xxxx?????xxxx?xx??x?xxxxxxxxxx??x?xxxx????xxxxx?xxx????xxxx?xxxxxxxx????xxxx?xxxx?xxxx?x????xxxx?xxxx?xx??x?xxxxxxxxxx??x?xxxx????xxxxx?xxx";

const char* USBZPartyClient__ConnectToLobby_Implementation_Shipping_pattern = "\x40\x56\x48\x83\xEC\x00\x83\x7A\x00\x00\x48\x8B\xF1\x0F\x8E\x00\x00\x00\x00\x48\x89\x5C\x24\x00\x48\x81\xC1\x00\x00\x00\x00\x48\x89\x6C\x24\x00\x48\x89\x7C\x24\x00\xE8\x00\x00\x00\x00\x48\x8B\xCE";
const char* USBZPartyClient__ConnectToLobby_Implementation_Shipping_mask = "xxxxx?xx??xxxxx????xxxx?xxx????xxxx?xxxx?x????xxx";


const char* USBZPartyManagerABV2__JoinParty_Shipping_pattern = "";
const char* USBZPartyManagerABV2__JoinParty_Shipping_mask = "";

class FOnlineSessionV2Accelbyte;

typedef void(__fastcall *USBZLobby__JoinSessionById_t)(__int64 *this_, TSharedPtr<FUniqueNetIdAccelbyteResource>* SessionId);
USBZLobby__JoinSessionById_t USBZLobby__JoinSessionById = nullptr;

typedef void(__fastcall* USBZPartyClient__ConnectToLobby_Implementation_t)(__int64* this_, const RC::Unreal::TArray<RC::Unreal::TCHAR>* InLobbyNetId);
USBZPartyClient__ConnectToLobby_Implementation_t USBZPartyClient__ConnectToLobby_Implementation;

typedef __int64(__fastcall* FUniqueNetIdAccelbyteResource__ctor_t)(FUniqueNetIdAccelbyteResource* this_, const FString* a2);
FUniqueNetIdAccelbyteResource__ctor_t FUniqueNetIdAccelbyteResource__ctor;

class USBZPartyManagerABV2;
namespace vftables {
  struct USBZPartyManagerABV2_vtbl
  {

  void(__fastcall* empty_filler_01111111111111111111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111111)();
  void(__fastcall* empty_filler_01111111111111111111)();
  void(__fastcall* empty_filler_0111111111111111111)();
  void(__fastcall* empty_filler_011111111111111111)();
  void(__fastcall* empty_filler_01111111111111111)();
  void(__fastcall* empty_filler_0111111111111111)();
  void(__fastcall* empty_filler_011111111111111)();
  void(__fastcall* empty_filler_01111111111111)();
  void(__fastcall* empty_filler_0111111111111)();
  void(__fastcall* empty_filler_011111111111)();
  void(__fastcall* empty_filler_01111111111)();
  void(__fastcall* empty_filler_0111111111)();
  void(__fastcall* empty_filler_011111111)();
  void(__fastcall* empty_filler_01111111)();
  void(__fastcall* empty_filler_0111111)();
  void(__fastcall* empty_filler_011111)();
  void(__fastcall* empty_filler_01111)();
  void(__fastcall* empty_filler_0111)();
  void(__fastcall* empty_filler_011)();
  void(__fastcall* empty_filler_01)();
  void(__fastcall* empty_filler_80)();
  void(__fastcall* empty_filler_81)();
  void(__fastcall* empty_filler_82)();
  void(__fastcall* JoinParty)(USBZPartyManagerABV2* this_, FString *);
  };

}
class USBZPartyManagerABV2 {
public:
  vftables::USBZPartyManagerABV2_vtbl* __vftable;
};

typedef void(__fastcall* USBZPartyManagerABV2__JoinParty_t)(USBZPartyManagerABV2* this_, FString* Code);
USBZPartyManagerABV2__JoinParty_t USBZPartyManagerABV2__JoinParty;*/

const char* USBZMatchmaking__SetArmadaInfo_Shipping_pattern = "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x00\x48\x8D\x99\x00\x00\x00\x00\x48\x8B\xFA\x48\x8B\xCB\xE8\x00\x00\x00\x00\x8B\x47";
const char* USBZMatchmaking__SetArmadaInfo_Shipping_mask = "xxxx?xxxx?xxx????xxxxxxx????xx";

const char* USBZStateMachine__SetState_Shipping_pattern = "\x48\x89\x5C\x24\x00\x55\x56\x57\x48\x83\xEC\x00\x80\x79";
const char* USBZStateMachine__SetState_Shipping_mask = "xxxx?xxxxxx?xx";

const char* USBZAnalyticsManager__SendLobbyJoined_Shipping_pattern = "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x48\x89\x7C\x24\x00\x55\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8B\xEC\x48\x83\xEC\x00\x48\x8B\x02\x4C\x8B\xE9";
const char* USBZAnalyticsManager__SendLobbyJoined_Shipping_mask = "xxxx?xxxx?xxxx?xxxxxxxxxxxxxxx?xxxxxx";

unsigned __int64 FindPattern(char* module, const char* funcname, const char* pattern, const char* mask);

//TSharedRef<FUniqueNetIdAccelbyteResource>* GetAccelbyteNetIdFromString(FString str) {
//  TSharedRef<FUniqueNetIdAccelbyteResource>* Ref = new TSharedRef<FUniqueNetIdAccelbyteResource>(nullptr); // breaking unreal's rules here, TSharedRefs cant be nullptr
//
//  //FUniqueNetIdAccelbyteResource* obj = (FUniqueNetIdAccelbyteResource*)RC::Unreal::FMemory::Malloc(SIZEOF_FUniqueNetIdAccelbyteResource);
//  FUniqueNetIdAccelbyteResource* obj = (FUniqueNetIdAccelbyteResource*)malloc(SIZEOF_FUniqueNetIdAccelbyteResource);
//  FUniqueNetIdAccelbyteResource__ctor(obj, &str);
//  Ref->Object = obj;
//
//  //FUniqueNetIdAccelByteResource__Create(Ref, &str);
//
//  return Ref;
//}

#pragma region Starbreeze Types

// Armada is what AccelByte uses for it's dedicated server system
class FSBZArmadaInfo {
public:
  FString IP;
  int PortGame;
  int PortBeacon;
  FString MatchID;
  FString ServerVersion;

#ifdef NEW_PD3 // Region was added at some point
  FString Region;
#endif

  FSBZArmadaInfo(const FSBZArmadaInfo& In) {
    this->IP = In.IP;
    this->PortGame = In.PortGame;
    this->PortBeacon = In.PortBeacon;
    this->MatchID = In.MatchID;
    this->ServerVersion = In.ServerVersion;

#ifdef NEW_PD3
    this->Region = In.Region;
#endif
  }

#ifdef NEW_PD3
  FSBZArmadaInfo(FString Ip, int PortGame, int PortBeacon, FString MatchId, FString ServerVersion, FString Region) : IP(Ip), PortGame(PortGame), PortBeacon(PortBeacon), MatchID(MatchId), ServerVersion(ServerVersion), Region(Region) {}
#else
  FSBZArmadaInfo(FString Ip, int PortGame, int PortBeacon, FString MatchId, FString ServerVersion) : IP(Ip), PortGame(PortGame), PortBeacon(PortBeacon), MatchID(MatchId), ServerVersion(ServerVersion) {}
#endif
};

typedef __int64 USBZMatchmaking;
typedef __int64 USBZStateMachine;
typedef __int64 USBZAnalyticsManager;

class USBZStateMachineState : public RC::Unreal::UObject {
public:
  char uobj[0x30];
  FName StateName;
};
class USBZStateMachineData : public RC::Unreal::UObject {
public:
  char uobj[0x30];
  USBZStateMachineState* PreviousState;
};

enum ESBZMatchmakingCommand : __int8 {
  RegularMatchmaking = 0x0,
  ForceListenServer = 0x1,
  ForceClient = 0x2
};

enum ESBZSecurityCompany {
  None,
  GenSec,
  Senturian,
  AmGuard
};

class FSBZOnlineMatchmakingParams { // Only LevelIdx is set when joining friend?
public:
  bool bQuickMatch;
  int LevelIdx;
  uint8_t DifficultyIdx; // unsigned __int8 DifficultyIdx;
  RC::Unreal::TArray<ESBZSecurityCompany> SecurityCompanies;
  int MatchmakingRandomSeed;
  ESBZMatchmakingCommand Command;

  FSBZOnlineMatchmakingParams() {};
  FSBZOnlineMatchmakingParams(bool QuickMatch, int LevelIdx, uint8_t DifficultyIdx,
                              RC::Unreal::TArray<ESBZSecurityCompany> SecurityCompanies,
                              int MatchmakingRandomSeed, ESBZMatchmakingCommand Command) {
    this->bQuickMatch = QuickMatch;
    this->LevelIdx = LevelIdx;
    this->DifficultyIdx = DifficultyIdx;
    this->SecurityCompanies = SecurityCompanies;
    this->MatchmakingRandomSeed = MatchmakingRandomSeed;
    this->Command = Command;
  }
};

/*
struct __cppobj __declspec(align(8)) USBZStateMachineDataMatchmaking : USBZStateMachineData
{
  FSBZOnlineMatchmakingParams Params;
  TOptional<FSBZArmadaInfo> ServerInfo;
  FString PartyCode;
  bool bAsyncLoadingComplete;
  bool bCrossPlayEnabled;
  bool bInCrossPlayLobby;
  bool bCrossPlayAllowed;
  FString BuildVersion;
  _BYTE JoinType[1];
};
*/

enum ESBZOnlineJoinType : uint8_t {
  Debug,
  Public,
  Private,
  FriendsOnly,
  InviteOnly,
  Default = 0x1,
};

#define USBZStateMachineDataMatchmaking_align alignas(8)
class USBZStateMachineDataMatchmaking : public USBZStateMachineData {
public:
  USBZStateMachineDataMatchmaking_align FSBZOnlineMatchmakingParams Params;
  USBZStateMachineDataMatchmaking_align TOptional<FSBZArmadaInfo> ServerInfo; // Missing in UHT, check if it still exists
  USBZStateMachineDataMatchmaking_align FString PartyCode;
  bool bAsyncLoadingComplete;
  bool bCrossPlayEnabled;
  bool bInCrossPlayLobby;
  bool bCrossPlayAllowed;
  USBZStateMachineDataMatchmaking_align FString BuildVersion;
  USBZStateMachineDataMatchmaking_align ESBZOnlineJoinType JoinType;//char JoinType[1]; // likely an enum

  USBZStateMachineDataMatchmaking(FSBZOnlineMatchmakingParams Params, TOptional<FSBZArmadaInfo> ServerInfo,
                                  FString PartyCode, bool bAsyncLoadingComplete, bool bCrossPlayEnabled,
                                  bool bInCrossPlayLobby, bool bCrossPlayAllowed, FString BuildVersion, ESBZOnlineJoinType JoinType) :
                                  Params(Params), ServerInfo(ServerInfo), PartyCode(PartyCode), bAsyncLoadingComplete(bAsyncLoadingComplete),
                                  bCrossPlayEnabled(bCrossPlayEnabled), bInCrossPlayLobby(bInCrossPlayLobby), bCrossPlayAllowed(bCrossPlayAllowed),
                                  BuildVersion(BuildVersion), JoinType(JoinType) {};
};
#undef USBZStateMachineDataMatchmaking_align

//static_assert(sizeof(USBZStateMachineDataMatchmaking) == 0xE0);
//static_assert(sizeof(USBZStateMachineData) == 0x38);
//static_assert(sizeof(RC::Unreal::UObject) == 0x30);
#pragma endregion

class FGameplayTag {
public:
  RC::Unreal::FName TagName;
};

typedef void(__fastcall* USBZMatchmaking__SetArmadaInfo_t)(USBZMatchmaking* _this, FSBZArmadaInfo* armadainfo);
USBZMatchmaking__SetArmadaInfo_t USBZMatchmaking__SetArmadaInfo;

typedef void(__fastcall* USBZStateMachine__SetState_t)(USBZStateMachine* _this, FName* StateName, USBZStateMachineData* InData);
USBZStateMachine__SetState_t USBZStateMachine__SetState;


typedef __int64(__fastcall* USBZAnalyticsManager__SendLobbyJoined_t)(USBZAnalyticsManager* _this, const RC::Unreal::UObject* WorldContextObject, const USBZStateMachineDataMatchmaking* MatchmakingData);
USBZAnalyticsManager__SendLobbyJoined_t USBZAnalyicsManager__SendLobbyJoined;
USBZAnalyticsManager__SendLobbyJoined_t USBZAnalyicsManager__SendLobbyJoined_o;

#pragma region FString + std::string conversions
std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
std::string FString_to_std_string(FString str) {
  return converter.to_bytes(str.GetCharArray());
}
std::string wstr_to_string(std::wstring wstr) {
  return converter.to_bytes(wstr);
}
FString wstr_to_fstring(const std::wstring& wide) {
  return FString(wide.c_str());
}
FString str_to_fstring(const std::string& str) {
  return wstr_to_fstring(converter.from_bytes(str));
}
#pragma endregion


std::ostream& operator<<(std::ostream& a, FString& b) {
  a << FString_to_std_string(b);
  return a;
}

std::ostream& operator<<(std::ostream& a, FName& b) {
  a << converter.to_bytes(b.ToString());
  return a;
}

bool FreezeLobbyJoinedAnalytics = false;
__int64 __fastcall USBZAnalyicsManager__SendLobbyJoined_h(USBZAnalyticsManager* _this, const RC::Unreal::UObject* WCO, const USBZStateMachineDataMatchmaking* MatchmakingData) {
  if (FreezeLobbyJoinedAnalytics)
    return 0;
  return USBZAnalyicsManager__SendLobbyJoined_o(_this, WCO, MatchmakingData);
}

#ifdef MOOLAHNET_DEVELOPMENT

std::string JoinTypeToStr(ESBZOnlineJoinType type) {
  switch(type) {
  case ESBZOnlineJoinType::Debug:
    return "ESBZOnlineJoinType::Debug";
  case ESBZOnlineJoinType::Public:
    return "ESBZOnlineJoinType::Public";
  case ESBZOnlineJoinType::Private:
    return "ESBZOnlineJoinType::Private";
  case ESBZOnlineJoinType::FriendsOnly:
    return "ESBZOnlineJoinType::FriendsOnly";
  case ESBZOnlineJoinType::InviteOnly:
    return "ESBZOnlineJoinType::InviteOnly";
  default:
    return "Unknown Value";
  }
}

std::string MatchmakingCommandToStr(ESBZMatchmakingCommand command) {
  switch (command) {
  case ESBZMatchmakingCommand::RegularMatchmaking:
    return "ESBZMatchmakingCommand::RegularMatchmaking";
  case ESBZMatchmakingCommand::ForceListenServer:
    return "ESBZMatchmakingCommand::ForceListenServer";
  case ESBZMatchmakingCommand::ForceClient:
    return "ESBZMatchmakingCommand::ForceClient";
  default:
    return "Unknown Value";
  }
}

std::ostream& operator<<(std::ostream& a, USBZStateMachineDataMatchmaking& matchmaking_state_machine_data) {
  std::cout << "USBZStateMachineDataMatchmaking:" << std::endl;


#define PRINT_(ToPrint) std::cout << "\t" << #ToPrint << " : " << ToPrint << std::endl;

#define PRINT_BOOL(ToPrint) std::cout << "\t" << #ToPrint << " : " << (ToPrint ? "true" : "false") << std::endl;

  PRINT_(matchmaking_state_machine_data.Params.bQuickMatch);
  PRINT_(matchmaking_state_machine_data.Params.LevelIdx);
  PRINT_(matchmaking_state_machine_data.Params.DifficultyIdx);
  PRINT_(matchmaking_state_machine_data.Params.SecurityCompanies.Num());
  PRINT_(matchmaking_state_machine_data.Params.MatchmakingRandomSeed);
  PRINT_(matchmaking_state_machine_data.Params.Command);

  /*PRINT_(matchmaking_state_machine_data.ServerInfo.IsSet());
  if (matchmaking_state_machine_data.ServerInfo.IsSet()) {
    PRINT_(matchmaking_state_machine_data.ServerInfo.Value.GetTypedPtr()->IP);
    PRINT_(matchmaking_state_machine_data.ServerInfo.Value.GetTypedPtr()->PortGame);
    PRINT_(matchmaking_state_machine_data.ServerInfo.Value.GetTypedPtr()->PortBeacon);
    PRINT_(matchmaking_state_machine_data.ServerInfo.Value.GetTypedPtr()->MatchID);
    PRINT_(matchmaking_state_machine_data.ServerInfo.Value.GetTypedPtr()->ServerVersion);
  }*/
  PRINT_(matchmaking_state_machine_data.PartyCode);

  PRINT_BOOL(matchmaking_state_machine_data.bAsyncLoadingComplete);
  PRINT_BOOL(matchmaking_state_machine_data.bCrossPlayEnabled);
  PRINT_BOOL(matchmaking_state_machine_data.bInCrossPlayLobby);
  PRINT_BOOL(matchmaking_state_machine_data.bCrossPlayAllowed);

  PRINT_(matchmaking_state_machine_data.BuildVersion);
  PRINT_(JoinTypeToStr(matchmaking_state_machine_data.JoinType));

#undef PRINT_
#undef PRINT_BOOL

  return a;
}

USBZMatchmaking__SetArmadaInfo_t USBZMatchmaking__SetArmadaInfo_o;
void __fastcall USBZMatchmaking__SetArmadaInfo_h(USBZMatchmaking* _this, FSBZArmadaInfo* armadainfo) {
  std::ofstream armadainfo_out("./armadalog.txt", std::ios::app);

  armadainfo_out << "Armada IP: " << armadainfo->IP << "\n";
  armadainfo_out << "Armada Game Port: " << armadainfo->PortGame << "\n";
  armadainfo_out << "Armada Beacon Port: " << armadainfo->PortBeacon << "\n";
  armadainfo_out << "Armada Match ID: " << armadainfo->MatchID << "\n";
  armadainfo_out << "Armada Server Version: " << armadainfo->ServerVersion << "\n";
  armadainfo_out << "Armada Server Region: " << armadainfo->Region << "\n";

  armadainfo_out << "\n\n";
  armadainfo_out.close();

  USBZMatchmaking__SetArmadaInfo_o(_this, armadainfo);
}

#endif

std::wstring G_CurrentGameState;
USBZStateMachine__SetState_t USBZStateMachine__SetState_o;
void __fastcall USBZStateMachine__SetState_h(USBZStateMachine* _this, FName* StateName, USBZStateMachineData* InData) {
#ifdef MOOLAHNET_DEVEOPMENT
  std::cout << "USBZStateMachine is changing states to state: " << *StateName << std::endl;

  if (*StateName == FName(STR("ABMatchmaking"))) {
    std::cout << *((USBZStateMachineDataMatchmaking*)InData) << std::endl;

    std::ofstream stream(std::string(std::string("statemachine_") + FString_to_std_string(((USBZStateMachineDataMatchmaking*)InData)->ServerInfo.Value.GetTypedPtr()->IP)) + ".bin");
    for (int i = 0; i < sizeof(USBZStateMachineDataMatchmaking); i++) {
      stream << ((char*)InData)[i];
    }
    stream.close();
  }
#endif
  G_CurrentGameState = StateName->ToString();
  USBZStateMachine__SetState_o(_this, StateName, InData);
}

masesk::EasySocket _server_socket;
void socket_message(const std::string& data) {

  if (data.starts_with("version")) { // Return version number
    _server_socket.socketSend("", MOOLAHNET_VERSION);
    return;
  }

  //FString unreal_string = FString((RC::Unreal::TCHAR*)converter.from_bytes(data).c_str());

  if (USBZMatchmaking__SetArmadaInfo == NULL) {
    MessageBoxA(GetActiveWindow(), "USBZMatchmaking::SetArmadaInfo is null, contact the mod author.", "MoolahNet - Fatal Error", NULL);
    return;
  }
  if (USBZStateMachine__SetState == NULL) {
    MessageBoxA(GetActiveWindow(), "USBZStateMachine::SetState is null, contact the mod author.", "MoolahNet - Fatal Error", NULL);
    return;
  }

  if (wstr_to_string(G_CurrentGameState) != "GameStart") {
    return; // user is not in a state where they can join a match
  }
  
  nlohmann::json session = nlohmann::json::parse(data);

  /*FString unreal_string = str_to_fstring(session[""]); // unreal string of party code

  TSharedRef<FUniqueNetIdAccelbyteResource>* ref = GetAccelbyteNetIdFromString(unreal_string);
  TSharedPtr<FUniqueNetIdAccelbyteResource>* resource = FromRef(ref);

  resource->SharedReferenceCount = FSharedReferencer<0>();

  __int64* sbzlobby = (__int64*)RC::Unreal::UObjectGlobals::FindFirstOf(STR("/Script/Starbreeze.SBZLobby"));
  __int64* sbzlobby = (__int64*)RC::Unreal::UObjectGlobals::FindFirstOf(STR("SBZLobby"));
  std::cout << sbzlobby << std::endl;
  std::stringstream stream;
  stream << sbzlobby;
  MessageBoxA(NULL, stream.str().c_str(), "SBZLobby", NULL);
  USBZLobby__JoinSessionById(sbzlobby, resource);


  __int64* sbzpartyclient = (__int64*)RC::Unreal::UObjectGlobals::FindFirstOf(STR("SBZPartyClient"));

  USBZPartyClient__ConnectToLobby_Implementation(sbzpartyclient, &unreal_string.GetCharTArray());

  USBZPartyManagerABV2* partymanager = (USBZPartyManagerABV2*)RC::Unreal::UObjectGlobals::FindFirstOf(STR("SBZPartyManagerABV2"));

  partymanager->__vftable->JoinParty(partymanager, &unreal_string);
  USBZPartyManagerABV2__JoinParty(partymanager, &unreal_string);

  std::cout << data << std::endl;*/

  nlohmann::json DSInformation = session["DSInformation"]["Server"];
#ifdef MOOLAHNET_DEVELOPMENT
#define LOG_TYPE(obj) \
std::cout << #obj << " : " << obj.type_name() << std::endl;

  LOG_TYPE(DSInformation);
  LOG_TYPE(DSInformation["ip"]);
  LOG_TYPE(DSInformation["port"]);
  LOG_TYPE(DSInformation["ports"]["beacon"]);
  LOG_TYPE(DSInformation["session_id"]);
  LOG_TYPE(DSInformation["game_version"]);
  LOG_TYPE(DSInformation["region"]);
#endif

  auto IP = str_to_fstring(DSInformation["ip"]);
  int PortGame = DSInformation["port"];
  int PortBeacon = DSInformation["ports"]["beacon"];
  auto MatchId = str_to_fstring(DSInformation["session_id"]);
  auto ServerVersion = str_to_fstring(DSInformation["game_version"]);
  auto Region = str_to_fstring(DSInformation["region"]);

  FSBZArmadaInfo armada_info = FSBZArmadaInfo(IP, PortGame, PortBeacon, MatchId, ServerVersion, Region);

  USBZMatchmaking* matchmaking = (USBZMatchmaking*)RC::Unreal::UObjectGlobals::FindFirstOf(STR("SBZMatchmaking"));

#ifdef MOOLAHNET_DEVELOPMENT
  USBZMatchmaking__SetArmadaInfo_o
#else
  USBZMatchmaking__SetArmadaInfo
#endif
    (matchmaking, &armada_info);

  USBZStateMachine* statemachine = (USBZStateMachine*)RC::Unreal::UObjectGlobals::FindFirstOf(STR("SBZStateMachine"));
  FName next_state = FName(STR("ABMatchmaking"));

  using UClass = RC::Unreal::UClass;
  using UObject = RC::Unreal::UObject;
  UClass* clazz = RC::Unreal::UObjectGlobals::StaticFindObject<UClass*>((UClass*)NULL, (UObject*)NULL, STR("/Script/Starbreeze.SBZStateMachineDataMatchmaking"));

  RC::Unreal::FStaticConstructObjectParameters ConstructParams(clazz, nullptr);

  ConstructParams.Name = RC::Unreal::NAME_None;
  ConstructParams.SetFlags = RC::Unreal::RF_NoFlags;
  ConstructParams.Template = nullptr;
  ConstructParams.bCopyTransientsFromClassDefaults = false;
  ConstructParams.InstanceGraph = nullptr;
  ConstructParams.ExternalPackage = nullptr;
  //ConstructParams;

  USBZStateMachineDataMatchmaking* in_data = RC::Unreal::UObjectGlobals::StaticConstructObject<USBZStateMachineDataMatchmaking*>(ConstructParams);

  auto attributes = session["attributes"];

  //std::string MatchmakingCommand_String = attributes["Command"]; // "ESBZMatchmakingCommand::RegularMatchmaking"

  std::string DifficultyIdx_String = "";
  if (attributes.contains("DifficultyIdx")) {
    DifficultyIdx_String = attributes["DifficultyIdx"];
  }
  else {
    DifficultyIdx_String = attributes["DIFFICULTYIDX"]; // "3" - OVK
  }

  //std::string MMGroup_String = attributes["MMGroup"]; // "-1"

  //std::string MMRandomSeed_String = attributes["MMRandomSeed"]; // "0"

  //std::string SecurityCompany_String = attributes["SecurityCompany"]; // "0"

  //in_data->PreviousState = (USBZStateMachineState*)(statemachine + 0x48);


  //USBZStateMachineDataMatchmaking* in_data = new USBZStateMachineDataMatchmaking(
  new (in_data) USBZStateMachineDataMatchmaking(
    FSBZOnlineMatchmakingParams(
      false, // QuickMatch
      InternalHeistNameToLevelIdx(session["configuration"]["attributes"]["MapAssetNameTest"][0]), // could change, older versions of game session info used to contain the LevelIdx
      std::stoi(DifficultyIdx_String), // DifficultyIdx
      {},// { ESBZSecurityCompany::None }, // SecurityCompanies // RC::Unreal::TArray<ESBZSecurityCompany>()
      0, // MatchmakingRandomSeed
      ESBZMatchmakingCommand::RegularMatchmaking // Command
    ),
    TOptional<FSBZArmadaInfo>(armada_info),
    FString(STR("")), // PartyCode
    true, true, false, true,
    FString(STR("")), // Build Version
    ESBZOnlineJoinType::Debug
  );


  //in_data.ServerInfo = TOptional<FSBZArmadaInfo>(armada_info);

  FreezeLobbyJoinedAnalytics = true;
  USBZStateMachine__SetState_o
    (statemachine, &next_state, in_data);
  return;
}

DWORD WINAPI socket_thread(LPVOID server_socket_) {
  SetThreadDescription(GetCurrentThread(), STR("MoolahNet-SocketServerThread"));

  masesk::EasySocket* server_socket = (masesk::EasySocket*)server_socket_;
  server_socket->socketListen("", MOOLAHNET_SERVER_PORT, socket_message);

  return TRUE;
}

static const wchar_t* GetMoolahNetVersionUE4SS() {
  static auto ret = converter.from_bytes(MOOLAHNET_VERSION);
  return ret.c_str();
}

class MoolahNetMod : public RC::CppUserModBase {
public:
#ifdef MOOLAHNET_DEVELOPMENT
  bool Development = true;
#else
  bool Development = false;
#endif
  MoolahNetMod() : CppUserModBase()
  {
    ModName = STR("MoolahNet");
    ModVersion = GetMoolahNetVersionUE4SS();
    ModDescription = STR("Game-side mod for MoolahNet");
  }

  ~MoolahNetMod() override {
    _server_socket.stop();
  }

  auto on_dll_load(std::wstring_view) -> void override {}
  auto on_program_start() -> void override {}

  void on_unreal_init() override {
    
#ifndef MOOLAHNET_DEVELOPMENT
    // Enable a subset of logging outside of a development environment
    LPSTR cmdline = GetCommandLineA();
    if (std::string(cmdline).find("-moolahnet-logging", 0) != std::string::npos) {
      this->Development = true;
    }
#else
#endif

    if (this->Development) {
      AllocConsole();
      freopen("CONOUT$", "w", stdout);
    }

    CreateThread(NULL, NULL, [](LPVOID) {
      SetThreadDescription(GetCurrentThread(), STR("MoolahNet-SigScanningThread"));

      USBZStateMachine__SetState = (USBZStateMachine__SetState_t)FindPattern((char*)"PAYDAY3Client-Win64-Shipping.exe", "USBZStateMachine__SetState",
        USBZStateMachine__SetState_Shipping_pattern,
        USBZStateMachine__SetState_Shipping_mask);

      USBZMatchmaking__SetArmadaInfo = (USBZMatchmaking__SetArmadaInfo_t)FindPattern((char*)"PAYDAY3Client-Win64-Shipping.exe", "USBZMatchmaking__SetArmadaInfo",
        USBZMatchmaking__SetArmadaInfo_Shipping_pattern,
        USBZMatchmaking__SetArmadaInfo_Shipping_mask);

      USBZAnalyicsManager__SendLobbyJoined = (USBZAnalyticsManager__SendLobbyJoined_t)FindPattern((char*)"PAYDAY3Client-Win64-Shipping.exe", "USBZAnalyicsManager__SendLobbyJoined",
        USBZAnalyticsManager__SendLobbyJoined_Shipping_pattern,
        USBZAnalyticsManager__SendLobbyJoined_Shipping_mask);

      /*USBZPartyManagerABV2__JoinParty = (USBZPartyManagerABV2__JoinParty_t)FindPattern((char*)"PAYDAY3Client-Win64-Shipping.exe", "USBZPartyManagerABV2__JoinParty",
        USBZPartyManagerABV2__JoinParty_Shipping_pattern,
        USBZPartyManagerABV2__JoinParty_Shipping_mask);

      USBZPartyClient__ConnectToLobby_Implementation = (USBZPartyClient__ConnectToLobby_Implementation_t)FindPattern((char*)"PAYDAY3Client-Win64-Shipping.exe", "USBZPartyClient__ConnectToLobby_Implementation",
        USBZPartyClient__ConnectToLobby_Implementation_Shipping_pattern,
        USBZPartyClient__ConnectToLobby_Implementation_Shipping_mask);

      USBZLobby__JoinSessionById = (USBZLobby__JoinSessionById_t)FindPattern((char*)"PAYDAY3Client-Win64-Shipping.exe", "USBZLobby__JoinSessionById",
        USBZLobby__JoinSessionById_Shipping_pattern,
        USBZLobby__JoinSessionById_Shipping_mask);

      FUniqueNetIdAccelbyteResource__ctor = (FUniqueNetIdAccelbyteResource__ctor_t)FindPattern((char*)"PAYDAY3Client-Win64-Shipping.exe", "FUniqueNetIdAccelbyteResource__ctor",
        FUniqueNetIdAccelbyteResource__ctor_Shipping_pattern,
        FUniqueNetIdAccelbyteResource__ctor_Shipping_mask);

      std::stringstream sstream;
      sstream << USBZLobby__JoinSessionById;
      MessageBox(NULL, converter.from_bytes(sstream.str()).c_str(), STR("JoinSessionById"), NULL);
      std::stringstream sstream2;
      sstream2 << FUniqueNetIdAccelbyteResource__ctor;
      MessageBox(NULL, converter.from_bytes(sstream2.str()).c_str(), STR("FUniqueNetIdAccelbyteResource"), NULL);*/

      if (USBZStateMachine__SetState == NULL) {
        MessageBoxA(GetActiveWindow(), "USBZStateMachine::SetState is null, contact the mod author.", "MoolahNet - Fatal Error", NULL);
        return (DWORD)TRUE;
      }

      MH_Initialize();

      MH_CreateHook((LPVOID)USBZAnalyicsManager__SendLobbyJoined, USBZAnalyicsManager__SendLobbyJoined_h, reinterpret_cast<LPVOID*>(USBZAnalyicsManager__SendLobbyJoined_o));
      MH_EnableHook((LPVOID)USBZAnalyicsManager__SendLobbyJoined);

      MH_CreateHook((LPVOID)USBZStateMachine__SetState, &USBZStateMachine__SetState_h, reinterpret_cast<LPVOID*>(&USBZStateMachine__SetState_o));
      MH_EnableHook((LPVOID)USBZStateMachine__SetState);

#ifdef MOOLAHNET_DEVELOPMENT
      MH_CreateHook((LPVOID)USBZMatchmaking__SetArmadaInfo, &USBZMatchmaking__SetArmadaInfo_h, reinterpret_cast<LPVOID*>(&USBZMatchmaking__SetArmadaInfo_o));
      MH_EnableHook((LPVOID)USBZMatchmaking__SetArmadaInfo);
#endif

      return (DWORD)TRUE;
      }, NULL, NULL, NULL)
      ;
    RC::Output::send<LogLevel::Normal>(STR("MoolahNet initialised"));
    CreateThread(NULL, NULL, socket_thread, (LPVOID)(&_server_socket), NULL, NULL);
  }
};

extern "C" {
  __declspec(dllexport) RC::CppUserModBase* start_mod() {
    return new MoolahNetMod();
  }
  __declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod) {
    delete mod;
  }
}

// From RAID-BLT https://raw.githubusercontent.com/Luffyyy/Raid-BLT/master/src/signatures/signatures.cpp
MODULEINFO GetModuleInfo(std::string szModule)
{
  MODULEINFO modinfo = { 0 };
  HMODULE hModule = GetModuleHandleA(szModule.c_str());
  if (hModule == 0)
    return modinfo;
  GetModuleInformation(GetCurrentProcess(), hModule, &modinfo, sizeof(MODULEINFO));
  return modinfo;
}


unsigned __int64 FindPattern(char* module, const char* funcname, const char* pattern, const char* mask)
{
  MODULEINFO mInfo = GetModuleInfo(module);
  DWORDLONG base = (DWORDLONG)mInfo.lpBaseOfDll;
  DWORDLONG size = (DWORDLONG)mInfo.SizeOfImage;
  DWORDLONG patternLength = (DWORDLONG)strlen(mask);
  for (DWORDLONG i = 0; i < size - patternLength; i++) {
    bool found = true;
    for (DWORDLONG j = 0; j < patternLength; j++) {
      found &= mask[j] == '?' || pattern[j] == *(char*)(base + i + j);
    }
    if (found) {
      //			printf("Found %s: 0x%p\n", funcname, base + i);
      return base + i;
    }
  }
  std::cout << "Warning: Failed to locate function " << funcname << std::endl;
  //MessageBoxA(NULL, std::format("Warning: Failed to locate function %s\n", funcname).c_str(), "FindPattern", NULL);
  return NULL;
}