# Phase 3: Journal Integration - Quest Notes - PRD

**Duration:** 1 day
**Complexity:** 3/10

---

## Overview

Replace general notes system with quest-specific notes. When viewing a quest in the Journal Menu, pressing comma opens a note editor for that quest. One note per quest, editable at any time. No journal modification - pure integration via hotkey detection.

---

## Requirements

### Functional Requirements

- **FR3.1:** Detect which quest is being viewed in Journal Menu
- **FR3.2:** Comma hotkey ONLY works when Journal Menu is open
- **FR3.3:** One note per quest (editable, not replaceable)
- **FR3.4:** If note exists: open in edit mode with existing text
- **FR3.5:** If no note exists: open empty text input
- **FR3.6:** Notes persist across saves
- **FR3.7:** New game = fresh notes (no carry-over)

### Technical Requirements

- **TR3.1:** Query Scaleform for `selectedQuestID` variable
- **TR3.2:** Store notes with quest FormID as key
- **TR3.3:** Hotkey disabled outside Journal Menu
- **TR3.4:** Use ExtendedVanillaMenus for text input (no transparency)
- **TR3.5:** Update serialization to include questID
- **TR3.6:** Remove general note functionality (comma/dot in-game)

### Non-Requirements

- No general notes (Phase 1 features removed)
- No read-only mode (always editable)
- No journal UI modification
- No inline display in journal
- No multiple notes per quest

---

## Design

### User Flow

```
Player opens Journal → selects quest
    ↓
Press comma (,)
    ↓
Check if note exists for this quest
    ↓
IF exists: Open text input with existing note text (edit mode)
IF not exists: Open empty text input (create mode)
    ↓
User edits/creates note
    ↓
User confirms → note saved/updated
User cancels → no changes
```

### Component Architecture

```
┌──────────────────────────────────────┐
│       InputHandler (C++)              │
│  - Detect comma ONLY in Journal      │
│  - Get current quest via Scaleform   │
│  - Call ShowQuestNoteInput(questID)  │
└──────────────┬───────────────────────┘
               │
               ↓
┌──────────────────────────────────────┐
│      NoteManager (C++)                │
│  - GetNoteForQuest(questID)          │
│  - SaveNoteForQuest(questID, text)   │
│  - Map<FormID, Note> storage         │
└──────────────┬───────────────────────┘
               │
               ↓
┌──────────────────────────────────────┐
│   PapyrusBridge (C++)                 │
│  - ShowQuestNoteInput(questID)       │
│  - GetQuestNoteName(questID)         │
└──────────────┬───────────────────────┘
               │
               ↓
┌──────────────────────────────────────┐
│   PersonalNotes.psc (Papyrus)        │
│  - ShowQuestNoteInput(questID, text) │
│  - ExtendedVanillaMenus.TextInput()  │
└──────────────────────────────────────┘
```

### Data Structures

#### Note Structure (Updated)

```cpp
struct Note {
    std::string text;
    std::time_t timestamp;
    RE::FormID questID;  // NEW: Quest this note belongs to

    // Remove: std::string context (quest name IS the context)
};
```

#### NoteManager (Refactored)

```cpp
class NoteManager {
public:
    // NEW: Quest-specific storage
    std::string GetNoteForQuest(RE::FormID questID) const;
    void SaveNoteForQuest(RE::FormID questID, const std::string& text);
    bool HasNoteForQuest(RE::FormID questID) const;
    void DeleteNoteForQuest(RE::FormID questID);

    // Keep for serialization
    std::unordered_map<RE::FormID, Note> GetAllNotes() const;
    size_t GetNoteCount() const;

private:
    // CHANGED: Map instead of vector
    std::unordered_map<RE::FormID, Note> notesByQuest_;
    mutable std::shared_mutex lock_;
};
```

#### Journal Quest Detection

