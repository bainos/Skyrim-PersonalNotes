# Phase 4: UX Notifications - Product Requirements Document

**Status**: Planning
**Created**: 2025-11-13
**Phase**: 4 - User Experience Enhancements

---

## Overview

Add user-friendly notifications to guide users on quest note functionality and indicate when selected quests have notes, without creating notification spam.

---

## Requirements

### Functional Requirements

**FR4.1: Journal Open Notification**
- When Journal Menu opens, show hint: "Quest Notes: Press , to add/edit notes"
- Notification uses standard DebugNotification API (3-second fade)
- Shows every time Journal opens (non-intrusive tutorial)

**FR4.2: Note Presence Indicator**
- When user hovers on quest with note for 2+ seconds, show: "Personal note present"
- Notification duration: ~1 second
- Debounce prevents spam during fast scrolling

**FR4.3: Debounce Logic**
- 2-second delay after quest selection change before showing note indicator
- Scrolling to different quest resets timer
- Scrolling back to same quest resets timer (can show notification again)

**FR4.4: No Session Tracking**
- Note indicator can show multiple times for same quest
- No "show once per session" logic (simpler, more responsive)
- Only prevents showing multiple times while quest stays selected

### Non-Functional Requirements

**NFR4.1: Performance**
- Update checks run every frame while Journal open (~60 FPS)
- Minimal CPU overhead (<0.1ms per frame)
- No memory leaks from timer tracking

**NFR4.2: Compatibility**
- Uses standard SKSE notification API (no mod conflicts)
- No modifications to Journal SWF (maintains compatibility)
- Works with vanilla and modded quests

**NFR4.3: User Experience**
- Tutorial hint non-intrusive but always present
- Note indicator only shows when user pauses on quest
- No spam when scrolling quickly through quest list

---

## Design

### Component Architecture

```
JournalNoteHelper (Singleton)
├── State Tracking
│   ├── currentQuestID (FormID)
│   ├── selectionTime (time_point)
│   ├── journalOpen (bool)
│   └── notificationShown (bool)
├── Event Handlers
│   ├── OnJournalOpen()
│   ├── OnJournalClose()
│   └── Update() [called every frame]
└── Integration
    ├── GetCurrentQuestInJournal()
    └── NoteManager::HasNoteForQuest()
```

### State Machine

```
[Game Start]
    ↓
[journalOpen = false]
    ↓
[Journal Opens] → Show "Quest Notes: Press , to add/edit notes"
    ↓
[journalOpen = true, currentQuestID = 0]
    ↓
[Quest Selected] → Start 2-second timer
    ↓
[Same Quest, 2 sec elapsed, has note] → Show "Personal note present"
    ↓                                     (notificationShown = true)
[Different Quest Selected] → Reset timer, notificationShown = false
    ↓
[Back to Previous Quest] → Restart 2-second timer
    ↓
[Journal Closes] → Reset all state
```

### Update Loop Integration

**Option A: Frame Update (Recommended)**
```cpp
// In MessageHandler, hook AdvanceMovie or similar frame event
void OnFrame() {
    JournalNoteHelper::GetSingleton()->Update();
}
```

**Option B: Polling in InputHandler**
```cpp
// In InputHandler::ProcessEvent, check every input event
RE::BSEventNotifyControl ProcessEvent(...) {
    JournalNoteHelper::GetSingleton()->Update();
    // ... existing code
}
```

**Decision: Option A** - More accurate timing, decoupled from input events.

### Timer Implementation

```cpp
#include <chrono>

std::chrono::steady_clock::time_point selectionTime;

// On quest change
selectionTime = std::chrono::steady_clock::now();

// Check elapsed time
auto now = std::chrono::steady_clock::now();
auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    now - selectionTime
);

if (elapsed.count() >= 2000) {
    // Show notification
}
```

---

## Tasks

### Task 4.1: Create JournalNoteHelper Class

**Description**: Implement singleton helper class for notification management.

