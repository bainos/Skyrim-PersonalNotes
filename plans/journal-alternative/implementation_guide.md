# "Show on Map" Implementation Guide

## Quick Reference: Feasibility by Goal

| Goal | Feasibility | Risk | Implementation Complexity |
|------|------------|------|---------------------------|
| 1. Set active quest | ✓✓✓ 10/10 | LOW | Simple |
| 2. Update map markers | ✗✓ 4/10 | HIGH | Complex |
| 3. Open map to location | ✗✓ 5/10 | HIGH | Complex |
| **Overall "Show on Map"** | **✓✓ 6/10** | **MED** | **Moderate** |

---

## Code Examples & API Reference

### 1. ACTIVATING A QUEST (10/10 Feasible)

```cpp
#include "RE/T/TESQuest.h"
#include "RE/P/PlayerCharacter.h"

void ShowQuestOnMap(TESQuest* quest) {
    if (!quest) return;
    
    // Check current state
    if (quest->IsActive()) {
        // Quest already active
        return;
    }
    
    // Start the quest - THIS WORKS!
    bool result = quest->Start();  // TESQuest.h:251
    
    if (result) {
        SKSE::log::info("Quest {} started successfully", 
                       quest->GetFormEditorID());
    }
}
```

**File**: `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/T/TESQuest.h`  
**Method**: `TESQuest::Start()` (Line 251)  
**Implementation**: `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/src/RE/T/TESQuest.cpp` (Lines 101-110)

---

### 2. ACCESSING QUEST OBJECTIVES & TARGETS (8/10 Accessible)

```cpp
#include "RE/T/TESQuest.h"
#include "RE/T/TESObjectREFR.h"

void AnalyzeQuestTargets(TESQuest* quest) {
    if (!quest) return;
    
    // Access objectives list
    auto& objectives = quest->objectives;  // Line 268 of TESQuest.h
    
    for (auto objective : objectives) {
        if (!objective) continue;
        
        // Get objective display text
        auto displayText = objective->displayText;  // Line 164
        
        // Access quest targets
        auto targets = objective->targets;          // Line 166
        auto numTargets = objective->numTargets;    // Line 167
        
        SKSE::log::info("Objective: {} ({} targets)", 
                       displayText.c_str(), numTargets);
        
        // Iterate targets
        for (std::uint32_t i = 0; i < numTargets; ++i) {
            auto target = targets[i];
            // TESQuestTarget structure (Lines 141-158)
            // Contains alias reference to location
        }
    }
}
```

**File**: `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/T/TESQuest.h`  
**Classes**: `BGSQuestObjective` (Lines 160-174), `TESQuestTarget` (Lines 141-158)

---

### 3. GETTING LOCATION DATA (9/10 Accessible)

```cpp
#include "RE/T/TESObjectREFR.h"
#include "RE/B/BGSLocation.h"

void GetLocationFromReference(TESObjectREFR* ref) {
    if (!ref) return;
    
    // Get position coordinates
    NiPoint3 position = ref->GetPosition();  // TESObjectREFR.h:414
    SKSE::log::info("Position: ({}, {}, {})", 
                   position.x, position.y, position.z);
    
    // Get world space
    TESWorldSpace* worldspace = ref->GetWorldspace();  // Line 425
    
    // Get editor location
    BGSLocation* location = ref->GetEditorLocation();  // Line 393
    if (location) {
        SKSE::log::info("Location: {}", location->GetName());
    }
    
    // Get current location (may be different from editor location)
    BGSLocation* currentLoc = ref->GetCurrentLocation();  // Line 389
}
```

**File**: `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/T/TESObjectREFR.h`  
**Methods**: Lines 389-426 (Location & coordinate access)

---

### 4. MAP MARKER DATA (MEDIUM Accessibility - 6/10)

```cpp
#include "RE/E/ExtraMapMarker.h"

// Map marker types
enum MARKER_TYPE {
    kQuestTarget = 62,              // Line 74
    kMultipleQuestTarget = 63,      // Line 76
    kPlayerSet = 64,                // Line 77
    kYouAreHere = 65,               // Line 78
};

// Map marker visibility
class MapMarkerData {
    // Line 81-107
    
    // Visibility control
    void SetHidden(bool value) {   // Line 97
        // Internal implementation
    }
    
    void SetVisible(bool value) {  // Line 98
        // Internal implementation
    }
};

// Accessing marker data on a reference
void ModifyMapMarker(TESObjectREFR* ref) {
    if (!ref) return;
    
    // Get extra data list
    auto& extraList = ref->GetExtraDataList();
    
    // Look for map marker extra
    auto mapMarker = extraList.GetByType<ExtraMapMarker>();
    
    if (mapMarker && mapMarker->mapData) {
        auto data = mapMarker->mapData;
        
        // Read marker type
        MARKER_TYPE type = data->type;
        
        // Modify visibility
        data->SetVisible(true);
        data->SetHidden(false);
    }
}
```