```cpp
RE::FormID GetCurrentQuestInJournal() {
    auto ui = RE::UI::GetSingleton();
    if (!ui || !ui->IsMenuOpen("Journal Menu")) {
        return 0;  // Not in journal
    }

    auto menu = ui->GetMenu("Journal Menu");
    if (!menu || !menu->uiMovie) {
        return 0;
    }

    // Query Scaleform (from swfdump analysis)
    RE::GFxValue result;
    if (!menu->uiMovie->GetVariable(&result,
        "_root.QuestJournalFader.Menu_mc.selectedQuestID")) {
        return 0;
    }

    if (result.IsUInt()) {
        return result.GetUInt();
    }

    return 0;
}
```

### Scaleform Variable Paths (From swfdump)

**Discovered from swfdump output:**
- `selectedQuestID` - Quest FormID
- `selectedQuestInstance` - Quest object reference
- `QuestJournalFader` - Main quest UI container

**Likely paths to try:**
1. `_root.QuestJournalFader.Menu_mc.selectedQuestID` (most likely)
2. `_root.Menu_mc.selectedQuestID`
3. `_root.selectedQuestID`

### Input Handler Changes

```cpp
class InputHandler {
    RE::BSEventNotifyControl ProcessEvent(...) {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;

        for (auto event = *a_event; event; event = event->next) {
            if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) continue;

            auto buttonEvent = event->AsButtonEvent();
            if (!buttonEvent || !buttonEvent->IsDown()) continue;

            // ONLY comma, ONLY in Journal
            if (buttonEvent->idCode == 51) {  // Comma
                OnQuestNoteHotkey();
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }

private:
    void OnQuestNoteHotkey() {
        // MUST be in Journal Menu
        auto ui = RE::UI::GetSingleton();
        if (!ui || !ui->IsMenuOpen("Journal Menu")) {
            spdlog::info("[HOTKEY] Comma pressed but not in Journal, ignoring");
            return;
        }

        // Get current quest
        RE::FormID questID = GetCurrentQuestInJournal();
        if (questID == 0) {
            spdlog::warn("[HOTKEY] No quest selected in Journal");
            RE::DebugNotification("No quest selected");
            return;
        }

        spdlog::info("[HOTKEY] Comma pressed in Journal - Quest: 0x{:X}", questID);

        // Trigger Papyrus note input
        PapyrusBridge::ShowQuestNoteInput(questID);
    }
};
```

### Papyrus Bridge Changes

```cpp
namespace PapyrusBridge {
    // NEW: Show quest note input
    void ShowQuestNoteInput(RE::FormID questID) {
        spdlog::info("[PAPYRUS] Showing note input for quest 0x{:X}", questID);

        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            spdlog::error("[PAPYRUS] Failed to get VM");
            return;
        }

        // Get quest name for display
        auto quest = RE::TESForm::LookupByID<RE::TESQuest>(questID);
        std::string questName = quest ? quest->GetName() : "Unknown Quest";

        // Get existing note text (if any)
        auto mgr = NoteManager::GetSingleton();
        std::string existingText = mgr->GetNoteForQuest(questID);

        // Call Papyrus with quest info
        auto args = RE::MakeFunctionArguments(
            static_cast<std::int32_t>(questID),
            RE::BSFixedString(questName),
            RE::BSFixedString(existingText)
        );
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

        vm->DispatchStaticCall("PersonalNotes", "ShowQuestNoteInput", args, callback);
        spdlog::info("[PAPYRUS] ShowQuestNoteInput dispatched");
    }

    // NEW: Save quest note
    void SaveQuestNote(RE::StaticFunctionTag*, std::int32_t questID, RE::BSFixedString noteText) {
        if (questID <= 0) {
            spdlog::warn("[PAPYRUS] Invalid quest ID: {}", questID);
            return;
        }

        std::string text(noteText.c_str());
        if (text.empty()) {
            spdlog::info("[PAPYRUS] Empty note text, deleting note for quest 0x{:X}", questID);
            NoteManager::GetSingleton()->DeleteNoteForQuest(static_cast<RE::FormID>(questID));
        } else {
            spdlog::info("[PAPYRUS] Saving note for quest 0x{:X}: '{}'", questID, text);
            NoteManager::GetSingleton()->SaveNoteForQuest(static_cast<RE::FormID>(questID), text);
        }

        RE::DebugNotification("Quest note saved!");
    }

    // Register functions
    bool Register(RE::BSScript::IVirtualMachine* vm) {
        vm->RegisterFunction("SaveQuestNote", "PersonalNotesNative", SaveQuestNote);
        // ... other registrations
        spdlog::info("[PAPYRUS] Native functions registered");
        return true;
    }
}
```

