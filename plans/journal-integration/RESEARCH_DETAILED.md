# CommonLibSSE-NG Research: Detecting Current Quest in Journal Menu

## Summary
Found multiple viable approaches to detect which quest is being viewed in the Journal Menu when a hotkey is pressed. The APIs provide access through both the UI system and Scaleform variables.

---

## KEY FINDINGS

### 1. JournalMenu Class Structure
**File**: `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/J/JournalMenu.h`
**Lines**: 18-97

#### Class Definition
```cpp
class JournalMenu : public IMenu, public MenuEventHandler, public BSTEventSink<BSSystemEvent>
{
    struct RUNTIME_DATA
    {
        Journal_QuestsTab questsTab;  // 00 - Main quest data structure
        Journal_StatsTab  statsTab;   // 38
        Journal_SystemTab systemTab;  // 50
        std::uint64_t     unkD0;      // 88
        std::uint64_t     unkD8;      // 90
        std::uint64_t     unkE0;      // 98
    };
    
    // Access via:
    RUNTIME_DATA& GetRuntimeData() noexcept
    {
        return REL::RelocateMember<RUNTIME_DATA>(this, 0x48, 0x58);
    }
};
```

**Key Members**:
- `uiMovie` (from IMenu): GPtr<GFxMovieView> - Scaleform UI view
- `questsTab`: Journal_QuestsTab - Contains quest list and current selection
- `menuFlags`: Enum showing kPausesGame, kUsesMenuContext flags

**Feasibility**: HIGH - Direct access to quest tab structure

---

### 2. Journal_QuestsTab Structure
**File**: `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/J/Journal_QuestsTab.h`
**Lines**: 8-26

```cpp
class Journal_QuestsTab : public JournalTab
{
    GFxValue      unk18;      // 18 - Likely Scaleform UI object
    bool          unk30;      // 30
    std::uint8_t  unk31;      // 31
    std::uint16_t unk32;      // 32
    std::uint32_t unk34;      // 34
};
```

**Key Member**: `unk18` (GFxValue) - Scaleform object that likely holds UI state
**Feasibility**: MEDIUM - GFxValue can be queried for member variables

---

### 3. UI Access Pattern via Singleton
**File**: `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/U/UI.h`
**Lines**: 54-144

#### Get JournalMenu Instance
```cpp
// Get UI singleton
UI* ui = UI::GetSingleton();

// Method 1: Get menu by type (type-safe)
GPtr<JournalMenu> journalMenu = ui->GetMenu<JournalMenu>();

// Method 2: Get menu by name
GPtr<IMenu> journalMenu = ui->GetMenu("Journal Menu");

// Check if menu is open
if (ui->IsMenuOpen("Journal Menu"))
{
    // Menu is currently visible
}
```

**Feasibility**: VERY HIGH - Standard pattern used throughout CommonLibSSE-NG

---

## APPROACH #1: Direct C++ Access (Most Reliable)

### Implementation Path
```cpp
UI* ui = UI::GetSingleton();
if (!ui) return nullptr;

GPtr<JournalMenu> journalMenu = ui->GetMenu<JournalMenu>();
if (!journalMenu) return nullptr;

// Access the quest tab
RUNTIME_DATA& rtData = journalMenu->GetRuntimeData();
Journal_QuestsTab& questTab = rtData.questsTab;

// questTab.unk18 is a GFxValue (Scaleform UI object)
// Can query it for selected quest index or form ID
```

**Status**: Feasible
**Challenge**: Unknown struct members in `Journal_QuestsTab`
**Workaround**: Reverse engineer unknown members or use Scaleform API

---

## APPROACH #2: Scaleform UI Variable Access (Most Comprehensive)

### Key Classes for Scaleform Access
**File**: `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/G/GFxMovie.h`
**Lines**: 11-77

#### Available Methods
```cpp
class GFxMovie
{
    // Get variable by ActionScript path
    bool GetVariable(GFxValue* a_val, const char* a_pathToVar) const;
    
    // Get array size
    std::uint32_t GetVariableArraySize(const char* a_pathToVar);
    
    // Get array element
    bool GetVariableArray(SetArrayType a_type, const char* a_pathToVar, 
                         std::uint32_t a_index, void* a_data, 
                         std::uint32_t a_count);
    
    // Get variable as double (useful for IDs/indices)
    double GetVariableDouble(const char* a_pathToVar) const;
    
    // Invoke ActionScript method
    bool Invoke(const char* a_methodName, GFxValue* a_result, 
               const GFxValue* a_args, std::uint32_t a_numArgs);
};
```

### Implementation Path
```cpp
UI* ui = UI::GetSingleton();
GPtr<GFxMovieView> movieView = ui->GetMovieView("Journal Menu");
if (!movieView) return nullptr;

// Example: Query quest list (exact paths need verification from SWF analysis)
// Pattern from other menus: "_level0.Menu_mc.{component}.{list}"
GFxValue questList;
if (movieView->GetVariable(&questList, "_level0.JournalMenu.questList"))
{
    // questList is now a GFxValue of array or object type
    // Can iterate through quest entries
}

// Get current selection
GFxValue selectedIndex;
if (movieView->GetVariable(&selectedIndex, "_level0.JournalMenu.selectedQuestIndex"))
{
    int index = (int)selectedIndex.GetNumber();
}
```

