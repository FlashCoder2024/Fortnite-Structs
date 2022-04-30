#pragma once

#include <Windows.h>
#include <string>
#include <locale>
#include <format>
#include <iostream>
#include <chrono>
#include <thread>

#include "other.h"
#include "xorstr.hpp" 
#include <regex>

using namespace std::chrono;

#define INL __forceinline

static inline void (*ToStringO)(struct FName*, class FString&);
static inline void* (*ProcessEventO)(void*, void*, void*);

std::string FN_Version;
int Engine_Version;

static struct FChunkedFixedUObjectArray* ObjObjects;
static struct FFixedUObjectArray* OldObjects;

namespace FMemory
{
	void (*Free)(void* Original);
	void (*Realloc)(void* Original, SIZE_T Count, uint32_t Alignment /* = DEFAULT_ALIGNMENT */);
}

struct Timer
{
	time_point<std::chrono::steady_clock> start, end;
	duration<float> dura;

	Timer()
	{
		start = high_resolution_clock::now();
	}

	~Timer()
	{
		end = high_resolution_clock::now();
		dura = end - start;

		float ms = dura.count() * 1000.0f;
		std::cout << _("Took ") << ms << _("ms \n");
	}

	// You would do "Timer* t = new Timer;" and then delete at the end of the function.
};

template <class ElementType>
class TArray // https://github.com/EpicGames/UnrealEngine/blob/4.21/Engine/Source/Runtime/Core/Public/Containers/Array.h#L305
{
	friend class FString;
protected:
	ElementType* Data = nullptr;
	int32_t ArrayNum = 0;
	int32_t ArrayMax = 0;
public:
	void Free()
	{
		if (FMemory::Free)
			FMemory::Free(Data);

		Data = nullptr; // incase someone for some reason tries to access it.

		ArrayNum = 0;
		ArrayMax = 0;
	}

	INL ElementType& operator[](int Index) const { return Data[Index]; }

	INL ElementType& At(int Index) const { return Data[Index]; }

	INL int32_t Slack() const
	{
		return ArrayMax - ArrayNum;
	}

	INL void Reserve(int Number)
	{
		if (!FMemory::Realloc)
		{
			MessageBoxA(0, _("How are you expecting to reserve with no Realloc?"), _("Fortnite"), MB_ICONERROR);
			return;
		}

		if (Number > ArrayMax)
			Data = Slack() >= ArrayNum ? Data : (ElementType*)FMemory::Realloc(Data, (ArrayMax = ArrayNum + ArrayNum) * sizeof(ElementType), 0); // thanks fischsalat for this line of code.
	}

	INL void Add(const ElementType& New)
	{
		Reserve(1);
		Data[ArrayNum] = New;
		++ArrayNum;
	};

	INL void RemoveSingle(const ElementType& Index) // TODO
	{

	}
};

class FString // https://github.com/EpicGames/UnrealEngine/blob/4.21/Engine/Source/Runtime/Core/Public/Containers/UnrealString.h#L59
{
public:
	TArray<TCHAR> Data;

	void Set(const wchar_t* NewStr) // by fischsalat
	{
		if (!NewStr || std::wcslen(NewStr) == 0) return;

		Data.ArrayMax = Data.ArrayNum = *NewStr ? (int)std::wcslen(NewStr) + 1 : 0;

		if (Data.ArrayNum)
			Data.Data = const_cast<wchar_t*>(NewStr);
	}

	std::string ToString() const
	{
		auto length = std::wcslen(Data.Data);
		std::string str(length, '\0');
		std::use_facet<std::ctype<wchar_t>>(std::locale()).narrow(Data.Data, Data.Data + length, '?', &str[0]);

		return str;
	}

	INL void operator=(const std::string& str) { Set(std::wstring(str.begin(), str.end()).c_str()); }

	void FreeString()
	{
		Data.Free();
	}

	~FString()
	{
		// FreeString();
	}

	/*

	template <typename StrType>
	FORCEINLINE FString& operator+=(StrType&& Str)	{ return Append(Forward<StrType>(Str)); }

	FORCEINLINE FString& operator+=(ANSICHAR Char) { return AppendChar(Char); }
	FORCEINLINE FString& operator+=(WIDECHAR Char) { return AppendChar(Char); }
	FORCEINLINE FString& operator+=(UCS2CHAR Char) { return AppendChar(Char); }

	*/
};

