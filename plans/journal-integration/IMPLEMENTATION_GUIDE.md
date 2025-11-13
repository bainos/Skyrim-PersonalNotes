# Implementation Guide: Detect Current Quest in Journal Menu

## Quick Start: Getting the Current Quest

### Most Reliable Method: Via UI Singleton + Scaleform

```cpp
#include "RE/J/JournalMenu.h"
#include "RE/G/GFxValue.h"
#include "RE/U/UI.h"
#include "RE/T/TESForm.h"

TESQuest* GetCurrentQuestViewed()
{
    // Step 1: Get UI singleton
    auto ui = RE::UI::GetSingleton();
    if (!ui) return nullptr;
    
    // Step 2: Get JournalMenu
    auto journalMenu = ui->GetMenu<RE::JournalMenu>();
    if (!journalMenu) return nullptr;
    
    // Step 3: Check if we have the movie view
    auto movieView = journalMenu->uiMovie;
    if (!movieView) return nullptr;
    
    // Step 4: Query current quest (requires SWF analysis to get exact path)
    RE::GFxValue questData;
    if (!movieView->GetVariable(&questData, "_level0.JournalMenu.data.selectedQuest"))
    {
        return nullptr;
    }
    
    // Step 5: Get form ID from quest data
    if (!questData.IsObject()) return nullptr;
    
    RE::GFxValue formID;
    questData.GetMember("formID", &formID);
    
    if (!formID.IsNumber()) return nullptr;
    
    FormID questID = static_cast<FormID>(formID.GetNumber());
    
    // Step 6: Look up the quest in game data
    return RE::TESForm::LookupByID<RE::TESQuest>(questID);
}
```

---

## Method 1: Direct Structure Access

### When JournalMenu is Accessed

```cpp
#include "RE/J/JournalMenu.h"

void OnHotkeyPressed()
{
    auto ui = RE::UI::GetSingleton();
    auto journalMenu = ui->GetMenu<RE::JournalMenu>();
    
    if (!journalMenu) {
        logger::info("Journal Menu not open");
        return;
    }
    
    // Access runtime data
    auto& rtData = journalMenu->GetRuntimeData();
    
    // Access quest tab
    auto& questTab = rtData.questsTab;
    
    logger::info("Quest Tab accessed: 0x{:X}", (uintptr_t)&questTab);
    
    // The unk18 member is a GFxValue - this is the UI object
    // To get quest data, we need to query through Scaleform API
}
```

---

## Method 2: Scaleform Variable Access (Recommended)

### Query Quest List

```cpp
#include "RE/G/GFxValue.h"
#include "RE/U/UI.h"

std::vector<FormID> GetAllQuestsInList()
{
    std::vector<FormID> quests;
    
    auto ui = RE::UI::GetSingleton();
    auto movieView = ui->GetMovieView("Journal Menu");
    if (!movieView) return quests;
    
    // Get quest array size (example path)
    uint32_t questCount = movieView->GetVariableArraySize("_level0.JournalMenu.questArray");
    
    logger::info("Found {} quests in journal", questCount);
    
    // Get each quest's form ID
    for (uint32_t i = 0; i < questCount; ++i)
    {
        RE::GFxValue questObj;
        if (movieView->GetElement(i, &questObj))
        {
            RE::GFxValue formIDVal;
            if (questObj.GetMember("formID", &formIDVal))
            {
                quests.push_back((FormID)formIDVal.GetNumber());
            }
        }
    }
    
    return quests;
}
```

### Get Selected Quest Index

```cpp
#include "RE/G/GFxValue.h"
#include "RE/U/UI.h"

int GetSelectedQuestIndex()
{
    auto ui = RE::UI::GetSingleton();
    auto movieView = ui->GetMovieView("Journal Menu");
    if (!movieView) return -1;
    
    RE::GFxValue selectedIndex;
    if (movieView->GetVariable(&selectedIndex, 
        "_level0.JournalMenu.selectedIndex"))
    {
        return static_cast<int>(selectedIndex.GetNumber());
    }
    
    return -1;
}
```

---

## Method 3: Using Journal_QuestsTab

### Access Scaleform Object in Tab

