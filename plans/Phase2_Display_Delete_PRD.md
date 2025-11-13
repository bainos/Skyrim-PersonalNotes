# Phase 2: Note Display and Management - PRD

**Duration:** 1 day
**Complexity:** 2/10

---

## Overview

Implement interactive note viewing and deletion using Extended Vanilla Menus ListMenu. Users can browse notes, view details, and delete individual notes.

---

## Requirements

### Functional Requirements

- **FR2.1:** Display all notes in scrollable list via hotkey (dot `.`)
- **FR2.2:** Show note preview in list (truncated text + context + timestamp)
- **FR2.3:** Select note to view full details
- **FR2.4:** Delete individual notes with confirmation
- **FR2.5:** Handle empty notes list gracefully

### Technical Requirements

- **TR2.1:** Hotkey: dot key (`.`) to open notes list
- **TR2.2:** Use ExtendedVanillaMenus.ListMenu() for display
- **TR2.3:** Papyrus bridge functions for note access
- **TR2.4:** C++ DeleteNote(index) function with thread safety
- **TR2.5:** Format timestamps human-readable

### Non-Requirements

- No note editing (future)
- No search/filter (future)
- No export to file (future)
- No console command (not needed)

---

## Design

### User Flow

```
Press dot (.) key
    ↓
ListMenu opens with all notes
    ↓
Select a note
    ↓
Submenu: "View Full Note" / "Delete" / "Cancel"
    ↓
If Delete → Confirmation: "Delete this note?" Yes/No
    ↓
Note deleted → return to list (or close if no notes left)
```

### Component Architecture

```
┌──────────────────────────────────────┐
│       NoteManager (C++)              │
│  - GetAllNotes()                     │
│  - GetNoteCount()                    │
│  - DeleteNote(index)                 │
└──────────────┬───────────────────────┘
               │
               │ Papyrus Bridge
               ↓
┌──────────────────────────────────────┐
│   PersonalNotesNative (Papyrus)      │
│  - GetNoteCount() → int              │
│  - GetNoteText(index) → String       │
│  - GetNoteContext(index) → String    │
│  - GetNoteTimestamp(index) → String  │
│  - DeleteNote(index)                 │
└──────────────┬───────────────────────┘
               │
               ↓
┌──────────────────────────────────────┐
│   PersonalNotes.psc (Papyrus)        │
│  - ShowNotesList()                   │
│  - ShowNoteSubmenu(index)            │
│  - FormatNoteForList(index)          │
└──────────────────────────────────────┘
               │
               ↓
┌──────────────────────────────────────┐
│  ExtendedVanillaMenus.ListMenu()     │
└──────────────────────────────────────┘
```

### Data Structures

#### C++ NoteManager Extensions

```cpp
class NoteManager {
public:
    // Existing
    void AddNote(const std::string& text, const std::string& context);
    std::vector<Note> GetAllNotes() const;
    size_t GetNoteCount() const;

    // New
    bool DeleteNote(size_t index);
    std::string GetNoteText(size_t index) const;
    std::string GetNoteContext(size_t index) const;
    std::time_t GetNoteTimestamp(size_t index) const;
    std::string FormatTimestamp(std::time_t timestamp) const;
};
```

#### Papyrus Bridge Functions

```cpp
namespace PapyrusBridge {
    // Existing
    void OnNoteReceived(RE::StaticFunctionTag*, RE::BSFixedString noteText);
    void RequestTextInput();

    // New
    std::int32_t GetNoteCount(RE::StaticFunctionTag*);
    RE::BSFixedString GetNoteText(RE::StaticFunctionTag*, std::int32_t index);
    RE::BSFixedString GetNoteContext(RE::StaticFunctionTag*, std::int32_t index);
    RE::BSFixedString GetNoteTimestamp(RE::StaticFunctionTag*, std::int32_t index);
    void DeleteNote(RE::StaticFunctionTag*, std::int32_t index);

    bool Register(RE::BSScript::IVirtualMachine* vm);
}
```

#### Papyrus Script Structure

