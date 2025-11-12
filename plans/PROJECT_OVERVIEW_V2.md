# Skyrim Personal Notes - Project Overview v2

**Date:** 2025-11-12
**Status:** Active
**Supersedes:** PROJECT_OVERVIEW.md (deprecated journal integration approach)

---

## Project Goal

Create a minimal note-taking system for Skyrim Special Edition that allows players to:
1. Quickly add notes via hotkey
2. Attach notes to specific locations or contexts
3. View all notes via simple interface

**Core Philosophy:** Simplicity over features. Focus on text input and storage.

---

## Technical Constraints

- 100% C++ SKSE plugin (primary logic)
- Minimal Papyrus (only for text input bridge)
- **Dependency:** Extended Vanilla Menus (for text input)
- No ESP file required
- No custom SWF creation
- No journal integration

**Complexity Target:** 2/10 - Dead simple implementation

---

## Architecture Overview

```
┌─────────────────────────────────────┐
│        NoteManager (Singleton)      │
│  - Stores notes with metadata       │
│  - SKSE co-save persistence         │
│  - Thread-safe access               │
└─────────────────────────────────────┘
           ▲
           │
    ┌──────┴──────┬──────────────┐
    │             │              │
┌───▼────┐  ┌─────▼─────┐  ┌────▼────────┐
│Hotkey  │  │  Papyrus  │  │   Display   │
│Handler │  │  Bridge   │  │   System    │
│(C++)   │  │(C++↔Pap)  │  │(MessageBox) │
└────────┘  └───────────┘  └─────────────┘
                │
        ┌───────▼────────┐
        │Extended Vanilla│
        │    Menus       │
        │  (Text Input)  │
        └────────────────┘
```

---

## User Experience Flow

### Adding a Note
1. Press Insert key (configurable)
2. Text input dialog appears (Extended Vanilla Menus)
3. Type note (e.g., "Check back here for loot")
4. Press Accept
5. Note saved with timestamp + context

### Viewing Notes
1. Press hotkey combination (e.g., Shift+Insert)
2. Message box shows recent notes
3. Or use console command: `shownotes`

### Context Detection
- **If in cell:** Note tagged with cell name
- **If in menu:** General note
- **If quest active:** Note tagged with quest (optional)

---

## Implementation Phases

### Phase 1: Core System
**Duration:** 3-5 days
**Goal:** Working note input and persistence

**Deliverables:**
- SKSE plugin loads
- NoteManager with co-save serialization
- Hotkey detection
- Papyrus bridge to Extended Vanilla Menus
- Notes persist across saves

**Validation:** Can add note, save, load, note still exists

---

### Phase 2: Display & Polish
**Duration:** 2-3 days
**Goal:** View notes and final touches

**Deliverables:**
- Display recent notes via message box
- Console command to list all notes
- Context tagging (cell name, timestamp)
- Configuration via INI

**Validation:** Can view all saved notes with details

---

## Total Estimated Timeline

**7-8 days of development**

Assumes:
- Experienced C++ developer
- Familiar with SKSE/CommonLibSSE-NG basics
- Extended Vanilla Menus installed

---

## Technical Requirements

### Development Dependencies
- Visual Studio 2022
- CMake 3.21+
- vcpkg
- SKSE64
- CommonLibSSE-NG

### Runtime Dependencies
- SKSE64
- Skyrim SE 1.6.640+ (AE compatible)
- **Extended Vanilla Menus** (Nexus mod #67946)

### Optional
- SkyUI (not required)

---

## Success Criteria

**Project COMPLETE when:**

1. ✅ Press hotkey → text input dialog appears
2. ✅ Type note → note saved
3. ✅ Notes persist across save/load
4. ✅ Can view all notes
5. ✅ No ESP required
6. ✅ No crashes
7. ✅ Configurable hotkey via INI

**That's it. Nothing more.**

---

## What We're NOT Doing

- ❌ Journal integration
- ❌ Virtual quest creation
- ❌ Quest-specific note display in journal
- ❌ Rich UI/menus
- ❌ Note editing/deletion (v1.0)
- ❌ Note categories/filtering (v1.0)
- ❌ Export functionality (v1.0)

**Future versions can add these if needed.**

---

## Key Design Decisions

### Why No Journal Integration?

**Original Problem:** Skyrim's journal system is complex:
- Quest descriptions are read-only BSFixedString
- Requires deep Scaleform hooking
- Fragile across UI mods (vanilla vs SkyUI)
- High complexity (7-12 days of work)

**Solution:** Skip it entirely. Use simple display methods.

### Why Extended Vanilla Menus?

**Problem:** No native text input in Skyrim C++ API

**Alternatives Considered:**
- Custom Scaleform SWF → Too complex
- Console hijacking → Poor UX
- Book menu hijacking → Hacky

**Solution:** Extended Vanilla Menus
- Battle-tested Nexus mod
- Simple Papyrus API
- Zero SWF work
- Clean C++↔Papyrus bridge

### Why Minimal Features?

**Philosophy:** Ship a working v1.0 fast. Iterate later if needed.

---

## Data Structure

### Note Format

```cpp
struct Note {
    std::string text;           // Note content
    std::time_t timestamp;      // When created
    std::string context;        // Cell name, "General", etc.
    RE::FormID questID;         // Optional quest association
};
```

### Storage

- **General notes:** Vector in NoteManager
- **Co-save:** SKSE serialization with record type 'NOTE'
- **Version:** 1 (for future compatibility)

---

## Configuration (INI)

**File:** `Data/SKSE/Plugins/PersonalNotes.ini`

```ini
[General]
AddNoteHotkey = 82        ; Insert key (scan code)
ViewNotesHotkey = 210     ; Shift+Insert
MaxNotes = 100            ; Prevent spam
EnableContextTagging = 1  ; Auto-tag with cell/quest
```

---

## Display Strategy

### Option A: Message Box (Simple)
- Show last 10 notes in vanilla message box
- Scrollable if needed
- No dependencies

### Option B: Console Command (Fallback)
```
~ (open console)
shownotes
```
Prints all notes to console

### Option C: Extended Vanilla Menus List
- Use EVM's ListMenu to show all notes
- Clickable, formatted nicely
- Consistent with text input UI

**Recommended:** Start with A, add C later

---

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Extended Vanilla Menus not installed | High | Check at runtime, show error |
| Papyrus bridge complexity | Medium | Keep minimal, test thoroughly |
| Hotkey conflicts | Low | Make configurable |
| Too many notes (spam) | Low | Add max limit (100 notes) |

---

## Documentation Plan

- **PROJECT_OVERVIEW_V2.md** (this file)
- **Phase1_PRD_V2.md** - Core system implementation
- **Phase2_PRD_V2.md** - Display and polish

**Old Plans:** Moved to `plans/deprecated/`

---

## Getting Started

1. Read this overview
2. Implement Phase 1 (core system)
3. Test: add note → save → load → note persists
4. Implement Phase 2 (display)
5. Test: view all notes
6. Ship v1.0

**No planning paralysis. Ship fast, iterate later.**

---

## Future Enhancements (v2.0+)

*Only if v1.0 succeeds and users request:*

- Note editing/deletion
- Categories/tags
- Search/filter
- Export to text file
- Journal integration (if really needed)
- MCM configuration
- Note sharing between characters

---

**Project approved:** 2025-11-12
**Estimated completion:** 7-8 days
**Complexity:** 2/10 (simple)