struct FName // https://github.com/EpicGames/UnrealEngine/blob/c3caf7b6bf12ae4c8e09b606f10a09776b4d1f38/Engine/Source/Runtime/Core/Public/UObject/NameTypes.h#L403
{
	uint32_t ComparisonIndex;
	uint32_t Number;

	INL std::string ToString()
	{
		FString temp;

		ToStringO(this, temp);

		auto Str = temp.ToString();

		temp.FreeString();

		return Str;
	}
};

struct UObject // https://github.com/EpicGames/UnrealEngine/blob/c3caf7b6bf12ae4c8e09b606f10a09776b4d1f38/Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectBase.h#L20
{
	void** VFTable;
	int32_t ObjectFlags;
	int32_t InternalIndex;
	UObject* ClassPrivate; // Keep it an object because the user will have to cast it to the correct type depending on the version.
	FName NamePrivate;
	UObject* OuterPrivate;

	INL std::string GetName() { return NamePrivate.ToString(); }

	INL std::string GetFullName()
	{
		std::string temp;

		for (auto outer = OuterPrivate; outer; outer = outer->OuterPrivate)
			temp = std::format("{}.{}", outer->GetName(), temp);

		return std::format("{} {}{}", ClassPrivate->GetName(), temp, this->GetName());
	}

	template <typename MemberType>
	INL MemberType* Member(const std::string& MemberName);

	bool IsA(UObject* cmp) const;

	INL struct UFunction* Function(const std::string& FuncName);

	INL auto ProcessEvent(UObject* Function, void* Params)
	{
		return ProcessEventO(this, Function, Params);
	}

	INL auto ProcessEvent(const std::string& FuncName, void* Params)
	{
		return ProcessEvent((UObject*)this->Function(FuncName), Params);
	}
};

struct UFunction : UObject {}; // TODO: Add acutal stuff to this

struct FUObjectItem // https://github.com/EpicGames/UnrealEngine/blob/4.27/Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectArray.h#L26
{
	UObject* Object;
	int32_t Flags;
	int32_t ClusterRootIndex;
	int32_t SerialNumber;
	// int pad_01;
};

struct FFixedUObjectArray
{
	FUObjectItem* Objects;
	int32_t MaxElements;
	int32_t NumElements;

	INL const int32_t Num() const { return NumElements; }

	INL const int32_t Capacity() const { return MaxElements; }

	INL bool IsValidIndex(int32_t Index) const { return Index < Num() && Index >= 0; }

	INL UObject* GetObjectById(int32_t Index) const
	{
		return Objects[Index].Object;
	}
};

struct FChunkedFixedUObjectArray // https://github.com/EpicGames/UnrealEngine/blob/7acbae1c8d1736bb5a0da4f6ed21ccb237bc8851/Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectArray.h#L321
{
	enum
	{
		NumElementsPerChunk = 64 * 1024,
	};

	FUObjectItem** Objects;
	FUObjectItem* PreAllocatedObjects;
	int32_t MaxElements;
	int32_t NumElements;
	int32_t MaxChunks;
	int32_t NumChunks;

	INL const int32_t Num() const { return NumElements; }

	INL const int32_t Capacity() const { return MaxElements; }

	INL UObject* GetObjectById(int32_t Index) const
	{
		if (Index > NumElements || Index < 0) return nullptr;

		const int32_t ChunkIndex = Index / NumElementsPerChunk;
		const int32_t WithinChunkIndex = Index % NumElementsPerChunk;

		if (ChunkIndex > NumChunks) return nullptr;
		FUObjectItem* Chunk = Objects[ChunkIndex];
		if (!Chunk) return nullptr;

		auto obj = (Chunk + WithinChunkIndex)->Object;

		return obj;
	}
};

template <typename ReturnType = UObject>
static ReturnType* FindObject(const std::string& str, bool bIsEqual = false, bool bIsName = false)
{
	if (bIsName) bIsEqual = true;

	for (int32_t i = 0; i < (ObjObjects ? ObjObjects->Num() : OldObjects->Num()); i++)
	{
		auto Object = ObjObjects ? ObjObjects->GetObjectById(i) : OldObjects->GetObjectById(i);
		
		if (!Object) continue;
		
		auto ObjectName = bIsName ? Object->GetName() : Object->GetFullName();

		if (bIsEqual)
		{
			if (ObjectName == str)
				return (ReturnType*)Object;
		}
		else
		{
			if (ObjectName.contains(str))
				return (ReturnType*)Object;
		}
	}

	return nullptr;
}

// Here comes the version changing and makes me want to die I need to find a better way to do this

