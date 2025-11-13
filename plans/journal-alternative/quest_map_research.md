# CommonLibSSE-NG Quest & Map API Research Report

## Summary
Research into CommonLibSSE-NG APIs for implementing "Show on Map" functionality for quests.

---

## 1. QUEST MANAGEMENT APIs

### 1.1 TESQuest Class (`/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/T/TESQuest.h`)

#### Quest State Methods
- **`bool IsActive() const`** (Line 241)
  - Checks if quest has `QuestFlag::kActive` flag set
  - Returns: `data.flags.all(QuestFlag::kActive)`
  - **Status**: Accessible

- **`bool IsEnabled() const`** (Line 243)
  - Checks if quest has `QuestFlag::kEnabled` flag
  - Returns: `data.flags.all(QuestFlag::kEnabled)`
  - **Status**: Accessible

- **`bool IsCompleted() const`** (Line 242)
  - Checks if quest has `QuestFlag::kCompleted` flag
  - **Status**: Accessible

- **`bool Start()`** (Line 251, src/TESQuest.cpp:101-110)
  - Starts the quest via `EnsureQuestStarted(result, true)`
  - Must not be an event-scoped quest
  - **Status**: Accessible - **IMPORTANT FOR "SHOW ON MAP"**

- **`void Stop()`** (Line 253, src/TESQuest.cpp:117-122)
  - Stops quest by calling `SetEnabled(false)` if enabled
  - **Status**: Accessible

- **`void SetEnabled(bool a_set)`** (Line 250, src/TESQuest.cpp:91-99)
  - Sets or resets `QuestFlag::kEnabled` flag
  - Adds change flag `kQuestFlags`
  - **Status**: Accessible

#### Quest Data Structure (Lines 256-283)
```cpp
BSTArray<BGSQuestInstanceText*> instanceData;    // 038
BSTArray<BGSBaseAlias*> aliases;                 // 058
BSSimpleList<BGSQuestObjective*> objectives;     // 0F8
std::uint16_t currentStage;                      // 228
```
- Quest contains objectives with targets
- Objectives array contains quest objectives data

---

## 2. MAP MARKER APIS

### 2.1 ExtraMapMarker Class (`/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/E/ExtraMapMarker.h`)

#### MapMarkerData Structure (Lines 81-107)
```cpp
class MapMarkerData {
  TESFullName locationName;           // 00
  Flag flags;                         // 10 (8 bits)
  MARKER_TYPE type;                   // 12 (16 bits)
};
```

#### Marker Type Enum (Lines 9-79)
```
kNone = 0
kCity, kTown, kSettlement ... kDwemer Ruin ... 
kQuestTarget = 62          // IMPORTANT: Quest target marker type
kMultipleQuestTarget = 63
kPlayerSet = 64
kYouAreHere = 65
```

#### MapMarkerData Flags (Lines 84-95)
```cpp
enum Flag {
  kNone = 0,
  kVisible = 1 << 0,        // Is visible on map
  kCanTravelTo = 1 << 1,    // Can fast travel to
  kShowAllHidden = 1 << 2,  // Show even when hidden
  kUnk3-7 = ...
};
```

#### Key Methods
```cpp
void SetHidden(bool a_value)   // Line 97
void SetVisible(bool a_value)  // Line 98
```

**Status**: Basic data structure accessible, but modifying runtime markers requires reference access

### 2.2 MapMenu Class (`/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/M/MapMenu.h`)

#### MapMenu Structure (Lines 23-200)
- Derives from `IMenu`, `BSTEventSink<MenuOpenCloseEvent>`, `IMapCameraCallbacks`
- Contains `MapCamera` for positioning/zoom
- Has local map menu component
- Maintains map marker data

#### Key Methods
- **`void PlaceMarker()`** (Lines 135-140)
  - Relocatable function: `RELOCATION_ID(52226, 53113)`
  - Places a map marker at current location
  - **Status**: Accessible via relocation

#### Runtime Data
```cpp
struct RUNTIME_DATA {
  BSTSmartPointer<MapMoveHandler> moveHandler;
  BSTSmartPointer<MapLookHandler> lookHandler;
  BSTSmartPointer<MapZoomHandler> zoomHandler;
  ObjectRefHandle mapMarker;           // Current map marker
  LocalMapMenu localMapMenu;
};
```

**Status**: UI-level access available but limited to PlaceMarker() functionality

### 2.3 MapCamera Class (`/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/M/MapCamera.h`)

- Controls map camera position and zoom
- Has world space reference
- Nested state classes for transitions
- **Status**: Available but complex for direct manipulation

