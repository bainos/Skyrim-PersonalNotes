# Phase 4B: Simple HUD Overlay - Product Requirements Document

**Status**: Planning
**Created**: 2025-11-13
**Phase**: 4B - User Experience (HUD Alternative)

---

## Overview

Add simple HUD text overlay to Journal Menu showing:
1. Persistent hint: "Quest Notes: Press , to add/edit"
2. Quest note indicator: "[Quest Name] - Note present" (appears after 0.5sec hover)

**Approach**: Custom IMenu with programmatically-created text fields (no custom SWF).

---

## Requirements

### Functional Requirements

**FR4B.1: Persistent Tutorial Text**
- Text shows in top-right corner when Journal Menu opens
- Content: "Quest Notes: Press , to add/edit"
- Always visible while Journal open
- No fade-out, no timeout

**FR4B.2: Note Indicator Text**
- Shows below tutorial text after 0.5sec hover on noted quest
- Format: "[Quest Name] - Note present"
- Updates when quest selection changes
- Hides when hovering on quest without note

**FR4B.3: Lifecycle**
- HUD menu created when Journal opens
- Destroyed when Journal closes
- Clean state management (no leaks)

### Non-Functional Requirements

**NFR4B.1: No Custom SWF**
- Use Scaleform GFxValue API to create text fields programmatically
- No .swf file modifications or additions
- Use game's built-in MovieClip/TextField system

**NFR4B.2: Simple Styling**
- White text, subtle drop shadow
- Font: $EverywhereMediumFont (vanilla Skyrim HUD font)
- Size: Medium (~20-24px)
- Position: Top-right, safe zone margins

**NFR4B.3: Performance**
- Text updates only on quest change (not every frame)
- Minimal GFxValue operations
- No impact on Journal Menu responsiveness

---

## Design

### Architecture

```
QuestNoteHUDMenu (IMenu)
├── movie (GFxMovieView*)
├── tutorialText (GFxValue) - "Quest Notes: Press , to add/edit"
├── noteIndicatorText (GFxValue) - "[QuestName] - Note present"
├── UpdateNoteIndicator(questID, questName)
└── Lifecycle: Show on Journal open, hide on Journal close
```

### HUD Menu Structure

**Menu Properties**:
- Name: `"QuestNoteHUDMenu"`
- Type: `kCustom`
- Context: `kJournal` (only active during Journal)
- Flags: `kAllowSaving = false`, `kDontHideCursorWhenTopmost = true`
- Depth priority: Above Journal but below cursor

**Text Field Creation (Programmatic)**:
```cpp
// Create MovieClip container
GFxValue movieClip;
movie->CreateObject(&movieClip);

// Create TextField
GFxValue textField;
movie->CreateObject(&textField, "flash.text.TextField");

// Set properties
textField.SetMember("text", "Quest Notes: Press , to add/edit");
textField.SetMember("textColor", 0xFFFFFF); // White
textField.SetMember("x", 50.0);
textField.SetMember("y", 50.0);
textField.SetMember("width", 400.0);
textField.SetMember("height", 50.0);

// Add to stage
movie->GetRootObject().Invoke("addChild", nullptr, &textField, 1);
```

### Integration with JournalNoteHelper

**Modified Flow**:
```
[Journal Opens]
  ↓
[JournalNoteHelper::OnJournalOpen()]
  ↓
[Show QuestNoteHUDMenu] → Tutorial text visible
  ↓
[Quest Selected + 0.5sec elapsed]
  ↓
[JournalNoteHelper::Update() detects note]
  ↓
[Call QuestNoteHUDMenu::UpdateNoteIndicator(questID, questName)]
  ↓
[Note indicator text updates: "[QuestName] - Note present"]
  ↓
[Journal Closes]
  ↓
[Hide QuestNoteHUDMenu]
```

---

## Tasks

### Task 4B.1: Create QuestNoteHUDMenu Class

**Description**: Implement custom IMenu subclass for HUD overlay.

**Implementation**:
```cpp
class QuestNoteHUDMenu : public RE::IMenu {
public:
    static constexpr const char* MENU_NAME = "QuestNoteHUDMenu";

    QuestNoteHUDMenu();
    ~QuestNoteHUDMenu() override = default;

    static void Register();
    static void Show();
    static void Hide();
    static QuestNoteHUDMenu* GetInstance();

    void UpdateNoteIndicator(RE::FormID questID, const std::string& questName);
    void ClearNoteIndicator();

private:
    RE::GFxValue tutorialText_;
    RE::GFxValue noteIndicatorText_;

    void InitializeTextFields();
};
```

**Files Modified**:
- `plugin.cpp` (add class after JournalNoteHelper)

**Acceptance Criteria**:
- [ ] Class compiles
- [ ] Inherits from RE::IMenu correctly
- [ ] Static instance management implemented

---

### Task 4B.2: Implement Menu Registration

**Description**: Register custom menu with UI system.