```papyrus
; PersonalNotes.psc
Scriptname PersonalNotes Hidden

; Existing
Function RequestInput() Global
    ; ... (already implemented)
EndFunction

; New
Function ShowNotesList() Global
    Int count = PersonalNotesNative.GetNoteCount()

    If count == 0
        ExtendedVanillaMenus.MessageBox("No notes yet.\n\nPress comma (,) to add a note.", New String[1])
        Return
    EndIf

    ; Build note list for display
    String[] noteOptions = Utility.CreateStringArray(count)
    String[] noteInfos = Utility.CreateStringArray(count)

    Int i = 0
    While i < count
        noteOptions[i] = FormatNoteForList(i)
        noteInfos[i] = PersonalNotesNative.GetNoteText(i)  ; Full text as info
        i += 1
    EndWhile

    Int selected = ExtendedVanillaMenus.ListMenu(noteOptions, noteInfos, "Personal Notes", "Select", "Cancel")

    If selected >= 0
        ShowNoteSubmenu(selected)
    EndIf
EndFunction

String Function FormatNoteForList(Int index) Global
    String text = PersonalNotesNative.GetNoteText(index)
    String context = PersonalNotesNative.GetNoteContext(index)
    String timestamp = PersonalNotesNative.GetNoteTimestamp(index)

    ; Truncate text to 40 chars
    If StringUtil.GetLength(text) > 40
        text = StringUtil.Substring(text, 0, 40) + "..."
    EndIf

    Return "[" + timestamp + "] " + context + ": " + text
EndFunction

Function ShowNoteSubmenu(Int index) Global
    String noteText = PersonalNotesNative.GetNoteText(index)
    String noteContext = PersonalNotesNative.GetNoteContext(index)
    String noteTimestamp = PersonalNotesNative.GetNoteTimestamp(index)

    String fullNote = "[" + noteTimestamp + "] " + noteContext + "\n\n" + noteText

    String[] options = New String[3]
    options[0] = "Delete Note"
    options[1] = "Back to List"
    options[2] = "Close"

    Int choice = ExtendedVanillaMenus.MessageBox(fullNote, options, True)

    If choice == 0  ; Delete
        ConfirmDelete(index)
    ElseIf choice == 1  ; Back to List
        ShowNotesList()
    EndIf
EndFunction

Function ConfirmDelete(Int index) Global
    String[] options = New String[2]
    options[0] = "Cancel"
    options[1] = "Yes, Delete"

    Int choice = ExtendedVanillaMenus.MessageBox("Delete this note?", options, True)

    If choice == 1
        PersonalNotesNative.DeleteNote(index)
        Debug.Notification("Note deleted")
        ShowNotesList()  ; Return to list
    Else
        ShowNoteSubmenu(index)  ; Return to note view
    EndIf
EndFunction
```

#### PersonalNotesNative.psc

```papyrus
Scriptname PersonalNotesNative Hidden

; Existing
Function OnNoteReceived(string noteText) Global Native

; New
Int Function GetNoteCount() Global Native
String Function GetNoteText(int index) Global Native
String Function GetNoteContext(int index) Global Native
String Function GetNoteTimestamp(int index) Global Native
Function DeleteNote(int index) Global Native
```

### Timestamp Format

Format: `MM/DD HH:MM`

Example: `11/12 14:30`

```cpp
std::string NoteManager::FormatTimestamp(std::time_t timestamp) const {
    std::tm* tm = std::localtime(&timestamp);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%m/%d %H:%M", tm);
    return std::string(buffer);
}
```

### InputHandler Extension

```cpp
class InputHandler {
    // ... existing code ...

    void OnHotkeyPressed() {
        // Existing: comma key for add note
        // ...
    }

    void OnViewNotesHotkey() {
        spdlog::info("[HOTKEY] Dot key pressed - viewing notes");

        // Call Papyrus ShowNotesList()
        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            spdlog::error("[VIEW] Failed to get VM");
            return;
        }

        auto args = RE::MakeFunctionArguments();
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

        vm->DispatchStaticCall("PersonalNotes", "ShowNotesList", args, callback);
        spdlog::info("[VIEW] ShowNotesList dispatched");
    }

    RE::BSEventNotifyControl ProcessEvent(...) {
        // ...
        if (buttonEvent->idCode == 51) {  // Comma - add note
            OnHotkeyPressed();
        }
        else if (buttonEvent->idCode == 52) {  // Dot - view notes
            OnViewNotesHotkey();
        }
        // ...
    }
};
```

---

## Tasks

### Task 2.1: C++ - Add Note Access Functions

**Duration:** 1 hour

- Add `GetNoteText(index)` to NoteManager
- Add `GetNoteContext(index)` to NoteManager
- Add `GetNoteTimestamp(index)` to NoteManager
- Add `FormatTimestamp()` helper
- Add bounds checking (return empty if index invalid)
- Add thread-safe read access (shared_lock)
- Add logging for all functions

**Acceptance:**
- [ ] Functions return correct data for valid index
- [ ] Functions return empty string for invalid index
- [ ] Thread-safe (multiple reads concurrent)
- [ ] Logged to PersonalNotes.log

