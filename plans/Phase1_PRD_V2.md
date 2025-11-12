# Phase 1: Core Note System - PRD v2

**Duration:** 3-5 days
**Complexity:** 2/10

---

## Overview

Implement the complete note input and persistence system. Users can press a hotkey, type a note in a text dialog, and have it saved permanently.

---

## Requirements

### Functional Requirements

- **FR1.1:** Detect Insert key press in-game
- **FR1.2:** Open text input dialog when hotkey pressed
- **FR1.3:** Capture user text input
- **FR1.4:** Save note with timestamp
- **FR1.5:** Persist notes across save/load using SKSE co-save
- **FR1.6:** Auto-tag notes with context (cell name)

### Technical Requirements

- **TR1.1:** SKSE64 plugin using CommonLibSSE-NG
- **TR1.2:** C++ to Papyrus bridge via IVirtualMachine
- **TR1.3:** Papyrus script calls Extended Vanilla Menus
- **TR1.4:** Thread-safe note storage
- **TR1.5:** SKSE serialization compatible
- **TR1.6:** Verify Extended Vanilla Menus installed at runtime

### Non-Requirements

- No display functionality (Phase 2)
- No note editing/deletion
- No configuration UI
- No quest-specific tagging (basic context only)

---

## Design

### Component Architecture

```
┌──────────────────────────────────────────────────┐
│              C++ Plugin (SKSE)                   │
│                                                  │
│  ┌─────────────┐      ┌──────────────┐         │
│  │ InputHandler│◄─────┤ NoteManager  │         │
│  │  (Hotkey)   │      │  (Singleton) │         │
│  └──────┬──────┘      └──────┬───────┘         │
│         │                    │                  │
│         │              ┌─────▼────────┐         │
│         └─────────────►│PapyrusBridge │         │
│                        └──────┬───────┘         │
└───────────────────────────────┼──────────────────┘
                                │
                        ┌───────▼────────┐
                        │ Papyrus Script │
                        │ PersonalNotes  │
                        └───────┬────────┘
                                │
                   ┌────────────▼─────────────┐
                   │ Extended Vanilla Menus   │
                   │   TextInput()            │
                   └──────────────────────────┘
```

### Data Structures

#### Note (C++)

```cpp
struct Note {
    std::string text;           // Max 500 characters
    std::time_t timestamp;      // Unix timestamp
    std::string context;        // "General", cell name, etc.

    // Serialization
    void Save(SKSE::SerializationInterface* a_intfc) const;
    bool Load(SKSE::SerializationInterface* a_intfc);
};
```

#### NoteManager (C++)

```cpp
class NoteManager {
public:
    static NoteManager* GetSingleton();

    // Core operations
    void AddNote(const std::string& text, const std::string& context);
    std::vector<Note> GetAllNotes() const;
    size_t GetNoteCount() const;

    // Persistence
    void Save(SKSE::SerializationInterface* a_intfc);
    void Load(SKSE::SerializationInterface* a_intfc);
    void Revert(SKSE::SerializationInterface* a_intfc);

private:
    std::vector<Note> notes_;
    mutable std::shared_mutex lock_;
    static constexpr size_t MAX_NOTES = 100;
};
```

#### InputHandler (C++)

```cpp
class InputHandler : public RE::BSTEventSink<RE::InputEvent*> {
public:
    static InputHandler* GetSingleton();
    static void Register();

    RE::BSEventNotifyControl ProcessEvent(
        RE::InputEvent* const* a_event,
        RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;

private:
    void OnHotkeyPressed();
    bool IsAddNoteHotkey(RE::ButtonEvent* a_event);

    uint32_t hotkeyAddNote_ = 0x52; // Insert key
};
```

#### PapyrusBridge (C++)

```cpp
class PapyrusBridge {
public:
    static void Register();

    // Called from Papyrus: receives note text
    static void OnNoteReceived(RE::StaticFunctionTag*, RE::BSFixedString text);

    // Called from C++: triggers Papyrus text input
    static void RequestTextInput();

private:
    static bool VerifyExtendedVanillaMenus();
};
```

#### PersonalNotes.psc (Papyrus)

```papyrus
Scriptname PersonalNotes Hidden

; Called from C++ when hotkey pressed
Function RequestInput() Global
    ; Get current context
    String context = GetCurrentContext()

    ; Show text input
    RegisterForModEvent("EVM_TextInputClosed", "OnTextInputClosed")
    String result = ExtendedVanillaMenus.TextInput("Enter Note:", "")
EndFunction

Event OnTextInputClosed(string eventName, string noteText, float accepted, form sender)
    UnregisterForModEvent("EVM_TextInputClosed")

    If accepted == 1 && noteText != ""
        ; Send back to C++
        PersonalNotesNative.OnNoteReceived(noteText)
    EndIf
EndEvent

String Function GetCurrentContext() Global
    ; Detect current game state
    If Game.GetPlayer().GetParentCell()
        Return Game.GetPlayer().GetParentCell().GetName()
    EndIf
    Return "General"
EndFunction
```