### Papyrus Script (Simplified)

```papyrus
Scriptname PersonalNotes Hidden

; Called from C++ when comma pressed in Journal
Function ShowQuestNoteInput(Int questID, String questName, String existingText) Global
    ; Show text input with quest name as title
    String title = "Note: " + questName

    ; Pre-fill with existing text (empty if new note)
    String result = ExtendedVanillaMenus.TextInput(title, existingText, \
        BackgroundAlpha = 100, ShowTextBackground = True, MultiLine = True)

    ; If user entered text (or empty to delete), save it
    If result != existingText
        PersonalNotesNative.SaveQuestNote(questID, result)
    EndIf
EndFunction
```

### Serialization Format

**Updated format:**
```
[Record Header: 'PNOT', Version 2]
[Note Count: uint32_t]
For each note:
    [Quest FormID: uint32_t]
    [Text Length: uint32_t]
    [Text Data: char[]]
    [Timestamp: time_t]
```

**Changes from Phase 1:**
- Version bumped to 2
- Removed context string
- Added quest FormID

---

## Tasks

### Task 3.1: Update Note Structure

**Duration:** 30 min

- Remove `context` field from Note struct
- Add `questID` field (RE::FormID)
- Update Note constructor
- Update Save/Load methods

**Acceptance:**
- [ ] Note struct has questID field
- [ ] Compiles without errors

### Task 3.2: Refactor NoteManager to Map-Based Storage

**Duration:** 1 hour

- Change `std::vector<Note>` to `std::unordered_map<RE::FormID, Note>`
- Implement `GetNoteForQuest(questID)`
- Implement `SaveNoteForQuest(questID, text)`
- Implement `HasNoteForQuest(questID)`
- Implement `DeleteNoteForQuest(questID)`
- Remove vector-based methods (GetNoteText, GetNoteContext, etc.)
- Keep GetAllNotes() for serialization

**Acceptance:**
- [ ] Map-based storage works
- [ ] Thread-safe with shared_mutex
- [ ] Can save/retrieve notes by quest ID

### Task 3.3: Implement GetCurrentQuestInJournal()

**Duration:** 1 hour

- Add function to query Scaleform for selectedQuestID
- Try multiple path patterns from swfdump
- Add error handling and logging
- Return 0 if not in journal or no quest selected

**Acceptance:**
- [ ] Returns valid quest FormID when in journal
- [ ] Returns 0 when not in journal
- [ ] Logs which path worked
- [ ] No crashes on invalid paths

### Task 3.4: Update InputHandler for Journal-Only Hotkey

**Duration:** 30 min

- Remove dot key handler
- Update comma handler to check Journal Menu state
- Call GetCurrentQuestInJournal()
- Call PapyrusBridge::ShowQuestNoteInput(questID)
- Add logging for all cases

**Acceptance:**
- [ ] Comma only works in Journal
- [ ] Comma ignored outside Journal
- [ ] Logs quest ID when triggered
- [ ] Notification if no quest selected

### Task 3.5: Implement PapyrusBridge Quest Note Functions

**Duration:** 1 hour

- Implement ShowQuestNoteInput(questID)
- Get quest name via TESQuest::LookupByID()
- Get existing note text
- Call Papyrus with 3 parameters
- Implement SaveQuestNote() native function
- Register new functions