**Implementation**:
```cpp
class JournalNoteHelper {
public:
    static JournalNoteHelper* GetSingleton() {
        static JournalNoteHelper instance;
        return &instance;
    }

    void Update();
    void OnJournalOpen();
    void OnJournalClose();

private:
    JournalNoteHelper() = default;

    RE::FormID currentQuestID = 0;
    std::chrono::steady_clock::time_point selectionTime;
    bool journalOpen = false;
    bool notificationShown = false;
};
```

**Files Modified**:
- `plugin.cpp` (add class before InputHandler)

**Acceptance Criteria**:
- [ ] Class compiles without errors
- [ ] Singleton pattern implemented correctly
- [ ] All member variables initialized

---

### Task 4.2: Implement OnJournalOpen()

**Description**: Show tutorial notification and reset state when Journal opens.

**Implementation**:
```cpp
void JournalNoteHelper::OnJournalOpen() {
    currentQuestID = 0;
    journalOpen = true;
    notificationShown = false;

    RE::DebugNotification("Quest Notes: Press , to add/edit notes");
    spdlog::info("[HELPER] Journal opened - tutorial shown");
}
```

**Acceptance Criteria**:
- [ ] Notification shows immediately on Journal open
- [ ] State variables reset correctly
- [ ] Logs journal open event

---

### Task 4.3: Implement OnJournalClose()

**Description**: Reset all state when Journal closes.

**Implementation**:
```cpp
void JournalNoteHelper::OnJournalClose() {
    journalOpen = false;
    currentQuestID = 0;
    notificationShown = false;

    spdlog::info("[HELPER] Journal closed - state reset");
}
```

**Acceptance Criteria**:
- [ ] All state cleared on close
- [ ] No lingering notifications
- [ ] Logs journal close event

---

### Task 4.4: Implement Update() Logic

**Description**: Core update loop with quest change detection and timer logic.

**Implementation**:
```cpp
void JournalNoteHelper::Update() {
    if (!journalOpen) return;

    RE::FormID questID = GetCurrentQuestInJournal();

    // Quest changed?
    if (questID != currentQuestID) {
        currentQuestID = questID;
        selectionTime = std::chrono::steady_clock::now();
        notificationShown = false;
        spdlog::debug("[HELPER] Quest changed to 0x{:X}", questID);
        return;
    }

    // No quest selected
    if (questID == 0) return;

    // Already shown for current selection?
    if (notificationShown) return;

    // Check if 2 seconds passed
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - selectionTime
    );

    if (elapsed.count() >= 2000) {
        // Check if quest has note
        if (NoteManager::GetSingleton()->HasNoteForQuest(questID)) {
            RE::DebugNotification("Personal note present");
            notificationShown = true;
            spdlog::info("[HELPER] Note indicator shown for quest 0x{:X}", questID);
        } else {
            // No note, but mark as checked to avoid re-checking
            notificationShown = true;
        }
    }
}
```

**Acceptance Criteria**:
- [ ] Quest change resets timer
- [ ] 2-second delay works correctly
- [ ] Note presence checked only after delay
- [ ] Notification shows for quests with notes
- [ ] No notification for quests without notes
- [ ] No duplicate notifications for same selection

---

### Task 4.5: Hook Journal Open/Close Detection

**Description**: Detect when Journal Menu opens and closes.

**Implementation**:
```cpp
// Add to existing MessageHandler or create frame update hook
void MessageHandler(SKSE::MessagingInterface::Message* msg) {
    switch (msg->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        // ... existing code ...

        // Register frame update for helper
        auto messageQueue = SKSE::GetTaskInterface();
        messageQueue->AddTask([]() {
            // Frame update loop
            static bool wasJournalOpen = false;
            auto ui = RE::UI::GetSingleton();
            bool isJournalOpen = ui && ui->IsMenuOpen("Journal Menu");

            if (isJournalOpen && !wasJournalOpen) {
                JournalNoteHelper::GetSingleton()->OnJournalOpen();
            } else if (!isJournalOpen && wasJournalOpen) {
                JournalNoteHelper::GetSingleton()->OnJournalClose();
            }

            if (isJournalOpen) {
                JournalNoteHelper::GetSingleton()->Update();
            }

            wasJournalOpen = isJournalOpen;
        });
        break;
    }
}
```