### Task 2.2: C++ - Add DeleteNote Function

**Duration:** 1 hour

- Add `DeleteNote(index)` to NoteManager
- Use unique_lock for write access
- Validate index bounds
- Erase note from vector
- Log deletion (index + text preview)
- Return success/failure bool

**Acceptance:**
- [ ] Valid index: note removed from vector
- [ ] Invalid index: returns false, no crash
- [ ] Thread-safe (exclusive write lock)
- [ ] Logged: "[NOTE] Deleted: 'text...' | Index: N | Remaining: M"

### Task 2.3: C++ - Register Papyrus Native Functions

**Duration:** 1 hour

- Implement `GetNoteCount()` native
- Implement `GetNoteText(index)` native
- Implement `GetNoteContext(index)` native
- Implement `GetNoteTimestamp(index)` native (call FormatTimestamp)
- Implement `DeleteNote(index)` native
- Register all functions in PapyrusBridge::Register()
- Add validation: negative index → clamp to 0

**Acceptance:**
- [ ] All 5 functions registered
- [ ] Callable from Papyrus
- [ ] Return correct types (int/string)
- [ ] Bounds checking works

### Task 2.4: C++ - Add Dot Hotkey Detection

**Duration:** 30 min

- Add dot key detection (idCode == 52)
- Implement `OnViewNotesHotkey()`
- Call `PersonalNotes.ShowNotesList()` via VM dispatch
- Add logging: "[HOTKEY] Dot key pressed - viewing notes"
- Don't trigger if console/menu open

**Acceptance:**
- [ ] Press dot → logs message
- [ ] Calls ShowNotesList()
- [ ] Doesn't trigger in menus/console
- [ ] Works alongside comma hotkey

### Task 2.5: Papyrus - Update PersonalNotesNative.psc

**Duration:** 15 min

- Add native function declarations:
  - `GetNoteCount()`
  - `GetNoteText(index)`
  - `GetNoteContext(index)`
  - `GetNoteTimestamp(index)`
  - `DeleteNote(index)`
- Compile with compile-papyrus.ps1

**Acceptance:**
- [ ] Compiles without errors
- [ ] .pex file generated

### Task 2.6: Papyrus - Implement ShowNotesList()

**Duration:** 2 hours

- Add `ShowNotesList()` function
- Handle empty notes (show helpful message)
- Build note options array
- Format each note: `[timestamp] context: text...`
- Truncate text to 40 chars
- Call `ExtendedVanillaMenus.ListMenu()`
- On selection → call `ShowNoteSubmenu(index)`

**Acceptance:**
- [ ] Empty list: shows "No notes yet" message
- [ ] Non-empty: displays all notes
- [ ] Timestamps formatted correctly
- [ ] Text truncated with "..."
- [ ] Selecting note calls submenu

### Task 2.7: Papyrus - Implement ShowNoteSubmenu()

**Duration:** 1 hour

- Add `ShowNoteSubmenu(index)` function
- Display full note text with MessageBox
- Show options: "Delete Note" / "Back to List" / "Close"
- Handle each choice:
  - Delete → call `ConfirmDelete(index)`
  - Back to List → call `ShowNotesList()`
  - Close → exit

**Acceptance:**
- [ ] Full note text displayed
- [ ] All 3 options work
- [ ] Delete triggers confirmation
- [ ] Back returns to list
- [ ] Close exits cleanly

### Task 2.8: Papyrus - Implement ConfirmDelete()

**Duration:** 30 min

- Add `ConfirmDelete(index)` function
- Show confirmation: "Delete this note?"
- Options: "Cancel" / "Yes, Delete"
- If Yes:
  - Call `PersonalNotesNative.DeleteNote(index)`
  - Show notification: "Note deleted"
  - Return to `ShowNotesList()`
- If Cancel → return to `ShowNoteSubmenu(index)`

**Acceptance:**
- [ ] Confirmation shows before delete
- [ ] Yes: note deleted, returns to list
- [ ] Cancel: returns to note view
- [ ] Notification shown on delete

### Task 2.9: Compile and Deploy

**Duration:** 15 min

- Compile C++ with CMake
- Compile Papyrus with compile-papyrus.ps1
- Run deploy.ps1
- Copy to test Skyrim installation
- Verify all files present:
  - PersonalNotes.dll
  - PersonalNotes.pex
  - PersonalNotesNative.pex

**Acceptance:**
- [ ] No compilation errors
- [ ] All files in mod/ directory
- [ ] Ready for in-game testing

### Task 2.10: Testing

**Duration:** 1 hour

