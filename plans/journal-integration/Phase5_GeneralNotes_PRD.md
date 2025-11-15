# Phase 5: General Notes - Product Requirements Document

**Status**: Planning
**Created**: 2025-11-15
**Phase**: 5 - General Notes (Scratchpad)

---

## Overview

Add a simple general notes feature accessible during gameplay. When the comma key is pressed outside of the Journal Menu, a text input dialog opens showing the user's general notes (scratchpad). The note persists across sessions and is always editable.

**Key Principle**: Maximum simplicity - single persistent note, no UI indicators, reuse existing infrastructure.

---

## Requirements

### Functional Requirements

**FR5.1: General Note Input**
- Comma key pressed during gameplay (NOT in Journal Menu) opens general note dialog
- Dialog title: "Personal Notes"
- Shows existing general note text for editing (if any)
- Empty text field if no general note exists yet
- User can edit and save

**FR5.2: Persistence**
- General note saved to SKSE cosave
- Survives game restart
- Single note per save file (not per character, per save)

**FR5.3: No Quest Association**
- General note is NOT tied to any quest
- Completely separate from quest notes
- Stored with special key "GENERAL" instead of quest FormID

**FR5.4: Hotkey Behavior**
- In Journal Menu: Comma = Quest note (existing behavior, unchanged)
- During gameplay: Comma = General note (new behavior)
- Clear separation of contexts

### Non-Functional Requirements

**NFR5.1: Reuse Existing Infrastructure**
- Use NoteManager for storage (add support for non-quest notes)
- Reuse Papyrus text input UI
- Minimal new code

**NFR5.2: No Visual Indicators**
- No HUD icon
- No notification when general note exists
- No TextField overlay outside Journal
- Silent and unobtrusive

**NFR5.3: Performance**
- No performance impact during gameplay
- Lightweight hotkey detection
- Fast dialog opening

---

## Design

### Architecture

```
[Gameplay - Comma Key Press]
         ↓
[InputHandler detects: NOT in Journal Menu]
         ↓
[Load general note from NoteManager("GENERAL")]
         ↓
[PapyrusBridge::ShowGeneralNoteInput()]
         ↓
[Papyrus UI shows text input with existing text]
         ↓
[User edits and saves]
         ↓
[Papyrus calls SaveGeneralNote()]
         ↓
[NoteManager saves to "GENERAL" key]
         ↓
[Done - no visual feedback needed]
```

### Storage Strategy

**Current quest notes storage:**
```cpp
std::unordered_map<RE::FormID, std::string> notes_;
```

**New approach for general notes:**
Use special FormID value for general note:
- Quest notes: `notes_[questFormID] = "note text"`
- General note: `notes_[0xFFFFFFFF] = "general note text"`

**Alternative (cleaner):**
Define constant:
```cpp
static constexpr RE::FormID GENERAL_NOTE_ID = 0xFFFFFFFF;
```

**Storage key in cosave:** `"GENERAL"` string instead of FormID

### UI Flow

**Opening general note:**
1. User presses comma during gameplay
2. Check NOT in Journal Menu
3. Load existing general note text (if any)
4. Call `PersonalNotes.ShowGeneralNoteInput("", existingText)`
   - First param: questName (empty for general notes)
   - Second param: existingText
5. Papyrus shows dialog with title "Personal Notes"

**Saving general note:**
1. User clicks Save in Papyrus dialog
2. Papyrus calls `PersonalNotesNative.SaveGeneralNote(noteText)`
3. C++ saves to NoteManager with special ID
4. No notification, no visual feedback

### Modified Components

**NoteManager:**
- Add `GetGeneralNote()` method
- Add `SaveGeneralNote(text)` method
- Use `GENERAL_NOTE_ID` constant for storage

**PapyrusBridge:**
- Add `ShowGeneralNoteInput()` function
- Add `SaveGeneralNote()` native function

**InputHandler:**
- Detect comma key outside Journal Menu
- Call `ShowGeneralNoteInput()`

**Papyrus:**
- Modify `ShowQuestNoteInput()` to handle empty questName (general notes)
- Or create separate `ShowGeneralNoteInput()` function

---

## Tasks

### Task 5.1: Add General Note Support to NoteManager

**Description**: Add methods to NoteManager for storing/retrieving general note.