### Flow Diagram

```
User presses Insert
    ↓
InputHandler::ProcessEvent (C++)
    ↓
InputHandler::OnHotkeyPressed()
    ↓
PapyrusBridge::RequestTextInput()
    ↓
Call Papyrus: PersonalNotes.RequestInput()
    ↓
Papyrus calls ExtendedVanillaMenus.TextInput()
    ↓
Text input dialog appears
    ↓
User types note text
    ↓
User presses Accept
    ↓
Papyrus receives EVM_TextInputClosed event
    ↓
Papyrus calls C++: PersonalNotesNative.OnNoteReceived(text)
    ↓
C++: PapyrusBridge::OnNoteReceived()
    ↓
NoteManager::AddNote(text, context)
    ↓
Note stored in memory
    ↓
User saves game
    ↓
NoteManager::Save() → SKSE co-save
```

### Serialization Format

**Record Type:** `'NOTE'` (0x4E4F5445)
**Version:** 1

```
[Version:uint32_t]
[NoteCount:uint32_t]
[Note1:text,timestamp,context]
[Note2:text,timestamp,context]
...
```

**Per-Note Format:**
```
[TextLength:uint32_t]
[Text:char[]]
[Timestamp:uint64_t]
[ContextLength:uint32_t]
[Context:char[]]
```

---

## Tasks

### Task 1.1: Project Setup
- Copy existing SKSE project structure from ENBNighteyeFix
- Update CMakeLists.txt for PersonalNotes
- Update vcpkg.json dependencies
- Configure plugin metadata (name, version, etc.)
- Verify compilation produces DLL

**Time:** 1-2 hours

### Task 1.2: Implement NoteManager
- Create Note struct with serialization methods
- Implement NoteManager singleton
- Add thread-safe AddNote() method
- Add GetAllNotes() and GetNoteCount()
- Add MAX_NOTES limit enforcement
- Add comprehensive logging

**Time:** 3-4 hours

### Task 1.3: Implement Serialization
- Register serialization callbacks in SKSEPluginLoad
- Implement NoteManager::Save()
  - Write version, count, then each note
- Implement NoteManager::Load()
  - Read version, validate, load notes
- Implement NoteManager::Revert()
  - Clear notes on new game
- Test with manual note additions

**Time:** 4-6 hours

### Task 1.4: Implement Input Handler
- Create InputHandler class extending BSTEventSink
- Implement ProcessEvent to filter button events
- Detect Insert key press (scan code 0x52)
- Ignore if in menu or console active
- Register handler on SKSE kDataLoaded message
- Add logging for hotkey detection

**Time:** 3-4 hours

### Task 1.5: Create Papyrus Scripts
- Write PersonalNotes.psc
  - RequestInput() global function
  - OnTextInputClosed() event handler
  - GetCurrentContext() helper
- Compile with Creation Kit compiler
- Package PSC/PEX files

**Time:** 2-3 hours

### Task 1.6: Implement Papyrus Bridge
- Create PapyrusBridge class
- Implement Register() to register native functions
- Implement OnNoteReceived() - called from Papyrus
- Implement RequestTextInput() - calls Papyrus
- Use RE::BSScript::IVirtualMachine to invoke Papyrus
- Handle errors gracefully (no crash if EVM missing)

**Time:** 6-8 hours

### Task 1.7: Integration and Testing
- Connect InputHandler → PapyrusBridge → Papyrus
- Test full flow: hotkey → dialog → save note
- Test Extended Vanilla Menus detection
- Test without EVM installed (error handling)
- Test save/load persistence
- Test new game (notes cleared)
- Test max notes limit

**Time:** 4-6 hours

### Task 1.8: Configuration System
- Create PersonalNotes.ini reader
- Use RE::IniSettingCollection or custom parser
- Load hotkey from INI (default: Insert)
- Load MaxNotes from INI (default: 100)
- Load EnableContextTagging from INI

**Time:** 2-3 hours

---

## Acceptance Criteria

### AC1.1: Plugin Loads Successfully
- [ ] DLL appears in `Data/SKSE/Plugins/`
- [ ] Plugin logged in SKSE logs on game start
- [ ] No errors during initialization
- [ ] Extended Vanilla Menus detected (if installed)

### AC1.2: Hotkey Detection Works
- [ ] Press Insert key in game
- [ ] Hotkey detected (verify via log)
- [ ] Does not trigger in console
- [ ] Does not trigger in existing menus
- [ ] Does not trigger during dialogue