---

## 3. QUEST OBJECTIVE & TARGET APIs

### 3.1 BGSQuestObjective Class (Lines 160-174 of TESQuest.h)
```cpp
class BGSQuestObjective {
  BSFixedString displayText;           // 00 - NNAM
  TESQuest* ownerQuest;                // 08
  TESQuestTarget** targets;            // 10 - QSTA (targets array)
  std::uint32_t numTargets;            // 18
  std::uint16_t index;                 // 1C - QOBJ
  QUEST_OBJECTIVE_STATE state;         // 1E
};
```

### 3.2 TESQuestTarget Class (Lines 141-158 of TESQuest.h)
```cpp
class TESQuestTarget  // QSTA
{
  std::uint64_t unk00;
  TESCondition conditions;
  std::uint8_t alias;                  // 10
  std::uint16_t unk12;
  std::uint32_t unk14;
};
```

### 3.3 BGSInstancedQuestObjective (Lines 9-17 of BGSInstancedQuestObjective.h)
```cpp
class BGSInstancedQuestObjective {
  BGSQuestObjective* Objective;
  std::uint32_t instanceID;
  QUEST_OBJECTIVE_STATE InstanceState;
};
```

**Status**: Objective structures defined but low-level manipulation limited

---

## 4. PLAYER QUEST TRACKING

### 4.1 PlayerCharacter Quest Tracking (`/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/P/PlayerCharacter.h`)

#### Key Data Members (Lines 652-654)
```cpp
BSSimpleList<TESQuestStageItem*> questLog;
BSTArray<BGSInstancedQuestObjective> objectives;
BSTHashMap<TESQuest*, BSTArray<TESQuestTarget*>*> questTargets;
```

#### Marker-Related Flag
```cpp
bool forceQuestTargetRepath: 1;  // Line 279 - "Updates quest target in compass"
```

**Status**: Quest tracking exists, flag for compass update available

---

## 5. GUIDE EFFECT (Quest Marker Handler)

### 5.1 GuideEffect Class (`/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/G/GuideEffect.h`)

```cpp
class GuideEffect : public ActiveEffect {
  TESQuest* quest;                     // 90
  TESQuestTarget* questTarget;         // 98
  BSTArray<ObjectRefHandle> hazards;   // A0
};
```

- Active effect that manages quest markers
- Links quest to quest target
- Manages hazard markers
- **Status**: Encapsulates quest marker logic, but access limited

---

## 6. LOCATION & MARKER DATA

### 6.1 BGSLocation Class (`/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/B/BGSLocation.h`)

```cpp
class BGSLocation : public TESForm, TESFullName, BGSKeywordForm {
  BGSLocation* parentLoc;              // 48 - Parent location
  ObjectRefHandle worldLocMarker;      // 60 - MNAM (world location marker ref)
  float worldLocRadius;                // 64 - RNAM
  ObjectRefHandle horseLocMarker;      // 68 - NAM0
  BSTArray<SpecialRefData> specialRefs;  // 70
  BSTArray<UniqueNPCData> uniqueNPCs;    // 88
};
```

**Status**: Locations can have map markers, but references are ObjectRefHandles

### 6.2 TESObjectREFR (Reference) (`/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/T/TESObjectREFR.h`)

#### Key Location/Position Methods (Lines 389-426)
```cpp
BGSLocation* GetCurrentLocation() const;
BGSLocation* GetEditorLocation() const;
bool GetEditorLocation(NiPoint3& outPos, NiPoint3& outRot, ...);
NiPoint3 GetPosition() const;
TESWorldSpace* GetWorldspace() const;
float GetWaterHeight() const;
```

**Status**: Can retrieve location and position data, essential for "Show on Map"

---

## FEASIBILITY ASSESSMENT

### Goal 1: Set Active Quest Programmatically
**Feasibility: YES (10/10)**
- Method: `TESQuest::Start()` or `SetEnabled(true)`
- Implementation: Direct API call on quest form
- Risk: Low - well-defined public API
- **Code Path**: `TESQuest::Start()` -> `EnsureQuestStarted(result, true)`

### Goal 2: Update Quest Markers on Map
**Feasibility: PARTIAL (4/10)**
- **Challenge**: Map markers are internal UI state managed by MapMenu
- **What's Available**:
  - MapMarkerData structure for reading/writing marker properties
  - MapMenu::PlaceMarker() for placing player's custom marker
  - Quest targets are trackable via PlayerCharacter::questTargets
