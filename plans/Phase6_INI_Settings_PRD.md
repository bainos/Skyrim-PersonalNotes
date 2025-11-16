# Phase 6: INI Settings - Product Requirements Document

## Overview
Replace hardcoded values with INI settings. No new features, no logic changes - only substitute existing hardcoded constants with configurable parameters.

## Requirements

### Scope: ONLY Existing Hardcoded Values

**TextField (JournalNoteHelper::OnJournalOpen):**
- Position X: Currently `5.0` (line 325)
- Position Y: Currently `5.0` (line 326)
- Font Size: Currently `20` (line 339)
- Text Color: Currently `0xFFFFFF` (line 340)

**TextInput Dialog (PersonalNotes.psc):**
- Width: Currently `500`
- Height: Currently `400`
- Font Size: Currently `14`
- Alignment: Currently `0` (left)

**Hotkey (InputHandler::ProcessEvent):**
- Scan Code: Currently `51` (comma key) (line 489)

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
4. Pass TextInput settings to Papyrus functions

**PersonalNotes.psc:**
- Modify ShowQuestNoteInput() to receive width, height, fontSize, alignment from C++
- Modify ShowGeneralNoteInput() to receive width, height, fontSize, alignment from C++

## Tasks

### Task 6.1: Implement SettingsManager
**Steps**:
1. Add SettingsManager singleton class to plugin.cpp
2. Add member variables with default values matching current hardcoded values
3. Implement LoadSettings() using SKSE INI functions
4. Call LoadSettings() in InitializePlugin()
5. Create default INI file if missing

**Files Modified**: `plugin.cpp`

### Task 6.2: Replace TextField Hardcoded Values
**Steps**:
1. In JournalNoteHelper::OnJournalOpen():
   - Replace `5` (X position) → `SettingsManager::textFieldX`
   - Replace `5` (Y position) → `SettingsManager::textFieldY`
   - Replace `20` (font size) → `SettingsManager::textFieldFontSize`
   - Replace `0xFFFFFF` (color) → `SettingsManager::textFieldColor`

**Files Modified**: `plugin.cpp` (JournalNoteHelper::OnJournalOpen)

### Task 6.3: Replace Hotkey Hardcoded Value
**Steps**:
1. In InputHandler::ProcessEvent():
   - Replace `51` → `SettingsManager::noteHotkeyScanCode`

**Files Modified**: `plugin.cpp` (InputHandler::ProcessEvent)

### Task 6.4: Replace TextInput Hardcoded Values
**Steps**:
1. Update PapyrusBridge::ShowQuestNoteInput() to pass TextInput settings to Papyrus
2. Update PapyrusBridge::ShowGeneralNoteInput() to pass TextInput settings to Papyrus
3. Update PersonalNotes.psc to receive and use these parameters
4. Update mod/Source/Scripts/PersonalNotes.psc (dist copy)

**Files Modified**:
- `plugin.cpp` (PapyrusBridge::ShowQuestNoteInput, ShowGeneralNoteInput)
- `PersonalNotes.psc`
- `mod/Source/Scripts/PersonalNotes.psc`

## Implementation Details

### SettingsManager LoadSettings() Implementation

Use SKSE's INI reading functions:
```cpp
void SettingsManager::LoadSettings() {
    constexpr auto path = L"Data/SKSE/Plugins/PersonalNotes.ini";

    // TextField
    textFieldX = GetPrivateProfileFloat("TextField", "fPositionX", 5.0f, path);
    textFieldY = GetPrivateProfileFloat("TextField", "fPositionY", 5.0f, path);
    textFieldFontSize = GetPrivateProfileInt("TextField", "iFontSize", 20, path);
    textFieldColor = GetPrivateProfileInt("TextField", "iTextColor", 0xFFFFFF, path);

    // TextInput
    textInputWidth = GetPrivateProfileInt("TextInput", "iWidth", 500, path);
    textInputHeight = GetPrivateProfileInt("TextInput", "iHeight", 400, path);
    textInputFontSize = GetPrivateProfileInt("TextInput", "iFontSize", 14, path);
    textInputAlignment = GetPrivateProfileInt("TextInput", "iAlignment", 0, path);

    // Hotkey
    noteHotkeyScanCode = GetPrivateProfileInt("Hotkey", "iScanCode", 51, path);

    spdlog::info("[SETTINGS] Loaded from INI");
}
```

### Papyrus Bridge Changes

**ShowQuestNoteInput** - pass TextInput settings:
```cpp
auto settings = SettingsManager::GetSingleton();
auto args = RE::MakeFunctionArguments(
    static_cast<std::int32_t>(questID),
    RE::BSFixedString(questName),
    RE::BSFixedString(existingText),
    settings->textInputWidth,
    settings->textInputHeight,
    settings->textInputFontSize,
    settings->textInputAlignment
);
```

**PersonalNotes.psc** - receive settings:
```papyrus
Function ShowQuestNoteInput(int questID, string questName, string existingText, int width, int height, int fontSize, int alignment) Global
    String prompt = "Note for: " + questName
    String result = ExtendedVanillaMenus.TextInput(prompt, existingText, Width = width, Height = height, FontSize = fontSize, align = alignment)
    ; ... rest of function
EndFunction
```

## Testing Plan

1. **No INI Test**: Delete INI, run game → INI created with defaults, behavior unchanged
2. **TextField Test**: Modify TextField settings → Verify position, size, color changes
3. **TextInput Test**: Modify TextInput settings → Verify dialog size, font changes
4. **Hotkey Test**: Change scan code → Verify new key works
5. **Invalid Values Test**: Set negative/extreme values → Falls back to defaults
6. **All Features Test**: Verify quest notes, general notes, mouse hover all work

## Success Criteria
- All hardcoded values replaced with INI settings
- Default behavior identical to before (users won't notice without changing INI)
- INI file auto-created with current defaults if missing
- No logic changes, no new features
- No crashes with invalid INI values
