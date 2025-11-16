# Phase 6: INI Settings - Product Requirements Document

## Overview
Replace hardcoded values with INI settings. No new features, no logic changes - only substitute existing hardcoded constants with configurable parameters.

## Requirements

### Scope: ONLY Existing Hardcoded Values

**TextField (JournalNoteHelper::OnJournalOpen):**
- Position X: Currently `5.0`
- Position Y: Currently `5.0`
- Font Size: Currently `20`
- Text Color: Currently `0xFFFFFF`

**TextInput Dialog (PersonalNotes.psc):**
- Width: Currently `500`
- Height: Currently `400`
- Font Size: Currently `14`
- Alignment: Currently `0` (left)

**Hotkey (InputHandler::ProcessEvent):**
- Scan Code: Currently `51` (comma key)

**HUD Positions (QuestNoteHUDMenu::InitializeTextFields):**
- Tutorial X: Currently `0.0`
- Tutorial Y: Currently `0.0`
- Indicator X: Currently `0.0`
- Indicator Y: Currently `30.0`

**HUD Delay (QuestNoteHUDMenu::UpdateQuestTracking):**
- Indicator Delay: Currently `500` milliseconds

## Design

### 1. INI File Structure

```ini
[TextField]
fPositionX=5.0
fPositionY=5.0
iFontSize=20
iTextColor=16777215

[TextInput]
iWidth=500
iHeight=400
iFontSize=14
iAlignment=0

[Hotkey]
iScanCode=51

[HUD]
fTutorialX=0.0
fTutorialY=0.0
fIndicatorX=0.0
fIndicatorY=30.0
iIndicatorDelayMS=500
```

### 2. SettingsManager Class

```cpp
class SettingsManager {
public:
    static SettingsManager* GetSingleton();
    void LoadSettings();

    // TextField
    float textFieldX = 5.0f;
    float textFieldY = 5.0f;
    int textFieldFontSize = 20;
    int textFieldColor = 0xFFFFFF;

    // TextInput
    int textInputWidth = 500;
    int textInputHeight = 400;
    int textInputFontSize = 14;
    int textInputAlignment = 0;

    // Hotkey
    int noteHotkeyScanCode = 51;

    // HUD
    float hudTutorialX = 0.0f;
    float hudTutorialY = 0.0f;
    float hudIndicatorX = 0.0f;
    float hudIndicatorY = 30.0f;
    int hudIndicatorDelayMS = 500;

private:
    SettingsManager() = default;
};
```

### 3. Code Changes Required

**plugin.cpp:**
1. Add SettingsManager class
2. Load settings in InitializePlugin()
3. Replace hardcoded values:
   - JournalNoteHelper::OnJournalOpen() - TextField position, font, color
   - InputHandler::ProcessEvent() - Hotkey scan code
   - QuestNoteHUDMenu::InitializeTextFields() - HUD positions
   - QuestNoteHUDMenu::UpdateQuestTracking() - Delay
4. Pass TextInput settings to Papyrus functions

**PersonalNotes.psc:**
- Add parameters to ShowQuestNoteInput() for width, height, fontSize, alignment
- Add parameters to ShowGeneralNoteInput() for width, height, fontSize, alignment
- Receive settings from C++ when called

## Tasks

### Task 6.1: Implement SettingsManager
**Steps**:
1. Add SettingsManager singleton class
2. Add member variables with default values
3. Implement LoadSettings() using SKSE INI functions
4. Call LoadSettings() in InitializePlugin()
5. Create default INI file if missing

**Files Modified**: `plugin.cpp`

### Task 6.2: Replace TextField Hardcoded Values
**Steps**:
1. Replace `5.0` → `SettingsManager::textFieldX`
2. Replace `5.0` → `SettingsManager::textFieldY`
3. Replace `20` → `SettingsManager::textFieldFontSize`
4. Replace `0xFFFFFF` → `SettingsManager::textFieldColor`

**Files Modified**: `plugin.cpp` (JournalNoteHelper::OnJournalOpen)

### Task 6.3: Replace Hotkey Hardcoded Value
**Steps**:
1. Replace `51` → `SettingsManager::noteHotkeyScanCode`

**Files Modified**: `plugin.cpp` (InputHandler::ProcessEvent)

### Task 6.4: Replace HUD Hardcoded Values
**Steps**:
1. Replace tutorial position `0.0, 0.0` → `SettingsManager::hudTutorialX/Y`
2. Replace indicator position `0.0, 30.0` → `SettingsManager::hudIndicatorX/Y`
3. Replace delay `500` → `SettingsManager::hudIndicatorDelayMS`

**Files Modified**: `plugin.cpp` (QuestNoteHUDMenu)

### Task 6.5: Replace TextInput Hardcoded Values
**Steps**:
1. Update C++ Papyrus bridge to pass settings to Papyrus
2. Update PersonalNotes.psc to accept width, height, fontSize, alignment parameters
3. Replace hardcoded values with parameters from C++

**Files Modified**:
- `plugin.cpp` (PapyrusBridge)
- `PersonalNotes.psc`
- `mod/Source/Scripts/PersonalNotes.psc`

## Testing Plan

1. Run with no INI → Creates INI with defaults, behavior unchanged
2. Modify each INI value → Verify change takes effect
3. Test invalid values → Falls back to defaults
4. Test all features still work identically

## Success Criteria
- All hardcoded values replaced with INI settings
- Default behavior identical to before
- INI file auto-created with current defaults
- No logic changes, no new features