- **What's Missing**:
  - Direct API to modify quest marker appearance/visibility
  - No public method to update compass marker
  - GuideEffect manages markers but no public API
- **Workaround**: Could potentially:
  1. Manipulate ExtraMapMarker on quest objectives
  2. Force quest state update to refresh markers
  3. Use forceQuestTargetRepath flag to trigger compass update

### Goal 3: Open Map to Specific Location
**Feasibility: PARTIAL (5/10)**
- **Challenge**: MapMenu is complex with deep nesting
- **What's Available**:
  - MapCamera for positioning/zoom
  - MapMenu singleton access via MenuManager
  - LocalMapMenu for detailed view
- **What's Missing**:
  - No public API to set map center/focus point
  - Camera manipulation is low-level
  - Would need reverse-engineering or relocation IDs
- **Workaround**: Could potentially:
  1. Access MapCamera and manipulate position directly
  2. Use MapMenu::PlaceMarker() then open map manually
  3. Leverage LocalMapMenu camera positioning

### Goal 4: Overall "Show on Map" Feasibility
**RATING: 6/10 - MODERATELY FEASIBLE**

#### Implementation Path:
1. ✓ **Quest Activation**: Use `TESQuest::Start()` - Direct and reliable
2. ✓ **Quest Objectives**: Access via `quest->objectives` - Direct access
3. ✓ **Location Data**: Use `TESObjectREFR::GetPosition()` & `GetWorldspace()` - Available
4. ? **Map Marker Display**: Limited direct control, would require:
   - Manual marker creation via ExtraMapMarker manipulation
   - Or leveraging GuideEffect internally
   - Or UI scripting via GFx callback
5. ? **Map Focus**: MapCamera exists but needs reverse-engineering for practical use

#### Realistic Implementation Strategy:
1. **Start the quest** ✓ - `quest->Start()`
2. **Get quest objective targets** ✓ - Access `quest->objectives`
3. **Retrieve target location** ✓ - Get position from reference
4. **Show on map** ? - Options:
   - Option A: Use `MapMenu::PlaceMarker()` - Limited functionality
   - Option B: Manipulate ExtraMapMarker directly on quest location reference
   - Option C: Create custom marker reference with quest marker type
   - Option D: Use SKSE papyrus scripting layer (if available)

#### Limitations:
- No public API for compass marker management
- MapMenu UI state is internal and complex
- Quest markers automatically disappear when quest completes
- No way to force map to focus on specific coordinates
- Would likely need SKSE papyrus integration for robust solution

---

## RECOMMENDATION

### For "Show on Map" Feature:
**Use hybrid approach**:
1. C++ Side (CommonLibSSE-NG):
   - Start quest: `quest->Start()`
   - Access objectives and locations
   - Potentially create physical map marker reference

2. Papyrus Side (if available):
   - Use papyrus `ShowOnMap()` function if available
   - Leverage built-in quest UI functions
   - Fall back to UI manipulation via GFx

3. Fallback:
   - Manual compass marker placement
   - Journal menu manipulation
   - UI notification to player

### Recommended API Calls:
```cpp
// Activate quest
if (quest && \!quest->IsActive()) {
    quest->Start();  // Available: TESQuest.h:251
}

// Get quest objectives
auto& objectives = quest->objectives;  // Available: TESQuest.h:268

// Access objective targets
for (auto obj : objectives) {
    auto targets = obj->targets;       // Available: TESQuest.h:166
    auto numTargets = obj->numTargets; // Available: TESQuest.h:167
}

// Get location data
for (auto target : targets) {
    auto location = target->/* location alias */;  // Via alias reference
}

// Potentially force compass update
auto player = PlayerCharacter::GetSingleton();
player->forceQuestTargetRepath = true;  // Line 279, PlayerCharacter.h
```

---

## SOURCE FILES SUMMARY

| File | Lines | Purpose | Accessibility |
|------|-------|---------|-----------------|
| `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/T/TESQuest.h` | 1-285 | Quest management | HIGH |
| `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/src/RE/T/TESQuest.cpp` | 1-123 | Quest implementation | HIGH |
| `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/E/ExtraMapMarker.h` | 1-125 | Map marker data | MEDIUM |
| `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/M/MapMenu.h` | 1-209 | Map UI | LOW |
| `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/M/MapCamera.h` | 1-104 | Map camera | LOW |
| `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/P/PlayerCharacter.h` | 229-279 | Player quest tracking | MEDIUM |
| `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/G/GuideEffect.h` | 1-30 | Quest marker effect | LOW |
| `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/B/BGSLocation.h` | 1-140 | Location data | MEDIUM |