**Implementation**:
```cpp
// In NoteManager class
static constexpr RE::FormID GENERAL_NOTE_ID = 0xFFFFFFFF;

std::string GetGeneralNote() {
    auto it = notes_.find(GENERAL_NOTE_ID);
    return it != notes_.end() ? it->second : "";
}

void SaveGeneralNote(const std::string& text) {
    if (text.empty()) {
        notes_.erase(GENERAL_NOTE_ID);
        spdlog::info("[NOTE] Deleted general note");
    } else {
        notes_[GENERAL_NOTE_ID] = text;
        spdlog::info("[NOTE] Saved general note ({} chars)", text.length());
    }
    SaveToFile();
}
```

**Files Modified**:
- `plugin.cpp` (NoteManager class)

**Acceptance Criteria**:
- [ ] `GetGeneralNote()` returns existing general note or empty string
- [ ] `SaveGeneralNote()` saves text to notes_ map
- [ ] Empty text deletes general note
- [ ] General note persists in cosave

---

### Task 5.2: Add Papyrus Bridge Functions

**Description**: Add C++ functions to show general note input and save result.

**Implementation**:
```cpp
namespace PapyrusBridge {
    void ShowGeneralNoteInput() {
        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            spdlog::error("[PAPYRUS] Failed to get VM");
            return;
        }

        // Get existing general note text
        std::string existingText = NoteManager::GetSingleton()->GetGeneralNote();

        // Call Papyrus to show text input dialog
        auto args = RE::MakeFunctionArguments(
            std::move(RE::BSFixedString("")),           // questName (empty for general)
            std::move(RE::BSFixedString(existingText))
        );

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

        vm->DispatchStaticCall("PersonalNotes", "ShowGeneralNoteInput", args, callback);
        spdlog::info("[PAPYRUS] General note input opened");
    }

    void SaveGeneralNote(RE::StaticFunctionTag*, RE::BSFixedString noteText) {
        std::string text(noteText.c_str());
        NoteManager::GetSingleton()->SaveGeneralNote(text);

        if (text.empty()) {
            spdlog::info("[NOTE] Deleted general note");
        } else {
            spdlog::info("[NOTE] Saved general note");
        }
    }

    bool Register(RE::BSScript::IVirtualMachine* vm) {
        vm->RegisterFunction("SaveQuestNote", "PersonalNotesNative", SaveQuestNote);
        vm->RegisterFunction("SaveGeneralNote", "PersonalNotesNative", SaveGeneralNote);
        spdlog::info("[PAPYRUS] Native functions registered");
        return true;
    }
}
```

**Files Modified**:
- `plugin.cpp` (PapyrusBridge namespace)

**Acceptance Criteria**:
- [ ] `ShowGeneralNoteInput()` calls Papyrus with empty quest name
- [ ] `SaveGeneralNote()` saves to NoteManager
- [ ] Function registered in Papyrus VM

---

### Task 5.3: Add Hotkey Detection for General Notes

**Description**: Detect comma key press during gameplay (outside Journal Menu).

**Implementation**:
```cpp
// In InputHandler::ProcessEvent()

// Check for comma key (scan code 51)
if (buttonEvent->IsDown() && buttonEvent->idCode == 51) {
    auto ui = RE::UI::GetSingleton();
    bool inJournal = ui && ui->IsMenuOpen("Journal Menu");

    if (inJournal) {
        // Existing behavior: Quest note
        OnQuestNoteHotkey();
    } else {
        // New behavior: General note
        PapyrusBridge::ShowGeneralNoteInput();
        spdlog::info("[INPUT] General note hotkey pressed");
    }
}
```

**Files Modified**:
- `plugin.cpp` (InputHandler class)

**Acceptance Criteria**:
- [ ] Comma in Journal Menu → Quest note (existing, unchanged)
- [ ] Comma during gameplay → General note (new)
- [ ] No conflict between contexts

---

### Task 5.4: Update Papyrus Scripts

**Description**: Modify or create Papyrus function to handle general notes.