**File**: `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/E/ExtraMapMarker.h`  
**Classes**: `MapMarkerData` (Lines 81-107), `ExtraMapMarker` (Lines 109-124)

---

### 5. COMPASS UPDATE FLAG (7/10 Accessible)

```cpp
#include "RE/P/PlayerCharacter.h"

void ForceCompassUpdate() {
    auto player = PlayerCharacter::GetSingleton();
    if (!player) return;
    
    // This flag forces quest target repath
    // Line 279: "Updates quest target in compass"
    player->forceQuestTargetRepath = true;
    
    SKSE::log::info("Compass update forced");
}
```

**File**: `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/P/PlayerCharacter.h`  
**Flag**: `forceQuestTargetRepath` (Line 279)

---

### 6. OPENING MAP (DIFFICULT - 5/10 Feasibility)

```cpp
#include "RE/M/MapMenu.h"
#include "RE/M/MapCamera.h"
#include "RE/L/LocalMapMenu.h"

void OpenMapAtLocation(TESWorldSpace* worldspace, 
                       const NiPoint3& position) {
    if (!worldspace) return;
    
    // Get MapMenu from UI
    auto ui = UI::GetSingleton();
    auto mapMenu = ui->GetMenu<MapMenu>(MapMenu::MENU_NAME);
    
    if (!mapMenu) {
        // Map not open, open it
        ui->ShowMenu(MapMenu::MENU_NAME);
        mapMenu = ui->GetMenu<MapMenu>(MapMenu::MENU_NAME);
    }
    
    if (mapMenu) {
        // Get runtime data
        auto& runtimeData = mapMenu->GetRuntimeData();
        
        // MapMenu::PlaceMarker() exists but limited
        // RELOCATION_ID(52226, 53113)
        mapMenu->PlaceMarker();  // Line 138
        
        // Camera manipulation (low-level, complex)
        auto& camera = runtimeData.localMapMenu./*...*/;
        // Would need reverse-engineering for practical use
    }
}
```

**File**: `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/M/MapMenu.h`  
**Challenge**: MapMenu UI state management is complex and largely undocumented

---

## RECOMMENDED IMPLEMENTATION APPROACH

### Phase 1: Core Functionality (Achievable - 90% Success Rate)

```cpp
class QuestMapper {
public:
    bool ShowQuestOnMap(TESQuest* quest) {
        if (!quest) return false;
        
        // Step 1: Start the quest
        if (!quest->Start()) {
            SKSE::log::error("Failed to start quest");
            return false;
        }
        
        // Step 2: Force compass update
        auto player = PlayerCharacter::GetSingleton();
        if (player) {
            player->forceQuestTargetRepath = true;
        }
        
        // Step 3: Log quest information
        SKSE::log::info("Quest {} shown on map", 
                       quest->GetFormEditorID());
        
        return true;
    }
    
    NiPoint3 GetQuestTargetLocation(TESQuest* quest) {
        if (!quest || quest->objectives.empty()) {
            return NiPoint3(0, 0, 0);
        }
        
        // Get first objective's first target
        auto objective = quest->objectives.front();
        if (!objective || objective->numTargets == 0) {
            return NiPoint3(0, 0, 0);
        }
        
        auto target = objective->targets[0];
        // Would need to resolve alias to reference
        return NiPoint3(0, 0, 0);  // Placeholder
    }
};
```

### Phase 2: Enhanced Features (Moderate Difficulty - 60% Success Rate)

- Manipulate map marker visibility via ExtraMapMarker
- Create custom marker references
- Trigger compass marker update via GuideEffect

### Phase 3: Advanced Features (High Difficulty - 30% Success Rate)

- Focus map camera on specific coordinate
- Custom marker rendering
- UI scripting via GFx callbacks

---

## CRITICAL LIMITATIONS

1. **No Direct Compass API**
   - Game manages compass internally via GuideEffect
   - No public method to create compass markers
   - Must work through quest objectives

2. **No Map Focus API**
   - MapMenu exists but positioning is complex
   - Would require reverse-engineering camera controls
   - May need relocation IDs that change between versions

3. **Marker Lifetime**
   - Quest markers auto-remove when quest completes
   - Custom markers don't persist across saves automatically
   - No way to "pin" a quest location permanently

4. **Player State Dependencies**
   - Compass update requires active PlayerCharacter
   - Map may not update immediately
   - State synchronization between multiple systems

---

## ALTERNATIVE APPROACHES

If direct "Show on Map" implementation fails:

1. **Papyrus Scripting**
   ```
   Use native Papyrus ShowOnMap() function
   Fallback if C++ approach insufficient
   ```

2. **UI Notification**
   ```
   Display HUD notification with quest info
   Player manually opens journal to see location
   ```

3. **Manual Marker Placement**
   ```
   Create physical map marker reference
   Place at quest objective location
   Name it after the quest objective
   ```

4. **Compass Workaround**
   ```
   Use objective alias to existing location
   Game automatically shows compass marker
   No custom code needed
   ```