**Acceptance:**
- [ ] ShowQuestNoteInput dispatches to Papyrus
- [ ] SaveQuestNote saves/deletes notes
- [ ] Quest name retrieved correctly
- [ ] Empty text deletes note

### Task 3.6: Update Papyrus Scripts

**Duration:** 30 min

- Rewrite PersonalNotes.psc for quest notes
- Remove ShowNotesList, ShowNoteSubmenu, ConfirmDelete
- Implement ShowQuestNoteInput(questID, questName, existingText)
- Update PersonalNotesNative.psc declarations
- Remove unused native functions

**Acceptance:**
- [ ] ShowQuestNoteInput displays correctly
- [ ] Quest name shown in title
- [ ] Existing text pre-filled
- [ ] Compiles without errors

### Task 3.7: Update Serialization to Version 2

**Duration:** 1 hour

- Bump kSerializationVersion to 2
- Update Save() to write questID
- Update Load() to handle both v1 and v2
  - v1: skip/ignore (incompatible)
  - v2: read questID + text + timestamp
- Update Revert() to clear map
- Add migration logging

**Acceptance:**
- [ ] Saves notes with quest IDs
- [ ] Loads notes correctly
- [ ] Old v1 saves handled gracefully
- [ ] No data corruption

### Task 3.8: Remove Phase 2 Features

**Duration:** 30 min

- Remove GetNoteText, GetNoteContext, GetNoteTimestamp natives
- Remove DeleteNote native
- Remove FormatTimestamp
- Remove ShowNotesList Papyrus function
- Clean up unused code

**Acceptance:**
- [ ] Compiles after removal
- [ ] No dead code remains
- [ ] Plugin size reduced

### Task 3.9: Compile and Deploy

**Duration:** 15 min

- Build C++ plugin
- Compile Papyrus scripts
- Run deploy.ps1
- Verify files present

**Acceptance:**
- [ ] No compilation errors
- [ ] DLL and PEX files generated
- [ ] Files in mod/ directory

### Task 3.10: Testing

**Duration:** 2 hours

**Test cases:**

1. Journal detection
   - [ ] Open journal → press comma → note input opens
   - [ ] Outside journal → press comma → nothing happens
   - [ ] No quest selected → press comma → notification shown

2. New note
   - [ ] Select quest → press comma → empty input
   - [ ] Enter text → confirm → saved
   - [ ] Press comma again → existing text shown

3. Edit note
   - [ ] Quest with note → press comma → text pre-filled
   - [ ] Modify text → confirm → updated
   - [ ] Cancel → no changes

4. Delete note
   - [ ] Quest with note → press comma → clear all text
   - [ ] Confirm → note deleted
   - [ ] Press comma again → empty input

5. Multiple quests
   - [ ] Add notes to 3 different quests
   - [ ] Each quest shows correct note
   - [ ] No mixing/corruption

6. Serialization
   - [ ] Add 3 notes → save game
   - [ ] Load game → notes present
   - [ ] Edit note → save → load → changes persist

7. Edge cases
   - [ ] Quest completes → note still accessible
   - [ ] Quest fails → note still accessible
   - [ ] Very long text (500+ chars) → saves correctly
   - [ ] Special characters → display correctly

8. Scaleform path validation
   - [ ] Log which path pattern worked
   - [ ] Try all 3 paths if needed
   - [ ] Handle invalid paths gracefully

---

## Acceptance Criteria

### AC3.1: Journal Detection Works
- [ ] Comma only triggers in Journal Menu
- [ ] Current quest FormID retrieved correctly
- [ ] Scaleform path pattern logged
- [ ] No crashes on path failures

### AC3.2: Quest Notes Work
- [ ] One note per quest
- [ ] Create new note works
- [ ] Edit existing note works
- [ ] Delete note (empty text) works
- [ ] Quest name shown in title

### AC3.3: UI Integration Clean
- [ ] No journal modification required
- [ ] Text input has opaque background
- [ ] Multi-line input works
- [ ] Cancel preserves existing note