### AC1.3: Text Input Appears
- [ ] Press Insert key
- [ ] Extended Vanilla Menus text dialog appears
- [ ] Can type text (at least 500 characters)
- [ ] Can cancel (ESC)
- [ ] Can accept (Enter or button)

### AC1.4: Notes Are Saved
- [ ] Type "Test note 1" and accept
- [ ] Note logged as saved
- [ ] Type "Test note 2" and accept
- [ ] Note logged as saved
- [ ] GetNoteCount() returns 2

### AC1.5: Context Tagging Works
- [ ] Add note while in Whiterun
- [ ] Context = "Whiterun"
- [ ] Add note while in inventory menu
- [ ] Context = "General" or cell name
- [ ] Context appears in logs

### AC1.6: Persistence Works
- [ ] Add 3 notes
- [ ] Save game
- [ ] Exit to main menu
- [ ] Load save
- [ ] All 3 notes still present (verify via log)
- [ ] Timestamps preserved
- [ ] Context preserved

### AC1.7: New Game Clears Notes
- [ ] Add notes in one save
- [ ] Start new game
- [ ] Notes cleared (count = 0)
- [ ] Old notes don't carry over

### AC1.8: Max Notes Limit Works
- [ ] Add 100 notes (via rapid testing)
- [ ] Attempt to add 101st note
- [ ] Rejected or oldest note removed
- [ ] No crash
- [ ] Warning logged

### AC1.9: Error Handling
- [ ] Run without Extended Vanilla Menus
- [ ] Hotkey shows error notification
- [ ] Plugin doesn't crash
- [ ] Error logged clearly

### AC1.10: Thread Safety
- [ ] Rapidly press hotkey multiple times
- [ ] No race conditions
- [ ] No crashes
- [ ] All notes saved correctly

---

## Implementation Notes

### Extended Vanilla Menus Detection

```cpp
bool PapyrusBridge::VerifyExtendedVanillaMenus() {
    auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) return false;

    // Check if ExtendedVanillaMenus script exists
    RE::BSTSmartPointer<RE::BSScript::Object> obj;
    if (!vm->CreateObject("ExtendedVanillaMenus", obj)) {
        SKSE::log::error("Extended Vanilla Menus not found!");
        return false;
    }

    return true;
}
```

### Calling Papyrus from C++

```cpp
void PapyrusBridge::RequestTextInput() {
    auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    auto policy = vm->GetObjectHandlePolicy();

    // Call PersonalNotes.RequestInput()
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
    vm->DispatchStaticCall(
        "PersonalNotes",
        "RequestInput",
        RE::MakeFunctionArguments(),
        callback
    );
}
```

### INI Configuration

**File:** `Data/SKSE/Plugins/PersonalNotes.ini`

```ini
[General]
AddNoteHotkey = 82        ; Insert key (DirectX scan code)
MaxNotes = 100            ; Maximum stored notes
EnableContextTagging = 1  ; 1 = enabled, 0 = disabled

[Debug]
EnableLogging = 1         ; Verbose logging
```

### Logging Strategy

```cpp
SKSE::log::info("Hotkey pressed: Adding note");
SKSE::log::info("Note added: '{}' | Context: '{}'", text, context);
SKSE::log::info("Total notes: {}/{}", count, MAX_NOTES);
SKSE::log::error("Extended Vanilla Menus not installed!");
```

---

## Testing Checklist

**Before Phase 2:**

- [ ] Plugin compiles without errors
- [ ] Plugin loads in Skyrim
- [ ] Hotkey triggers text input
- [ ] Can type and submit note
- [ ] Note saved to memory
- [ ] Save game → Load game → notes persist
- [ ] New game clears notes
- [ ] Max notes limit enforced
- [ ] Extended Vanilla Menus missing = error shown
- [ ] No crashes in any scenario
- [ ] Thread-safe under rapid input
- [ ] Context tagging works (cell names)
- [ ] INI configuration loads correctly

---

## Dependencies

### Development
- CommonLibSSE-NG (via vcpkg)
- SKSE64 SDK
- Papyrus compiler (Creation Kit)

### Runtime
- SKSE64
- Extended Vanilla Menus (Nexus mod #67946)

---

## Deliverables

1. ✅ PersonalNotes.dll (SKSE plugin)
2. ✅ PersonalNotes.psc + .pex (Papyrus scripts)
3. ✅ PersonalNotes.ini (configuration)
4. ✅ Source code in repository
5. ✅ Build instructions in README

---

## Next Phase

**Phase 2:** Note display system (message box, console command, list UI)

---

**Phase 1 Status:** Ready to implement
**Estimated Time:** 3-5 days
**Complexity:** 2/10