struct UField : UObject
{
	UField* Next;
};

struct UFieldPadding : UObject
{
	UField* Next;
	void* pad_01;
	void* pad_02;
};

struct UProperty_UE : public UField // Default UProperty for UE, >4.20.
{
	int32_t ArrayDim;
	int32_t ElementSize;
	uint64_t PropertyFlags;
	uint16_t RepIndex;
	TEnumAsByte<ELifetimeCondition> BlueprintReplicationCondition;
	int32_t Offset_Internal;
	FName RepNotifyFunc;
	UProperty_UE* PropertyLinkNext;
	UProperty_UE* NextRef;
	UProperty_UE* DestructorLinkNext;
	UProperty_UE* PostConstructLinkNext;
};

struct UStruct_FT : public UField // >4.20
{
	UStruct_FT* SuperStruct;
	UField* ChildProperties; // Children
	int32_t PropertiesSize;
	int32_t MinAlignment;
	TArray<uint8_t> Script;
	UProperty_UE* PropertyLink;
	UProperty_UE* RefLink;
	UProperty_UE* DestructorLink;
	UProperty_UE* PostConstructLink;
	TArray<UObject*> ScriptObjectReferences;
};

struct UProperty_FTO : UField
{
	uint32_t ArrayDim; // 0x30
	uint32_t ElementSize; // 0x34
	uint64_t PropertyFlags; // 0x38
	char pad_40[4]; // 0x40
	uint32_t Offset_Internal; // 0x44
	char pad_48[0x70 - 0x48];
};

struct UStruct_FTO : public UField // 4.21 only
{
	UStruct_FTO* SuperStruct;
	UField* ChildProperties; // Children
	int32_t PropertiesSize;
	int32_t MinAlignment;
	UProperty_FTO* PropertyLink;
	UProperty_FTO* RefLink;
	UProperty_FTO* DestructorLink;
	UProperty_FTO* PostConstructLink;
	TArray<UObject*> ScriptObjectReferences;
};

struct UStruct_FTT : UField // 4.22-4.24
{
	void* Pad;
	void* Pad2;
	UStruct_FTT* SuperStruct; // 0x30
	UField* ChildProperties; // 0x38
	uint32_t PropertiesSize; // 0x40
	char pad_44[0x88 - 0x30 - 0x14];
};

struct FField
{
	void** VFT;
	void* ClassPrivate;
	void* Owner;
	void* pad;
	FField* Next;
	FName Name;
	EObjectFlags FlagsPrivate;

	std::string GetName()
	{
		return Name.ToString();
	}
};

struct FProperty : public FField
{
	int32_t	ArrayDim;
	int32_t	ElementSize;
	EPropertyFlags PropertyFlags;
	uint16_t RepIndex;
	TEnumAsByte<ELifetimeCondition> BlueprintReplicationCondition;
	int32_t	Offset_Internal;
	FName RepNotifyFunc;
	FProperty* PropertyLinkNext;
	FProperty* NextRef;
	FProperty* DestructorLinkNext;
	FProperty* PostConstructLinkNext;
};

class UStruct_CT : public UFieldPadding
{
public:
	UStruct_CT* SuperStruct;
	UFieldPadding* Children;
	FField* ChildProperties;
	int32_t PropertiesSize;
	int32_t MinAlignment;
	TArray<uint8_t> Script;
	FProperty* PropertyLink;
	FProperty* RefLink;
	FProperty* DestructorLink;
	FProperty* PostConstructLink;
	TArray<UObject*> ScriptAndPropertyObjectReferences;
};

struct UClass_FT : public UStruct_FT {}; // >4.20
struct UClass_FTO : public UStruct_FTO {}; // 4.21
struct UClass_FTT : public UStruct_FTT {}; // 4.22-4.24
struct UClass_CT : public UStruct_CT {}; // C2 to before C3

template <typename ClassType, typename PropertyType, typename ReturnValue = PropertyType>
auto GetMembers(UObject* Object)
{
	std::vector<ReturnValue*> Members;

	if (Object)
	{
		for (auto CurrentClass = (ClassType*)Object->ClassPrivate; CurrentClass; CurrentClass = (ClassType*)CurrentClass->SuperStruct)
		{
			auto Property = CurrentClass->ChildProperties;

			if (Property)
			{
				auto Next = Property->Next;

				if (Next)
				{
					Members.push_back((ReturnValue*)Property);

					while (Property)
					{
						Members.push_back((ReturnValue*)Property);

						Property = Property->Next;
					}
				}
			}

		}
	}

	return Members;
}