```cpp
#include "RE/J/Journal_QuestsTab.h"
#include "RE/G/GFxValue.h"

void InspectQuestTabUI()
{
    auto ui = RE::UI::GetSingleton();
    auto journalMenu = ui->GetMenu<RE::JournalMenu>();
    if (!journalMenu) return;
    
    auto& rtData = journalMenu->GetRuntimeData();
    auto& questTab = rtData.questsTab;
    
    // questTab has a GFxValue at offset 0x18 (unk18)
    // This is likely the root UI object for the quest list
    
    // Can call methods on it:
    const RE::GFxValue& uiObj = questTab.unk18;
    
    RE::GFxValue result;
    uiObj.Invoke("GetSelectedQuest", &result);
    
    if (result.IsObject())
    {
        RE::GFxValue questID;
        result.GetMember("formID", &questID);
        
        logger::info("Selected quest ID: 0x{:X}", 
                    (FormID)questID.GetNumber());
    }
}
```

---

## Complete Example: Plugin Hook

### SKSE Plugin Entry Point

```cpp
#include "RE/J/JournalMenu.h"
#include "RE/U/UI.h"
#include "RE/T/TESForm.h"

class QuestDetector
{
public:
    static void OnHotkey(uint32_t keyCode)
    {
        if (auto quest = GetCurrentQuestInJournal())
        {
            logger::info("Current quest: {} (0x{:X})", 
                        quest->GetName(), quest->GetFormID());
        }
        else
        {
            logger::info("No quest currently selected in Journal");
        }
    }
    
    static RE::TESQuest* GetCurrentQuestInJournal()
    {
        // Get UI singleton
        auto ui = RE::UI::GetSingleton();
        if (!ui) return nullptr;
        
        // Check if journal is open
        if (!ui->IsMenuOpen("Journal Menu")) return nullptr;
        
        // Get journal menu
        auto journalMenu = ui->GetMenu<RE::JournalMenu>();
        if (!journalMenu) return nullptr;
        
        // Get the movie view
        auto movieView = journalMenu->uiMovie;
        if (!movieView) return nullptr;
        
        // Try to get selected quest index first
        int selectedIndex = GetSelectedQuestIndex(movieView.get());
        if (selectedIndex >= 0)
        {
            auto quest = GetQuestAtIndex(movieView.get(), selectedIndex);
            if (quest) return quest;
        }
        
        // Fallback: Try direct variable access
        return GetQuestFromVariable(movieView.get());
    }
    
private:
    static int GetSelectedQuestIndex(RE::GFxMovieView* movieView)
    {
        RE::GFxValue selectedIndex;
        if (movieView->GetVariable(&selectedIndex, 
            "_level0.JournalMenu.selectedIndex"))
        {
            return static_cast<int>(selectedIndex.GetNumber());
        }
        return -1;
    }
    
    static RE::TESQuest* GetQuestAtIndex(RE::GFxMovieView* movieView, 
                                         int index)
    {
        RE::GFxValue questArray;
        if (!movieView->GetVariable(&questArray, 
            "_level0.JournalMenu.questArray"))
        {
            return nullptr;
        }
        
        if (!questArray.IsArray()) return nullptr;
        
        RE::GFxValue questObj;
        if (!questArray.GetElement(index, &questObj)) return nullptr;
        
        if (!questObj.IsObject()) return nullptr;
        
        RE::GFxValue formIDVal;
        questObj.GetMember("formID", &formIDVal);
        
        if (!formIDVal.IsNumber()) return nullptr;
        
        FormID questID = static_cast<FormID>(formIDVal.GetNumber());
        return RE::TESForm::LookupByID<RE::TESQuest>(questID);
    }
    
    static RE::TESQuest* GetQuestFromVariable(RE::GFxMovieView* movieView)
    {
        RE::GFxValue currentQuest;
        if (!movieView->GetVariable(&currentQuest, 
            "_level0.JournalMenu.currentQuest"))
        {
            return nullptr;
        }
        
        if (!currentQuest.IsObject()) return nullptr;
        
        RE::GFxValue formIDVal;
        currentQuest.GetMember("formID", &formIDVal);
        
        if (!formIDVal.IsNumber()) return nullptr;
        
        FormID questID = static_cast<FormID>(formIDVal.GetNumber());
        return RE::TESForm::LookupByID<RE::TESQuest>(questID);
    }
};
```

---

## GFxValue API Reference

### Type Checking Methods

```cpp
RE::GFxValue val;

// Type queries
val.IsUndefined();
val.IsNull();
val.IsBool();
val.IsNumber();
val.IsString();
val.IsObject();
val.IsArray();
val.IsDisplayObject();

// Get values
bool b = val.GetBool();
double n = val.GetNumber();
const char* s = val.GetString();
const wchar_t* w = val.GetStringW();
```

### Object Access

