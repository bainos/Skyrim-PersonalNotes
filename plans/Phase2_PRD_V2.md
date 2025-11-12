# Phase 2: Note Display System - PRD v2

**Duration:** 2-3 days
**Complexity:** 1/10

---

## Overview

Implement simple methods to view saved notes. Provide multiple display options: message box, console command, and Extended Vanilla Menus list.

---

## Requirements

### Functional Requirements

- **FR2.1:** Display recent notes (last 10) via hotkey
- **FR2.2:** Display all notes via console command
- **FR2.3:** Show note text, timestamp, and context
- **FR2.4:** Format timestamps human-readable
- **FR2.5:** Optional: Use Extended Vanilla Menus ListMenu for browsing

### Technical Requirements

- **TR2.1:** Hotkey for viewing notes (Shift+Insert)
- **TR2.2:** Console command: `shownotes`
- **TR2.3:** Format notes with UTF-8 support
- **TR2.4:** Paginated display if too many notes
- **TR2.5:** Thread-safe read access to notes

### Non-Requirements

- No note editing (future)
- No note deletion (future)
- No search/filter (future)
- No export to file (future)

---

## Design

### Display Methods

#### Option A: Message Box (Primary)

**Use:** Quick view of recent notes

```cpp
void DisplayRecentNotes(size_t count = 10) {
    auto notes = NoteManager::GetSingleton()->GetRecentNotes(count);
    std::string message = FormatNotesForMessageBox(notes);
    RE::DebugMessageBox(message.c_str());
}
```

**Format:**
```
━━━ PERSONAL NOTES ━━━

[11/12 14:30] Whiterun
Check blacksmith for steel

[11/12 15:45] General
Remember to sell dragon bones

[11/12 16:20] Bleak Falls Barrow
Golden Claw on table upstairs

(Showing 3 of 15 notes)
Use 'shownotes' to see all
```

#### Option B: Console Command

**Use:** View all notes, searchable in console history

```cpp
// Register console command
void RegisterConsoleCommand() {
    // SKSE console command registration
    // Command: shownotes
}

void ShowAllNotesConsole() {
    auto notes = NoteManager::GetSingleton()->GetAllNotes();
    for (size_t i = 0; i < notes.size(); ++i) {
        RE::ConsoleLog::GetSingleton()->Print(
            "[%d] %s | %s | %s",
            i + 1,
            FormatTimestamp(notes[i].timestamp).c_str(),
            notes[i].context.c_str(),
            notes[i].text.c_str()
        );
    }
}
```

#### Option C: Extended Vanilla Menus List (Optional)

**Use:** Interactive browsing with clickable entries

```papyrus
Function ShowNotesListMenu() Global
    String[] noteTexts = PersonalNotesNative.GetNoteTexts()
    String[] noteInfos = PersonalNotesNative.GetNoteInfos()

    Int selected = ExtendedVanillaMenus.ListMenu(
        Options = noteTexts,
        OptionInfos = noteInfos,
        TitleText = "Personal Notes",
        AcceptText = "Close",
        CancelText = "Cancel"
    )
EndFunction
```

### Component Architecture

```
┌─────────────────────────────────────┐
│         NoteManager (C++)           │
│  GetRecentNotes(count)              │
│  GetAllNotes()                      │
│  FormatNote(note) → string          │
└───────────┬─────────────────────────┘
            │
    ┌───────┴────────┬─────────────┐
    │                │             │
┌───▼─────┐  ┌───────▼──────┐  ┌──▼──────────┐
│MessageBox│  │Console Cmd   │  │ListMenu     │
│Display   │  │"shownotes"   │  │(EVM)        │
└──────────┘  └──────────────┘  └─────────────┘
```

### Data Structures

#### Display Formatter

```cpp
class NoteFormatter {
public:
    static std::string FormatTimestamp(std::time_t timestamp);
    static std::string FormatNote(const Note& note);
    static std::string FormatNotesForMessageBox(const std::vector<Note>& notes);
    static std::string FormatNoteForConsole(const Note& note, size_t index);

private:
    static std::string TruncateText(const std::string& text, size_t maxLen);
};
```

