# Phase 7: MCM Menu - Product Requirements Document

## Overview
Create MCM menu to expose the INI settings from Phase 6. Provides in-game GUI for the same configurable parameters - no additional features.

## Prerequisites
- **Phase 6 (INI Settings)** must be completed
- **Dependency**: SkyUI 5.x+ (MCM framework)

## Requirements

### Scope: ONLY Phase 6 Settings

MCM will expose the exact same settings as Phase 6 INI:

**TextField Settings:**
- Position X, Position Y
- Font Size
- Text Color

**TextInput Dialog Settings:**
- Width, Height
- Font Size
- Alignment

**Hotkey Setting:**
- Scan Code

**HUD Settings:**
- Tutorial Position (X, Y)
- Indicator Position (X, Y)
- Indicator Delay

### Menu Structure

**3 Pages**:
1. **TextField** - Journal TextField appearance
2. **Text Input** - Dialog customization
3. **Hotkey & HUD** - Hotkey and HUD positions/delay

## Design

### 1. MCM Script Structure

**PersonalNotesMCM.psc**:
```papyrus
Scriptname PersonalNotesMCM extends SKI_ConfigBase

event OnPageReset(string page)
    if page == "TextField"
        BuildTextFieldPage()
    elseif page == "Text Input"
        BuildTextInputPage()
    elseif page == "Hotkey & HUD"
        BuildHotkeyHUDPage()
    endif
endEvent
```

### 2. Page Details

**TextField Page**:
- Slider: Position X (0-1920)
- Slider: Position Y (0-1080)
- Slider: Font Size (8-72)
- Input: Text Color (hex)
- Button: Reset to Defaults

**Text Input Page**:
- Slider: Width (200-1920)
- Slider: Height (100-1080)
- Slider: Font Size (8-72)
- Menu: Alignment (Left/Center/Right)
- Button: Reset to Defaults

**Hotkey & HUD Page**:
- Keymap: Note Hotkey
- Slider: Tutorial X (-500 to 500)
- Slider: Tutorial Y (-500 to 500)
- Slider: Indicator X (-500 to 500)
- Slider: Indicator Y (-500 to 500)
- Slider: Indicator Delay (0-5000 ms)
- Button: Reset to Defaults

### 3. INI Sync

- **On MCM Open**: Read current values from INI
- **On Change**: Write immediately to INI
- **Reset Button**: Restore Phase 6 defaults

## Tasks

### Task 7.1: Create MCM Foundation
**Steps**:
1. Create PersonalNotesMCM.psc extending SKI_ConfigBase
2. Implement 3-page structure
3. Add INI read/write helper functions
4. Register with MCM system

**Files Created**: `PersonalNotesMCM.psc`

### Task 7.2: Build TextField Page
**Steps**:
1. Add sliders for position X/Y, font size
2. Add color input field
3. Add reset button
4. Wire up to INI settings

**Files Modified**: `PersonalNotesMCM.psc`

### Task 7.3: Build Text Input Page
**Steps**:
1. Add sliders for width, height, font size
2. Add alignment dropdown menu
3. Add reset button
4. Wire up to INI settings

**Files Modified**: `PersonalNotesMCM.psc`

### Task 7.4: Build Hotkey & HUD Page
**Steps**:
1. Add keymap for hotkey scan code
2. Add sliders for HUD positions (tutorial and indicator X/Y)
3. Add slider for indicator delay
4. Add reset button
5. Wire up to INI settings

**Files Modified**: `PersonalNotesMCM.psc`

### Task 7.5: Testing and Polish
**Steps**:
1. Test all settings save to INI
2. Test reset buttons
3. Test that changes take effect after restart
4. Add tooltips for all controls

**Files Modified**: `PersonalNotesMCM.psc`

## Testing Plan

1. All pages load without errors
2. Settings read from INI correctly
3. Changes save to INI immediately
4. Reset buttons restore defaults
5. Settings match Phase 6 INI exactly

## Success Criteria
- MCM exposes all Phase 6 settings
- No additional features beyond Phase 6
- Clean, simple UI
- Settings sync perfectly with INI file