```cpp
RE::GFxValue obj;

// Check if member exists
bool has = obj.HasMember("memberName");

// Get member
RE::GFxValue member;
obj.GetMember("memberName", &member);

// Set member
RE::GFxValue newVal = 42;
obj.SetMember("memberName", newVal);

// Delete member
obj.DeleteMember("memberName");

// Iterate members
class MemberVisitor : public RE::GFxValue::ObjectVisitor
{
    void Visit(const char* a_name, const RE::GFxValue& a_val) override
    {
        logger::info("  {}: {}", a_name, a_val.ToString().c_str());
    }
};
MemberVisitor visitor;
obj.VisitMembers(&visitor);
```

### Array Access

```cpp
RE::GFxValue arr;

// Get array size
uint32_t size = arr.GetArraySize();

// Get element
RE::GFxValue elem;
arr.GetElement(0, &elem);

// Set element
arr.SetElement(0, RE::GFxValue(123));

// Push element
arr.PushBack(RE::GFxValue("new element"));

// Clear
arr.ClearElements();
```

### Method Invocation

```cpp
RE::GFxValue obj;

// Invoke with no args
RE::GFxValue result;
obj.Invoke("methodName", &result);

// Invoke with args
RE::GFxValue args[] = {
    RE::GFxValue(10),
    RE::GFxValue("test")
};
obj.Invoke("methodName", &result, args, 2);

// Use array wrapper
std::array<RE::GFxValue, 2> argsArray = {
    RE::GFxValue(10),
    RE::GFxValue("test")
};
obj.Invoke("methodName", &result, argsArray);
```

---

## Detecting Paths via Reverse Engineering

### SWF Decompilation Checklist

1. Extract Journal menu SWF from Skyrim/Data/Interface/
2. Use JPEXS Free Flash Decompiler to decompile
3. Look for ActionScript classes related to:
   - QuestList
   - QuestEntry  
   - Journal display

4. Search for key variables:
   - `selectedIndex`
   - `currentQuest`
   - `questArray`
   - `questList`

5. Map variable paths to Scaleform paths

### Example Path Discovery

If ActionScript code shows:
```actionscript
this._questList.selectedIndex = 0;
this._questList._questArray[0]
```

The Scaleform path would be:
```
_level0.JournalMenu._questList.selectedIndex
_level0.JournalMenu._questList._questArray
```

---

## File References

| File | Key Classes | Usage |
|------|------------|-------|
| JournalMenu.h | JournalMenu, Journal_QuestsTab | Menu access, quest tab |
| UI.h | UI singleton | Get menu instances |
| GFxMovie.h | GFxMovie, GFxMovieView | Scaleform access, variables |
| GFxValue.h | GFxValue | Object/value manipulation |
| TESForm.h | TESForm, TESQuest | Quest data lookup |
| TESQuest.h | TESQuest | Quest methods (GetName, GetFormID) |

---

## Troubleshooting

### Issue: movieView is null
```cpp
// Check if menu is open first
if (!ui->IsMenuOpen("Journal Menu")) {
    logger::warn("Journal Menu not open");
    return nullptr;
}

// Verify menu has uiMovie
if (!journalMenu->uiMovie) {
    logger::warn("Journal Menu has no movie view");
    return nullptr;
}
```

### Issue: GetVariable returns false
```cpp
// Try alternative paths:
const char* paths[] = {
    "_level0.JournalMenu.selectedIndex",
    "_level0.JournalMenu.selectedQuestIndex",
    "_level0.JournalMenu.currentQuestIndex",
    "_root.JournalMenu.selectedIndex",
};

for (const auto& path : paths) {
    if (movieView->GetVariable(&val, path)) {
        logger::info("Found at path: {}", path);
        break;
    }
}
```

### Issue: Quest index doesn't match active quest
```cpp
// Index might be different from FormID lookup
// Maintain mapping of indices to FormIDs:
std::map<int, FormID> indexToFormID;

// Or use quest names for matching
const char* questName = quest->GetName();
logger::info("Selected quest: {}", questName);
```

---

## Performance Considerations

- **GetVariable calls**: Relatively fast, safe to call each frame
- **GFxValue member access**: No performance penalty
- **TESForm lookup**: O(1) hash table lookup via FormID
- **Caching**: Consider caching quest list if accessed frequently

### Recommended Approach

```cpp
// Cache menu pointer
static RE::JournalMenu* cachedMenu = nullptr;

void UpdateCachedMenu()
{
    auto ui = RE::UI::GetSingleton();
    cachedMenu = ui->GetMenu<RE::JournalMenu>().get();
}

void OnEachFrame()
{
    if (!cachedMenu && ui->IsMenuOpen("Journal Menu"))
        UpdateCachedMenu();
    
    if (cachedMenu)
    {
        // Query current quest
    }
}
```