### Hotkey System Extension

```cpp
class InputHandler {
    // ... existing AddNote hotkey ...

    void OnViewNotesHotkey(); // New: Shift+Insert
    bool IsViewNotesHotkey(RE::ButtonEvent* event);

    uint32_t hotkeyViewNotes_ = 0xD2; // Shift+Insert
};
```

---

## Tasks

### Task 2.1: Implement Note Formatting
- Create NoteFormatter class
- Implement FormatTimestamp()
  - Convert time_t to "MM/DD HH:MM" format
  - Handle timezone (local time)
- Implement FormatNote()
  - Include timestamp, context, text
  - Truncate long text (max 80 chars for previews)
- Implement FormatNotesForMessageBox()
  - Header + formatted notes + footer
- Test with various note counts (0, 1, 10, 100)

**Time:** 2-3 hours

### Task 2.2: Implement Message Box Display
- Add GetRecentNotes(count) to NoteManager
  - Return last N notes (newest first)
  - Thread-safe read access
- Implement DisplayRecentNotes()
  - Format notes for message box
  - Show count (e.g., "Showing 10 of 50")
  - Add instructions footer
- Handle edge cases (no notes, 1 note, etc.)
- Test in-game

**Time:** 2-3 hours

### Task 2.3: Implement Console Command
- Research SKSE console command registration
  - Check CommonLibSSE-NG console API
  - Use RE::ConsoleLog or similar