auto GetMembersAsObjects(UObject* Object)
{
	std::vector<UObject*> Members;

	if (Engine_Version <= 420)
		Members = GetMembers<UClass_FT, UProperty_UE, UObject>(Object);

	else if (Engine_Version == 421) // && Engine_Version <= 424)
		Members = GetMembers<UClass_FTO, UProperty_FTO, UObject>(Object);

	else if (Engine_Version >= 422 && Engine_Version <= 424)
		Members = GetMembers<UClass_FTT, UProperty_FTO, UObject>(Object);

	else if (Engine_Version >= 425 && Engine_Version < 500)
		Members = GetMembers<UClass_CT, FProperty, UObject>(Object);

	else if (Engine_Version >= 500)
		Members = GetMembers<UClass_CT, FProperty, UObject>(Object);

	return Members;
}

std::vector<std::string> GetMemberNames(UObject* Object)
{
	std::vector<std::string> Names;
	std::vector<UObject*> Members = GetMembersAsObjects(Object);

	for (auto Member : Members)
		Names.push_back(Member->GetName());

	return Names;
}

UFunction* FindFunction(const std::string& Name, UObject* Object) // might as well pass in object because what else u gon use a func for.
{
	for (auto Member : GetMembersAsObjects(Object))
	{
		if (Member->GetName() == Name) // dont use IsA cuz slower
			return (UFunction*)Member;
	}
}

template <typename ClassType, typename PropertyType>
int LoopMembersAndFindOffset(UObject* Object, const std::string& MemberName, int offset = 0)
{
	// We loop through the whole class hierarchy to find the offset.

	for (auto Member : GetMembers<ClassType, PropertyType>(Object))
	{
		if (Member->GetName() == MemberName)
		{
			if (!offset)
				return ((PropertyType*)Member)->Offset_Internal;
			else
				return *(int*)(__int64(Member) + offset);
		}
	}

	return 0;
}

static int GetOffset(UObject* Object, const std::string& MemberName)
{
	if (Object && !MemberName.contains(_(" ")))
	{
		if (Engine_Version <= 420)
			return LoopMembersAndFindOffset<UClass_FT, UProperty_UE>(Object, MemberName);

		else if (Engine_Version == 421) // && Engine_Version <= 424)
			return LoopMembersAndFindOffset<UClass_FTO, UProperty_FTO>(Object, MemberName);

		else if (Engine_Version >= 422 && Engine_Version <= 424)
			return LoopMembersAndFindOffset<UClass_FTT, UProperty_FTO>(Object, MemberName);

		else if (Engine_Version >= 425 && Engine_Version < 500)
			return LoopMembersAndFindOffset<UClass_CT, FProperty>(Object, MemberName);
		
		else if (Engine_Version >= 500)
			return LoopMembersAndFindOffset<UClass_CT, FProperty>(Object, MemberName, 0x44);
	}

	else
	{
		std::cout << std::format(_("Either invalid object or MemberName. MemberName {} Object {}"), MemberName, Object->GetFullName());
	}

	return 0;
}

template <typename MemberType>
INL MemberType* UObject::Member(const std::string& MemberName)
{
	// MemberName.erase(0, MemberName.find_last_of(".", MemberName.length() - 1) + 1); // This would be getting the short name of the member if you did like ObjectProperty /Script/stuff

	return (MemberType*)(__int64(this) + GetOffset(this, MemberName));
}

template <typename ClassType>
bool IsA_(const UObject* cmpto, UObject* cmp)
{
	for (auto super = (ClassType*)cmpto->ClassPrivate; super; super = (ClassType*)super->SuperStruct)
	{
		if (super == cmp)
			return true;
	}

	return false;
}

bool UObject::IsA(UObject* cmp) const
{
	if (Engine_Version <= 420)
		return IsA_<UClass_FT>(this, cmp);

	else if (Engine_Version == 421) // && Engine_Version <= 424)
		return IsA_<UClass_FTO>(this, cmp);

	else if (Engine_Version >= 422 && Engine_Version < 425)
		return IsA_<UClass_FTT>(this, cmp);

	else if (Engine_Version >= 425)
		return IsA_<UClass_CT>(this, cmp);

	return false;
}

INL UFunction* UObject::Function(const std::string& FuncName)
{
	return FindFunction(FuncName, this);
}

FString(*GetEngineVersion)();

// TODO: There is this 1.9 function, 48 8D 05 D9 51 22 03. It has the CL and stuff. We may be able to determine the version using the CL.