**Status**: Feasible
**Challenge**: Requires knowing exact Scaleform variable paths
**Solution**: Requires SWF decompilation or reverse engineering

---

## APPROACH #3: ItemList Pattern (Template from Inventory)

### Reference Implementation
**File**: `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/I/ItemList.h`
**Lines**: 13-44

This shows how Inventory menu accesses selected items:
```cpp
struct ItemList
{
    GPtr<GFxMovieView> view;       // Reference to movie
    GFxValue           root;       // UI display object path
    GFxValue           entryList;  // Array of items
    BSTArray<Item*>    items;      // C++ item array
    
    // Access selected item
    Item* GetSelectedItem();
};
```

### Applied to Quests
Could create similar structure for quest access:
```cpp
struct QuestListHelper
{
    GPtr<GFxMovieView> view;
    GFxValue root;          // "_level0.JournalMenu..."
    GFxValue questArray;    // Quest list array
    BSTArray<TESQuest*> quests;
    
    TESQuest* GetSelectedQuest();
};
```

**Status**: Proof of concept works
**Challenge**: Need to map Scaleform paths correctly

---

## APPROACH #4: Event-Based Detection (Alternative)

**File**: `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/T/TESQuestStageEvent.h`

Quest state changes trigger events:
```cpp
// Can register for quest stage/start/stop events
// When menu opens, query current selected quest separately
```

**Status**: Supplementary approach (use with other methods)

---

## TESQuest Class - Getting Quest Data
**File**: `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/T/TESQuest.h`
**Lines**: 186-284

Once you have a quest, access data:
```cpp
class TESQuest : public BGSStoryManagerTreeForm
{
    // Get quest status/state
    bool IsActive() const;
    bool IsCompleted() const;
    bool IsEnabled() const;
    std::uint16_t GetCurrentStageID() const;
    
    // Get form information
    FormID GetFormID() const { return formID; }
    const char* GetName() const;  // From TESFullName
    
    // Get objectives
    BSSimpleList<BGSQuestObjective*> objectives;
    
    // Get current stage
    std::uint16_t currentStage;
};
```

**Key Methods**:
- `GetFormID()` - Line 288: Returns quest's form ID
- `GetName()` - Line 303: Returns quest name
- `IsActive()` - Line 241: Checks if quest is running
- `GetCurrentStageID()` - Line 239: Current progress

---

## Recommended Implementation Strategy

### Step 1: Access the Menu
```cpp
auto ui = RE::UI::GetSingleton();
auto journalMenu = ui->GetMenu<RE::JournalMenu>();
if (!journalMenu) return nullptr;
```

### Step 2: Get Scaleform Movie View
```cpp
auto movieView = journalMenu->uiMovie;
if (!movieView) return nullptr;
```

### Step 3: Query Selected Quest Index
```cpp
RE::GFxValue selectedIndex;
// Need to determine correct path through reverse engineering
if (movieView->GetVariable(&selectedIndex, "...selectedQuestIndex"))
{
    int index = (int)selectedIndex.GetNumber();
}
```

### Step 4: Get Quest from Game Data
```cpp
// Use quest index to lookup actual quest object
// OR query quest form ID from Scaleform variable
RE::GFxValue questFormID;
if (movieView->GetVariable(&questFormID, "...currentQuestFormID"))
{
    FormID id = (FormID)questFormID.GetNumber();
    auto quest = RE::TESForm::LookupByID<RE::TESQuest>(id);
    if (quest)
    {
        // Success! Have the current quest
        auto name = quest->GetName();
    }
}
```

---

## FEASIBILITY RATINGS

| Method | Feasibility | Effort | Reliability | Notes |
|--------|-------------|--------|-------------|-------|
| Direct Struct Access | HIGH | MEDIUM | HIGH | Need unknown member mapping |
| Scaleform Variables | HIGH | HIGH | HIGH | Best once paths determined |
| TESQuest Lookup | VERY HIGH | LOW | VERY HIGH | Once you have quest ID |
| Event-Based | MEDIUM | LOW | MEDIUM | Supplementary only |

---

## CHALLENGES & MITIGATIONS

### Challenge 1: Unknown Scaleform Variable Paths
- **Status**: Can be reverse engineered
- **Tools**: SWF decompiler, debugger
- **Mitigation**: Start with known patterns from BarterMenu, InventoryMenu

### Challenge 2: Unknown Journal_QuestsTab Members
- **Status**: Some are marked `unk##`
- **Mitigation**: Use Scaleform API instead of direct struct access
- **Size**: struct is only 0x38 bytes, leaving room to map unknowns

### Challenge 3: VR vs SE vs AE Differences
- **Status**: Code uses REL::RelocateMember for offsets
- **Mitigation**: Use provided offset system, not hardcoded addresses

---

## NEXT STEPS

1. **Scaleform Path Discovery**
   - Decompile Journal menu SWF
   - Find quest list array name
   - Find selected quest index variable name
   - Find quest form ID access

2. **Testing**
   - Create test SKSE plugin
   - Hook hotkey press
   - Query current quest
   - Log results

3. **Fallback Implementation**
   - If live quest detection fails, poll quest state
   - Use TESQuestStageEvent for updates
   - Maintain quest data cache

---

## References

- `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/J/JournalMenu.h`
- `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/U/UI.h`
- `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/G/GFxMovie.h`
- `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/G/GFxValue.h`
- `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/T/TESQuest.h`
- `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/I/ItemList.h`