- Register "shownotes" command
- Implement ShowAllNotesConsole()
  - Print all notes to console
  - Number each note
  - Format: [#] timestamp | context | text
- Test via in-game console

**Time:** 3-4 hours

### Task 2.4: Add View Notes Hotkey
- Extend InputHandler to detect Shift+Insert
  - Check for shift modifier + Insert key
- Call DisplayRecentNotes() on hotkey
- Load hotkey from INI (configurable)
- Add logging for view hotkey
- Test: press Shift+Insert → see notes

**Time:** 2 hours

### Task 2.5: Optional Extended Vanilla Menus List
- Extend Papyrus bridge:
  - GetNoteTexts() → string array
  - GetNoteInfos() → string array (timestamp+context)
- Create Papyrus function ShowNotesListMenu()
  - Call ExtendedVanillaMenus.ListMenu()
  - Pass formatted note data
- Add hotkey option to trigger list menu
- Test interactive selection

**Time:** 3-4 hours (optional)

### Task 2.6: Polish and Edge Cases
- Handle empty notes list
  - Message: "No notes yet. Press Insert to add a note."
- Handle very long notes
  - Truncate with "..." in previews
- Handle special characters in text
  - UTF-8 encoding issues
- Test with maximum notes (100)
- Add pagination hints if needed

**Time:** 2-3 hours

### Task 2.7: Update Configuration
- Add ViewNotesHotkey to INI
- Add DisplayMethod option (MessageBox, Console, List)
- Add RecentNotesCount option (default: 10)
- Document all options in INI comments

**Time:** 1 hour

---

## Acceptance Criteria

### AC2.1: Message Box Display Works
- [ ] Press Shift+Insert
- [ ] Message box appears
- [ ] Shows last 10 notes (or fewer if <10 total)
- [ ] Each note has timestamp, context, text
- [ ] Timestamps formatted readably
- [ ] Footer shows total count

### AC2.2: Console Command Works
- [ ] Open console (~)
- [ ] Type `shownotes`
- [ ] All notes printed to console
- [ ] Each note numbered
- [ ] Format: [#] timestamp | context | text
- [ ] Can scroll through all notes

### AC2.3: Formatting Correct
- [ ] Timestamps show as "MM/DD HH:MM"
- [ ] Context shows cell name or "General"
- [ ] Long notes truncated with "..."
- [ ] Special characters display correctly
- [ ] No garbled text

### AC2.4: Edge Cases Handled
- [ ] Zero notes: "No notes yet" message
- [ ] One note: displays correctly
- [ ] 100 notes: no crash, shows recent 10
- [ ] Very long note text: truncated gracefully
- [ ] Unicode characters: display correctly

### AC2.5: Hotkey Configuration Works
- [ ] Change ViewNotesHotkey in INI
- [ ] Restart game
- [ ] New hotkey works
- [ ] Old hotkey doesn't trigger

### AC2.6: Performance
- [ ] Viewing 100 notes: no lag
- [ ] Message box opens instantly
- [ ] Console print completes quickly
- [ ] No memory leaks

### AC2.7: Optional List Menu (if implemented)
- [ ] Trigger list menu via hotkey/command
- [ ] Notes displayed in scrollable list
- [ ] Can select note to see full text
- [ ] Can cancel and close menu
- [ ] Consistent with Extended Vanilla Menus style

---

## Implementation Notes

### Timestamp Formatting

```cpp
std::string NoteFormatter::FormatTimestamp(std::time_t timestamp) {
    std::tm* tm = std::localtime(&timestamp);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%m/%d %H:%M", tm);
    return std::string(buffer);
}
```

### Message Box Example

```cpp
void DisplayRecentNotes(size_t count) {
    auto notes = NoteManager::GetSingleton()->GetRecentNotes(count);

    if (notes.empty()) {
        RE::DebugMessageBox("No notes yet.\n\nPress Insert to add a note.");
        return;
    }

    std::ostringstream oss;
    oss << "━━━ PERSONAL NOTES ━━━\n\n";

    for (const auto& note : notes) {
        oss << "[" << NoteFormatter::FormatTimestamp(note.timestamp) << "] ";
        oss << note.context << "\n";
        oss << NoteFormatter::TruncateText(note.text, 60) << "\n\n";
    }

    size_t total = NoteManager::GetSingleton()->GetNoteCount();
    oss << "(Showing " << notes.size() << " of " << total << " notes)\n";
    oss << "Use 'shownotes' to see all";

    RE::DebugMessageBox(oss.str().c_str());
}
```

### Console Command Registration

```cpp
// Research needed: Check CommonLibSSE-NG for console command API
// May need to use SKSE messaging or hook console dispatcher

void RegisterConsoleCommand() {
    // Register "shownotes" command
    // When executed, calls ShowAllNotesConsole()
}
```

### INI Configuration (Updated)

```ini
[General]
AddNoteHotkey = 82          ; Insert
ViewNotesHotkey = 210       ; Shift+Insert
MaxNotes = 100
EnableContextTagging = 1

[Display]
DefaultDisplayMethod = 0    ; 0=MessageBox, 1=Console, 2=ListMenu
RecentNotesCount = 10       ; How many notes to show in MessageBox
TruncateLength = 60         ; Max chars in preview
```

---

## Testing Checklist

**Before Release:**

- [ ] Add 15 notes with various contexts
- [ ] Press Shift+Insert → see last 10
- [ ] Open console → `shownotes` → see all 15
- [ ] Timestamps formatted correctly
- [ ] Contexts displayed correctly
- [ ] Long notes truncated properly
- [ ] Empty notes: helpful message shown
- [ ] Change hotkey in INI → works
- [ ] Change RecentNotesCount → reflects in display
- [ ] No crashes with 100 notes
- [ ] Thread-safe (test rapid hotkey presses)
- [ ] Console command repeatable (no memory issues)

---

## Dependencies

### Runtime
- Phase 1 (NoteManager must be complete)
- Extended Vanilla Menus (if using ListMenu option)

---

## Deliverables

1. ✅ NoteFormatter class (C++)
2. ✅ Display functions (MessageBox, Console)
3. ✅ View notes hotkey implementation
4. ✅ Console command registration
5. ✅ Optional: ListMenu Papyrus integration
6. ✅ Updated INI configuration
7. ✅ Documentation (usage instructions)

---

## Next Steps

**After Phase 2:**

Project is **COMPLETE** (v1.0).

**Future Enhancements (v2.0):**
- Note editing
- Note deletion
- Search/filter
- Export to text file
- Journal integration (if requested)

---

**Phase 2 Status:** Ready to implement after Phase 1
**Estimated Time:** 2-3 days
**Complexity:** 1/10 (very simple)