bool Setup(/* void* ProcessEventHookAddr */)
{
	uint64_t ToStringAddr = 0;
	uint64_t ProcessEventAddr = 0;
	uint64_t ObjectsAddr = 0;
	uint64_t FreeMemoryAddr = 0;
	bool bOldObjects = false;

	GetEngineVersion = decltype(GetEngineVersion)(FindPattern(_("40 53 48 83 EC 20 48 8B D9 E8 ? ? ? ? 48 8B C8 41 B8 04 ? ? ? 48 8B D3")));

	std::string FullVersion;
	FString toFree;

	if (!GetEngineVersion)
	{
		auto VerStr = FindPattern(_("2B 2B 46 6F 72 74 6E 69 74 65 2B 52 65 6C 65 61 73 65 2D ? ? ? ?"));

		if (!VerStr)
		{
			MessageBoxA(0, _("Failed to find fortnite version!"), _("Fortnite"), MB_ICONERROR);
			return false;
		}

		FullVersion = decltype(FullVersion.c_str())(VerStr);
		Engine_Version = 500;
	}

	else
	{
		toFree = GetEngineVersion();
		FullVersion = toFree.ToString();
	}

	std::string FNVer = FullVersion;
	std::string EngineVer = FullVersion;

	if (!FullVersion.contains(_("Live")) && !FullVersion.contains(_("Next")) && !FullVersion.contains(_("Cert")))
	{
		if (Engine_Version < 500)
		{
			FNVer.erase(0, FNVer.find_last_of(_("-"), FNVer.length() - 1) + 1);
			EngineVer.erase(EngineVer.find_first_of(_("-"), FNVer.length() - 1), 40);

			if (EngineVer.find_first_of(".") != EngineVer.find_last_of(".")) // this is for 4.21.0 and itll remove the .0
				EngineVer.erase(EngineVer.find_last_of(_(".")), 2);

			Engine_Version = std::stod(EngineVer) * 100;
		}

		else
		{
			const std::regex base_regex(_("-([0-9.]*)-"));
			std::cmatch base_match;

			std::regex_search(FullVersion.c_str(), base_match, base_regex);

			FNVer = base_match[1];
		}

		FN_Version = FNVer;

		// if (FN_Version >= 16.00 && FN_Version < 18.40)
			// Engine_Version = 4.27; // 4.26.1;
	}

	else
	{
		Engine_Version = 419;
		FN_Version = 2.69;
	}
	
	if (Engine_Version >= 416 && Engine_Version <= 420)
	{
		ObjectsAddr = FindPattern(_("48 8B 05 ? ? ? ? 48 8D 1C C8 81 4B ? ? ? ? ? 49 63 76 30"), false, 7, true);

		if (!ObjectsAddr)
			ObjectsAddr = FindPattern(_("48 8B 05 ? ? ? ? 48 8D 14 C8 EB 03 49 8B D6 8B 42 08 C1 E8 1D A8 01 0F 85 ? ? ? ? F7 86 ? ? ? ? ? ? ? ?"), false, 7, true);

		if (Engine_Version == 420)
			ToStringAddr = FindPattern(_("48 89 5C 24 ? 57 48 83 EC 40 83 79 04 00 48 8B DA 48 8B F9 75 23 E8 ? ? ? ? 48 85 C0 74 19 48 8B D3 48 8B C8 E8 ? ? ? ? 48"));
		else
		{
			ToStringAddr = FindPattern(_("40 53 48 83 EC 40 83 79 04 00 48 8B DA 75 19 E8 ? ? ? ? 48 8B C8 48 8B D3 E8 ? ? ? ?"));

			if (!ToStringAddr) // This means that we are in season 1 (i think).
			{
				ToStringAddr = FindPattern(_("48 89 5C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 83 EC 20 48 8B DA 4C 8B F1 E8 ? ? ? ? 4C 8B C8 41 8B 06 99"));

				if (ToStringAddr)
					Engine_Version = 416;
			}
		}

		FreeMemoryAddr = FindPattern(_("48 85 C9 74 1D 4C 8B 05 ? ? ? ? 4D 85 C0 0F 84 ? ? ? ? 49"));

		if (!FreeMemoryAddr)
			FreeMemoryAddr = FindPattern(_("48 85 C9 74 2E 53 48 83 EC 20 48 8B D9 48 8B 0D ? ? ? ? 48 85 C9 75 0C E8 ? ? ? ? 48 8B 0D ? ? ? ? 48"));

		ProcessEventAddr = FindPattern(_("40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC ? ? ? ? 48 8D 6C 24 ? 48 89 9D ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C5 48 89 85 ? ? ? ? 48 63 41 0C 45 33 F6"));

		bOldObjects = true;
	}

	if (Engine_Version >= 421 && Engine_Version <= 424)
	{
		ToStringAddr = FindPattern(_("48 89 5C 24 ? 57 48 83 EC 30 83 79 04 00 48 8B DA 48 8B F9"));
		ProcessEventAddr = FindPattern(_("40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC ? ? ? ? 48 8D 6C 24 ? 48 89 9D ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C5 48 89 85 ? ? ? ? ? ? ? 45 33 F6"));
	}

	if (Engine_Version >= 425 && Engine_Version < 500)
	{
		ToStringAddr = FindPattern(_("48 89 5C 24 ? 55 56 57 48 8B EC 48 83 EC 30 8B 01 48 8B F1 44 8B 49 04 8B F8 C1 EF 10 48 8B DA 0F B7 C8 89 4D 24 89 7D 20 45 85 C9"));
		ProcessEventAddr = FindPattern(_("40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC ? ? ? ? 48 8D 6C 24 ? 48 89 9D ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C5 48 89 85 ? ? ? ? 8B 41 0C 45 33 F6"));
	}

	if (Engine_Version >= 421 && Engine_Version <= 426)
	{
		ObjectsAddr = FindPattern(_("48 8B 05 ? ? ? ? 48 8B 0C C8 48 8D 04 D1 EB 03 48 8B ? 81 48 08 ? ? ? 40 49"), false, 7, true);
		FreeMemoryAddr = FindPattern(_("48 85 C9 74 2E 53 48 83 EC 20 48 8B D9"));
		bOldObjects = false;

		if (!ObjectsAddr)
			ObjectsAddr = FindPattern(_("48 8B 05 ? ? ? ? 48 8B 0C C8 48 8B 04 D1"), true, 3);
	}

	if (Engine_Version >= 426) // 4.26.1
	{
		ToStringAddr = FindPattern(_("48 89 5C 24 ? 48 89 74 24 ? 55 57 41 56 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 45 33 F6 48 8B F2 44 39 71 04 0F 85 ? ? ? ? 8B 19 0F B7 FB E8 ? ? ? ? 8B CB 48 8D 54 24 ? 48 C1 E9 10 8D 1C 3F 48 03 5C C8 ? 48 8B CB F6 03 01 0F 85 ? ? ? ? E8 ? ? ? ?"));
		ObjectsAddr = FindPattern(_("48 8B 05 ? ? ? ? 48 8B 0C C8 48 8B 04 D1"), true, 3);
	}

	if (Engine_Version >= 500)
	{
		ToStringAddr = FindPattern(_("48 89 5C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 84 24 ? ? ? ? 8B"));
		ProcessEventAddr = FindPattern(_("40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC ? ? ? ? 48 8D 6C 24 ? 48 89 9D ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C5 48 89 85 ? ? ? ? 45 33 ED"));
		FreeMemoryAddr = FindPattern(_("48 85 C9 0F 84 ? ? ? ? 48 89 5C 24 ? 57 48 83 EC 20 48 8B 3D ? ? ? ? 48 8B D9 48"));
	}

	if (!ToStringAddr)
	{
		MessageBoxA(NULL, _("Failed to find FName::ToString"), _("Fortnite"), MB_OK);
		return false;
	}

	ToStringO = decltype(ToStringO)(ToStringAddr);
	
	if (!ProcessEventAddr)
	{
		MessageBoxA(NULL, _("Failed to find UObject::ProcessEvent"), _("Fortnite"), MB_OK);
		return false;
	}

	ProcessEventO = decltype(ProcessEventO)(ProcessEventAddr);

	if (!FreeMemoryAddr)
	{
		MessageBoxA(NULL, _("Failed to find FMemory::Free"), _("Fortnite"), MB_OK);
		return false;
	}

	FMemory::Free = decltype(FMemory::Free)(FreeMemoryAddr);

	if (!ObjectsAddr)
	{
		MessageBoxA(NULL, _("Failed to find FUObjectArray::ObjObjects"), _("Fortnite"), MB_OK);
		return false;
	}

	if (bOldObjects)
		OldObjects = decltype(OldObjects)(ObjectsAddr);
	else
		ObjObjects = decltype(ObjObjects)(ObjectsAddr);

	toFree.FreeString();
}