### AC3.4: Data Persistence
- [ ] Notes save with quest IDs
- [ ] Notes load correctly
- [ ] Version 2 format works
- [ ] No save corruption

### AC3.5: Phase 1 Removed
- [ ] No general notes
- [ ] Comma disabled outside journal
- [ ] Dot key removed entirely
- [ ] Phase 2 features removed

### AC3.6: Performance
- [ ] No lag when pressing comma
- [ ] Scaleform query <1ms
- [ ] Thread-safe map access
- [ ] No memory leaks

---

## Implementation Notes

### Scaleform Path Discovery

**From swfdump analysis, try in order:**

1. `_root.QuestJournalFader.Menu_mc.selectedQuestID`
2. `_root.QuestJournalFader.selectedQuestID`
3. `_root.Menu_mc.selectedQuestID`
4. `_root.selectedQuestID`

**Logging pattern:**
```cpp
const char* paths[] = {
    "_root.QuestJournalFader.Menu_mc.selectedQuestID",
    "_root.QuestJournalFader.selectedQuestID",
    "_root.Menu_mc.selectedQuestID",
    "_root.selectedQuestID"
};

for (const char* path : paths) {
    if (movie->GetVariable(&result, path)) {
        spdlog::info("[JOURNAL] SUCCESS: Found quest ID at path: {}", path);
        return result.GetUInt();
    }
    spdlog::debug("[JOURNAL] FAILED: Path not found: {}", path);
}
```

### Map vs Vector Trade-offs

**Why Map:**
- O(1) quest lookup (vs O(n) in vector)
- Natural one-note-per-quest enforcement
- Easy deletion
- Clean API (GetNoteForQuest vs GetNoteText(index))

**Downside:**
- No ordering (but we don't need it)
- Slightly more memory overhead (acceptable)

### Serialization Compatibility

**Version 1 (Phase 1):**
```
[count][note1: text, timestamp, context][note2: ...]
```

**Version 2 (Phase 3):**
```
[count][note1: questID, text, timestamp][note2: ...]
```

**Migration strategy:** Don't migrate - old saves lose notes. Clean break.

---

## Dependencies

### Runtime
- Phase 1 foundation (serialization, logging)
- Extended Vanilla Menus (text input)
- Journal Menu must be open
- Quest must exist in game data

### Build
- CommonLibSSE-NG
- spdlog
- Papyrus Compiler

---

## Deliverables

1. ✅ Updated NoteManager with map-based storage
2. ✅ GetCurrentQuestInJournal() function
3. ✅ Journal-only hotkey detection
4. ✅ Quest note Papyrus bridge
5. ✅ Simplified Papyrus scripts
6. ✅ Version 2 serialization
7. ✅ Phase 2 features removed
8. ✅ Testing checklist completed

---

## Risks & Mitigations

### Risk 1: Scaleform Path Unknown
**Impact:** HIGH - Feature won't work
**Mitigation:** Try all 4 path patterns, log extensively
**Fallback:** Use alternative method (questsTab direct access)

### Risk 2: Journal Menu State Changes
**Impact:** MEDIUM - Quest detection may fail
**Mitigation:** Validate menu state before every query
**Fallback:** Graceful error messages to user

### Risk 3: Save Compatibility Break
**Impact:** LOW - Users lose old notes
**Mitigation:** Document in release notes
**Acceptance:** Clean break acceptable for prototype

---

## Next Steps

**After Phase 3:**
- Test extensively with various quests
- Gather user feedback
- Consider Phase 4: Quest list browser (optional)

**Future Enhancements (v2.0):**
- Quest objectives as sub-notes
- Note categories (TODO, INFO, SPOILER)
- Export quest notes to text file
- Share notes between characters

---

**Phase 3 Status:** Ready to implement
**Estimated Time:** 1 day (6-8 hours)
**Complexity:** 3/10
**Success Probability:** 98% (only unknown: exact Scaleform path)