**Implementation**:
```cpp
void QuestNoteHUDMenu::Register() {
    auto ui = RE::UI::GetSingleton();
    if (ui) {
        ui->Register(MENU_NAME, []() -> RE::IMenu* {
            return new QuestNoteHUDMenu();
        });
        spdlog::info("[HUD] QuestNoteHUDMenu registered");
    }
}

// Call from SKSEPlugin_Load or MessageHandler kDataLoaded
void InitializeHUD() {
    QuestNoteHUDMenu::Register();
}
```

**Files Modified**:
- `plugin.cpp` (add registration in MessageHandler)

**Acceptance Criteria**:
- [ ] Menu registered with UI system
- [ ] No crashes on registration
- [ ] Log confirms registration

---

### Task 4B.3: Implement Constructor & Text Field Creation

**Description**: Create text fields programmatically when menu instantiates.

**Implementation**:
```cpp
QuestNoteHUDMenu::QuestNoteHUDMenu() {
    menuFlags.set(RE::UI_MENU_FLAGS::kAllowSaving, false);
    menuFlags.set(RE::UI_MENU_FLAGS::kDontHideCursorWhenTopmost);

    auto scaleformManager = RE::BSScaleformManager::GetSingleton();
    if (!scaleformManager) {
        spdlog::error("[HUD] Failed to get ScaleformManager");
        return;
    }

    scaleformManager->LoadMovieEx(this, "HUDMenu",
        RE::BSScaleformManager::ScaleModeType::kExactFit);

    if (!uiMovie) {
        spdlog::error("[HUD] Failed to load movie");
        return;
    }

    InitializeTextFields();
}

void QuestNoteHUDMenu::InitializeTextFields() {
    auto movie = uiMovie;
    if (!movie) return;

    // Create tutorial text
    movie->CreateObject(&tutorialText_, "flash.text.TextField");
    tutorialText_.SetMember("text", "Quest Notes: Press , to add/edit");
    tutorialText_.SetMember("textColor", 0xFFFFFF);
    tutorialText_.SetMember("x", 1600.0); // Top-right (1920x1080)
    tutorialText_.SetMember("y", 50.0);
    tutorialText_.SetMember("width", 300.0);
    tutorialText_.SetMember("height", 40.0);

    RE::GFxValue root;
    movie->GetVariable(&root, "_root");
    root.Invoke("addChild", nullptr, &tutorialText_, 1);

    // Create note indicator text (initially empty)
    movie->CreateObject(&noteIndicatorText_, "flash.text.TextField");
    noteIndicatorText_.SetMember("text", "");
    noteIndicatorText_.SetMember("textColor", 0xFFFF00); // Yellow
    noteIndicatorText_.SetMember("x", 1600.0);
    noteIndicatorText_.SetMember("y", 90.0);
    noteIndicatorText_.SetMember("width", 300.0);
    noteIndicatorText_.SetMember("height", 40.0);

    root.Invoke("addChild", nullptr, &noteIndicatorText_, 1);

    spdlog::info("[HUD] Text fields initialized");
}
```

**Acceptance Criteria**:
- [ ] Text fields created successfully
- [ ] Tutorial text visible on menu show
- [ ] No crashes during initialization

---

### Task 4B.4: Implement Show/Hide Methods

**Description**: Show menu when Journal opens, hide when closes.

**Implementation**:
```cpp
void QuestNoteHUDMenu::Show() {
    auto ui = RE::UI::GetSingleton();
    if (ui && !ui->IsMenuOpen(MENU_NAME)) {
        ui->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
        spdlog::info("[HUD] Menu shown");
    }
}

void QuestNoteHUDMenu::Hide() {
    auto ui = RE::UI::GetSingleton();
    if (ui && ui->IsMenuOpen(MENU_NAME)) {
        ui->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
        spdlog::info("[HUD] Menu hidden");
    }
}

QuestNoteHUDMenu* QuestNoteHUDMenu::GetInstance() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) return nullptr;

    auto menu = ui->GetMenu<QuestNoteHUDMenu>(MENU_NAME);
    return menu ? menu.get() : nullptr;
}
```

**Files Modified**:
- `plugin.cpp`

**Acceptance Criteria**:
- [ ] Menu shows on demand
- [ ] Menu hides cleanly
- [ ] GetInstance returns valid pointer when shown

---

### Task 4B.5: Implement UpdateNoteIndicator()

**Description**: Update note indicator text when quest with note is hovered.

**Implementation**:
```cpp
void QuestNoteHUDMenu::UpdateNoteIndicator(RE::FormID questID, const std::string& questName) {
    if (noteIndicatorText_.IsUndefined()) return;

    std::string text = "[" + questName + "] - Note present";
    noteIndicatorText_.SetMember("text", text.c_str());

    spdlog::info("[HUD] Note indicator updated: {}", text);
}

void QuestNoteHUDMenu::ClearNoteIndicator() {
    if (noteIndicatorText_.IsUndefined()) return;

    noteIndicatorText_.SetMember("text", "");
    spdlog::debug("[HUD] Note indicator cleared");
}
```