**Alternative**: Use SKSE task system with periodic updates.

**Acceptance Criteria**:
- [ ] Journal open detected correctly
- [ ] Journal close detected correctly
- [ ] Update() called every frame while Journal open
- [ ] Minimal performance impact (<0.1ms per frame)

---

### Task 4.6: Add Required Includes

**Description**: Add chrono header for timer functionality.

**Implementation**:
```cpp
#include <chrono>
```

**Files Modified**:
- `plugin.cpp`

**Acceptance Criteria**:
- [ ] Compiles without errors
- [ ] chrono types available

---

### Task 4.7: Testing

**Description**: Comprehensive testing of all notification scenarios.

**Test Cases**:

1. **Tutorial Notification**
   - Open Journal → Tutorial shows
   - Close and reopen → Tutorial shows again

2. **Note Indicator - Basic**
   - Select quest with note → Wait 2 sec → "Personal note present" shows
   - Select quest without note → Wait 2 sec → No notification

3. **Quest Change Behavior**
   - Select quest A (has note) → Wait 1 sec → Switch to quest B
   - Result: No notification for quest A (didn't reach 2 sec)

4. **Scroll Back Behavior**
   - Select quest A (has note) → Wait 2 sec → Notification shows
   - Switch to quest B → Switch back to quest A → Wait 2 sec
   - Result: Notification shows again ✓

5. **Fast Scrolling**
   - Rapidly scroll through 10 quests with notes
   - Result: No notifications (debounce prevents spam)

6. **Edge Cases**
   - Journal open with no quests → No crashes
   - Create note during Journal session → Notification appears if hovering 2+ sec
   - Delete note during Journal session → Notification stops appearing

**Acceptance Criteria**:
- [ ] All test cases pass
- [ ] No notification spam observed
- [ ] No crashes or errors
- [ ] Logs show correct state transitions

---

## Acceptance Criteria (Phase Complete)

### Functional
- [ ] Tutorial notification shows every time Journal opens
- [ ] Note indicator shows after 2-second hover on noted quests
- [ ] Debounce prevents spam during fast scrolling
- [ ] Can see notification multiple times for same quest (after re-selection)
- [ ] No crashes or errors in any scenario

### Technical
- [ ] Frame update runs at ~60 FPS with <0.1ms overhead
- [ ] No memory leaks from timer tracking
- [ ] Clean state management (no lingering data)
- [ ] Proper logging for debugging

### User Experience
- [ ] Tutorial is clear and non-intrusive
- [ ] Note indicator provides helpful feedback
- [ ] No spam or annoyance from over-notification
- [ ] Works seamlessly with existing note-taking flow

---

## Known Limitations

1. **Frame Update Hook**: Requires SKSE task system or message hook - needs research on best approach
2. **Notification Timing**: DebugNotification duration is fixed by game (~3 sec), can't customize
3. **Multiple Notes**: If user has many noted quests, could see many notifications in one session (acceptable - user chose to add notes)

---

## Future Enhancements (Post-Phase 4)

- Customizable notification text via config file
- Adjustable debounce delay (default 2 sec)
- Toggle notifications on/off via MCM menu
- Different notification styles (corner message vs center)

---

## References

**Related Files**:
- `/mnt/c/dev/Skyrim-Modding/Skyrim-PersonalNotes/plugin.cpp`
- `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/U/UI.h`

**Dependencies**:
- `<chrono>` - C++11 time utilities
- SKSE messaging/task system for frame updates

**Prior Art**:
- Phase 3: Journal integration and quest detection
- Existing notification usage: RE::DebugNotification()

---

**Document Version**: 1.0
**Last Updated**: 2025-11-13