**Test cases:**

1. Empty notes list
   - [ ] Press dot → "No notes yet" message

2. Single note
   - [ ] Press dot → list shows 1 note
   - [ ] Select → submenu appears
   - [ ] View full text → correct
   - [ ] Delete → confirmation → deleted
   - [ ] Press dot again → "No notes yet"

3. Multiple notes (10+)
   - [ ] Press dot → list scrollable
   - [ ] All notes formatted correctly
   - [ ] Timestamps in order (newest first?)
   - [ ] Select any note → correct note shown
   - [ ] Delete middle note → list updates
   - [ ] Count decreases

4. Long note text
   - [ ] Text truncated in list (40 chars + "...")
   - [ ] Full text shown in submenu
   - [ ] No overflow/crash

5. Edge cases
   - [ ] Delete last note → "No notes yet"
   - [ ] Rapid hotkey presses → no crash
   - [ ] Delete while another script adding → thread-safe

6. Serialization
   - [ ] Add 5 notes, save game
   - [ ] Delete 2 notes, save game
   - [ ] Load game → 3 notes present
   - [ ] Correct notes deleted (persistence works)

---

## Acceptance Criteria

### AC2.1: Viewing Notes Works
- [ ] Press dot key → ListMenu opens
- [ ] All notes displayed in list
- [ ] Notes formatted: `[MM/DD HH:MM] Context: Text...`
- [ ] Text truncated to 40 chars
- [ ] Empty list shows helpful message

### AC2.2: Selecting Notes Works
- [ ] Select note from list → submenu opens
- [ ] Full note text displayed
- [ ] Timestamp and context shown
- [ ] Three options available

### AC2.3: Deleting Notes Works
- [ ] "Delete Note" → confirmation shown
- [ ] "Yes, Delete" → note removed
- [ ] Returns to list (or "No notes" if last)
- [ ] Notification shown: "Note deleted"
- [ ] Deleted note not in next view

### AC2.4: Navigation Works
- [ ] "Back to List" returns to list
- [ ] "Close" exits menu
- [ ] "Cancel" in delete confirmation returns to submenu
- [ ] No infinite loops

### AC2.5: Thread Safety
- [ ] No crashes with rapid hotkey presses
- [ ] Delete during add → no corruption
- [ ] Save during delete → consistent state

### AC2.6: Persistence
- [ ] Delete note → save → load → note gone
- [ ] Add note → delete → save → load → still gone
- [ ] Remaining notes intact after deletion

### AC2.7: Edge Cases
- [ ] Index out of bounds → graceful failure
- [ ] Delete last note → clean empty state
- [ ] Very long text → truncated, no crash
- [ ] Special characters → display correctly

---

## Implementation Notes

### Thread Safety for DeleteNote

```cpp
bool NoteManager::DeleteNote(size_t index) {
    std::unique_lock lock(lock_);

    if (index >= notes_.size()) {
        spdlog::warn("[DELETE] Invalid index: {} (size: {})", index, notes_.size());
        return false;
    }

    std::string preview = notes_[index].text;
    if (preview.length() > 30) {
        preview = preview.substr(0, 30) + "...";
    }

    notes_.erase(notes_.begin() + index);

    spdlog::info("[DELETE] Deleted note at index {}: '{}' | Remaining: {}",
        index, preview, notes_.size());

    return true;
}
```

### Logging Additions

New log prefixes:
- `[VIEW]` - Viewing notes
- `[DELETE]` - Deleting notes
- `[FORMAT]` - Formatting operations

---

## Dependencies

### Runtime
- Phase 1 complete (NoteManager, serialization, add note)
- Extended Vanilla Menus installed
- SKSE64

### Build
- CommonLibSSE-NG
- spdlog
- Papyrus Compiler

---

## Deliverables

1. ✅ C++ NoteManager extensions (access + delete)
2. ✅ Papyrus native function bridge
3. ✅ PersonalNotes.psc display logic
4. ✅ PersonalNotesNative.psc declarations
5. ✅ Dot hotkey implementation
6. ✅ Compiled DLL + PEX files
7. ✅ Testing checklist completed

---

## Next Steps

**After Phase 2:**
- Project complete for MVP v1.0
- Package for distribution
- Create Nexus mod page (optional)

**Future Enhancements (v2.0):**
- Note editing
- Search/filter by context
- Export notes to text file
- Sort options (date, context, alphabetical)
- Note categories/tags
- Favorites/pinning

---

**Phase 2 Status:** Ready to implement
**Estimated Time:** 1 day (6-8 hours)
**Complexity:** 2/10