**Acceptance Criteria**:
- [ ] Text updates correctly
- [ ] Quest name displays properly
- [ ] Clear works as expected

---

### Task 4B.6: Integrate with JournalNoteHelper

**Description**: Call HUD menu from helper's journal lifecycle events.

**Implementation**:
```cpp
void JournalNoteHelper::OnJournalOpen() {
    currentQuestID = 0;
    journalOpen = true;
    notificationShown = false;

    QuestNoteHUDMenu::Show();
    spdlog::info("[HELPER] Journal opened, HUD shown");
}

void JournalNoteHelper::OnJournalClose() {
    journalOpen = false;
    currentQuestID = 0;
    notificationShown = false;

    QuestNoteHUDMenu::Hide();
    spdlog::info("[HELPER] Journal closed, HUD hidden");
}

void JournalNoteHelper::Update() {
    // ... existing logic ...

    if (elapsed.count() >= 500) {
        if (NoteManager::GetSingleton()->HasNoteForQuest(questID)) {
            notificationShown = true;

            // Get quest name and update HUD
            auto quest = RE::TESForm::LookupByID<RE::TESQuest>(questID);
            std::string questName = quest ? quest->GetName() : "Unknown Quest";

            auto hudMenu = QuestNoteHUDMenu::GetInstance();
            if (hudMenu) {
                hudMenu->UpdateNoteIndicator(questID, questName);
            }

            spdlog::info("[HELPER] Quest 0x{:X} has note", questID);
        } else {
            notificationShown = true;

            // Clear indicator for quests without notes
            auto hudMenu = QuestNoteHUDMenu::GetInstance();
            if (hudMenu) {
                hudMenu->ClearNoteIndicator();
            }
        }
    }
}
```

**Files Modified**:
- `plugin.cpp` (JournalNoteHelper methods)

**Acceptance Criteria**:
- [ ] HUD shows on journal open
- [ ] HUD hides on journal close
- [ ] Note indicator updates on quest hover
- [ ] Indicator clears for quests without notes

---

### Task 4B.7: Testing

**Description**: Comprehensive testing of HUD functionality.

**Test Cases**:

1. **HUD Visibility**
   - Open Journal → Tutorial text appears top-right
   - Close Journal → HUD disappears
   - Reopen → HUD reappears

2. **Note Indicator**
   - Hover 0.5sec on quest with note → Indicator shows "[QuestName] - Note present"
   - Switch to quest without note → Indicator clears
   - Switch back to noted quest → Indicator reappears after 0.5sec

3. **Text Positioning**
   - Tutorial text top-right, readable
   - Note indicator below tutorial, no overlap
   - No clipping on 1920x1080

4. **Edge Cases**
   - Journal open with no quests → No crashes
   - Fast scrolling → Indicator updates correctly
   - Create note mid-session → Indicator appears

**Acceptance Criteria**:
- [ ] All test cases pass
- [ ] Text readable and positioned correctly
- [ ] No crashes or visual glitches
- [ ] Performance acceptable

---

## Acceptance Criteria (Phase Complete)

### Functional
- [ ] Tutorial text shows every time Journal opens
- [ ] Note indicator shows after 0.5sec hover on noted quests
- [ ] Indicator clears when hovering quest without note
- [ ] HUD lifecycle matches Journal open/close

### Technical
- [ ] No custom SWF files required
- [ ] Menu registered and managed correctly
- [ ] GFxValue text fields work properly
- [ ] Clean memory management (no leaks)

### User Experience
- [ ] Text readable and well-positioned
- [ ] Non-intrusive, doesn't block Journal UI
- [ ] Updates feel responsive (0.5sec acceptable)

---

## Known Risks

1. **GFxValue TextField Creation**: May need `gfx.text.TextField` path instead of `flash.text.TextField` (test both)
2. **Movie Loading**: Loading "HUDMenu" SWF might not work; may need to create blank movie or use different base
3. **Text Formatting**: Flash TextField styling limited; may need TextFormat object for advanced styling
4. **Positioning**: Coordinates may need adjustment for different resolutions

---

## Fallback Plan

If programmatic TextField creation fails:
1. Use RE::DebugMessageBox for tutorial (one-time popup)
2. Document comma key in mod description
3. Keep helper logging for debugging

---

## References

**Related Files**:
- `/mnt/c/dev/Skyrim-Modding/Skyrim-PersonalNotes/plugin.cpp`
- `/mnt/c/dev/CommonLibSSE-NG/include/RE/I/IMenu.h`
- `/mnt/c/dev/CommonLibSSE-NG/include/RE/G/GFxValue.h`

**Dependencies**:
- Scaleform GFx API (built into game)
- RE::UI menu system
- JournalNoteHelper (existing)

---

**Document Version**: 1.0
**Last Updated**: 2025-11-13