**Option A: Modify existing function**
```papyrus
; PersonalNotes.psc
Function ShowGeneralNoteInput(string questName, string existingText) Global
    if questName == ""
        ; General note - use different title
        UITextEntryMenu menu = UIExtensions.GetMenu("UITextEntryMenu") as UITextEntryMenu
        menu.SetPropertyString("text", existingText)
        menu.OpenMenu("Personal Notes")  ; Title for general notes
    else
        ; Quest note - existing logic
        UITextEntryMenu menu = UIExtensions.GetMenu("UITextEntryMenu") as UITextEntryMenu
        menu.SetPropertyString("text", existingText)
        menu.OpenMenu(questName + " - Quest Notes")
    endif
EndFunction
```

**Option B: Create separate function**
```papyrus
; PersonalNotes.psc
Function ShowGeneralNoteInput(string existingText) Global
    UITextEntryMenu menu = UIExtensions.GetMenu("UITextEntryMenu") as UITextEntryMenu
    menu.SetPropertyString("text", existingText)
    menu.OpenMenu("Personal Notes")

    string result = menu.GetResultString()
    if result != ""
        PersonalNotesNative.SaveGeneralNote(result)
    endif
EndFunction
```

**Files Modified**:
- `mod/Source/Scripts/PersonalNotes.psc`
- `mod/Source/Scripts/PersonalNotesNative.psc` (add native function declaration)

**Acceptance Criteria**:
- [ ] Dialog shows "Personal Notes" as title
- [ ] Existing general note text pre-filled
- [ ] Save calls `SaveGeneralNote()` native function

---

### Task 5.5: Testing

**Description**: Comprehensive testing of general notes feature.

**Test Cases**:

1. **Basic Functionality**
   - Press comma during gameplay → Dialog opens with "Personal Notes" title
   - Enter text, save → General note saved
   - Press comma again → Previous text shows for editing
   - Clear text, save → General note deleted

2. **Context Separation**
   - In Journal Menu, comma → Quest note dialog (unchanged)
   - During gameplay, comma → General note dialog
   - No cross-contamination between contexts

3. **Persistence**
   - Save general note
   - Save game and quit
   - Load game
   - Press comma → General note text restored

4. **Edge Cases**
   - Press comma with no existing general note → Empty dialog
   - Save empty general note → Note deleted
   - Very long text (test character limits)
   - Special characters in text

**Acceptance Criteria**:
- [ ] All test cases pass
- [ ] No conflicts with quest notes
- [ ] Persistent across sessions
- [ ] No crashes or data loss

---

## Acceptance Criteria (Phase Complete)

### Functional
- [ ] Comma during gameplay opens general note dialog
- [ ] Dialog titled "Personal Notes"
- [ ] Existing general note text pre-filled
- [ ] Empty text deletes general note
- [ ] General note persists across sessions

### Technical
- [ ] Reuses existing NoteManager infrastructure
- [ ] Reuses existing Papyrus UI
- [ ] No new UI components needed
- [ ] Clean separation from quest notes

### User Experience
- [ ] Simple and unobtrusive
- [ ] No visual indicators needed
- [ ] Instant access via comma key
- [ ] Familiar UI (same as quest notes)

---

## Known Risks

1. **Hotkey Conflict**: Comma key might be used by other mods
   - Mitigation: Clear logging, easy to change key if needed

2. **UI Menu Availability**: UIExtensions must be installed
   - Mitigation: Same requirement as quest notes, already documented

3. **Storage Key Collision**: Using FormID 0xFFFFFFFF might conflict
   - Mitigation: Very unlikely, FormIDs this high are invalid for game objects

---

## Future Enhancements (Out of Scope)

- Multiple general notes with categories
- Search functionality
- Export to text file
- Timestamps on notes
- Note templates

---

## References

**Related Files**:
- `/mnt/c/dev/Skyrim-Modding/Skyrim-PersonalNotes/plugin.cpp`
- `/mnt/c/dev/Skyrim-Modding/Skyrim-PersonalNotes/mod/Source/Scripts/PersonalNotes.psc`
- `/mnt/c/dev/Skyrim-Modding/Skyrim-PersonalNotes/mod/Source/Scripts/PersonalNotesNative.psc`

**Dependencies**:
- Existing NoteManager (Phase 2)
- Existing Papyrus bridge (Phase 3)
- UIExtensions

**Related Phases**:
- Phase 2: Note Storage
- Phase 3: Journal Integration (quest notes)
- Phase 4B: HUD TextField (quest notes only)

---

**Document Version**: 1.0
**Last Updated**: 2025-11-15
