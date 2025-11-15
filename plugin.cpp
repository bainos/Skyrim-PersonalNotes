/**
 * PersonalNotes - SKSE Plugin for Skyrim SE/AE
 *
 * Simple in-game note-taking system using Extended Vanilla Menus for text input.
 * Notes persist across saves via SKSE co-save system.
 */

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <ctime>
#include <chrono>

//=============================================================================
// Data Structures
//=============================================================================

struct Note {
    std::string text;
    std::time_t timestamp;
    RE::FormID questID;

    Note() : timestamp(0), questID(0) {}
    Note(const std::string& t, RE::FormID qid)
        : text(t), timestamp(std::time(nullptr)), questID(qid) {}

    bool Save(SKSE::SerializationInterface* intfc) const {
        // Write quest ID
        if (!intfc->WriteRecordData(&questID, sizeof(questID))) {
            return false;
        }

        // Write text length and text
        std::uint32_t textLen = static_cast<std::uint32_t>(text.size());
        if (!intfc->WriteRecordData(&textLen, sizeof(textLen))) {
            return false;
        }
        if (textLen > 0 && !intfc->WriteRecordData(text.data(), textLen)) {
            return false;
        }

        // Write timestamp
        if (!intfc->WriteRecordData(&timestamp, sizeof(timestamp))) {
            return false;
        }

        return true;
    }

    bool Load(SKSE::SerializationInterface* intfc) {
        // Read quest ID
        if (!intfc->ReadRecordData(&questID, sizeof(questID))) {
            return false;
        }

        // Read text
        std::uint32_t textLen = 0;
        if (!intfc->ReadRecordData(&textLen, sizeof(textLen))) {
            return false;
        }
        if (textLen > 0) {
            text.resize(textLen);
            if (!intfc->ReadRecordData(text.data(), textLen)) {
                return false;
            }
        }

        // Read timestamp
        if (!intfc->ReadRecordData(&timestamp, sizeof(timestamp))) {
            return false;
        }

        return true;
    }
};

//=============================================================================
// Note Manager
//=============================================================================

class NoteManager {
public:
    static constexpr std::uint32_t kDataKey = 'PNOT';  // PersonalNOTes
    static constexpr std::uint32_t kSerializationVersion = 2;

    static NoteManager* GetSingleton() {
        static NoteManager instance;
        return &instance;
    }

    std::string GetNoteForQuest(RE::FormID questID) const {
        std::shared_lock lock(lock_);

        auto it = notesByQuest_.find(questID);
        if (it != notesByQuest_.end()) {
            return it->second.text;
        }
        return "";
    }

    void SaveNoteForQuest(RE::FormID questID, const std::string& text) {
        std::unique_lock lock(lock_);

        if (text.empty()) {
            // Empty text = delete note
            notesByQuest_.erase(questID);
        } else {
            Note note(text, questID);
            notesByQuest_[questID] = note;
        }
    }

    bool HasNoteForQuest(RE::FormID questID) const {
        std::shared_lock lock(lock_);
        return notesByQuest_.find(questID) != notesByQuest_.end();
    }

    void DeleteNoteForQuest(RE::FormID questID) {
        std::unique_lock lock(lock_);
        notesByQuest_.erase(questID);
    }

    std::unordered_map<RE::FormID, Note> GetAllNotes() const {
        std::shared_lock lock(lock_);
        return notesByQuest_;
    }

    size_t GetNoteCount() const {
        std::shared_lock lock(lock_);
        return notesByQuest_.size();
    }

    void Save(SKSE::SerializationInterface* intfc) {
        std::shared_lock lock(lock_);

        // Write note count
        std::uint32_t count = static_cast<std::uint32_t>(notesByQuest_.size());
        if (!intfc->WriteRecordData(&count, sizeof(count))) {
            spdlog::error("[SAVE] Failed to write note count");
            return;
        }

        // Write each note
        for (const auto& [questID, note] : notesByQuest_) {
            if (!note.Save(intfc)) {
                spdlog::error("[SAVE] Failed to write note for quest 0x{:X}", questID);
                return;
            }
        }

        spdlog::info("[SAVE] Saved {} notes", count);
    }

    void Load(SKSE::SerializationInterface* intfc) {
        std::unique_lock lock(lock_);
        notesByQuest_.clear();

        std::uint32_t type;
        std::uint32_t version;
        std::uint32_t length;

        while (intfc->GetNextRecordInfo(type, version, length)) {
            if (type == kDataKey) {
                if (version == 1) {
                    spdlog::warn("[LOAD] Version 1 save data found (Phase 1/2). Not compatible with Phase 3 quest notes. Skipping.");
                    continue;
                }
                if (version != kSerializationVersion) {
                    spdlog::warn("[LOAD] Unknown version: {}", version);
                    continue;
                }

                // Read note count
                std::uint32_t count = 0;
                if (!intfc->ReadRecordData(&count, sizeof(count))) {
                    spdlog::error("[LOAD] Failed to read note count");
                    return;
                }

                // Read each note
                for (std::uint32_t i = 0; i < count; ++i) {
                    Note note;
                    if (note.Load(intfc)) {
                        notesByQuest_[note.questID] = note;
                    } else {
                        spdlog::error("[LOAD] Failed to load note {}", i);
                    }
                }

                spdlog::info("[LOAD] Loaded {} notes", notesByQuest_.size());
            }
        }
    }

    void Revert(SKSE::SerializationInterface*) {
        std::unique_lock lock(lock_);
        notesByQuest_.clear();
        spdlog::info("[REVERT] Cleared all notes (new game)");
    }

private:
    NoteManager() = default;

    std::unordered_map<RE::FormID, Note> notesByQuest_;
    mutable std::shared_mutex lock_;
};

//=============================================================================
// Forward Declarations
//=============================================================================

namespace PapyrusBridge {
    void ShowQuestNoteInput(RE::FormID questID);
}

//=============================================================================
// Journal Quest Detection
//=============================================================================

RE::FormID GetCurrentQuestInJournal() {
    spdlog::info("[JOURNAL] GetCurrentQuestInJournal() entry");

    auto ui = RE::UI::GetSingleton();
    if (!ui || !ui->IsMenuOpen("Journal Menu")) {
        spdlog::info("[JOURNAL] Not in journal menu");
        return 0;  // Not in journal
    }

    spdlog::info("[JOURNAL] Getting JournalMenu pointer");
    auto journalMenu = ui->GetMenu<RE::JournalMenu>();
    if (!journalMenu) {
        spdlog::error("[JOURNAL] Failed to get JournalMenu pointer");
        return 0;
    }

    spdlog::info("[JOURNAL] Getting runtime data");
    // Access quest tab and get selected entry
    auto& rtData = journalMenu->GetRuntimeData();
    auto& questsTab = rtData.questsTab;

    spdlog::info("[JOURNAL] Checking questsTab.unk18");
    if (!questsTab.unk18.IsObject()) {
        spdlog::info("[JOURNAL] questsTab.unk18 is not an Object");
        return 0;
    }

    spdlog::info("[JOURNAL] Getting selectedEntry");
    // Get selectedEntry.formID
    RE::GFxValue selectedEntry;
    if (!questsTab.unk18.GetMember("selectedEntry", &selectedEntry) || !selectedEntry.IsObject()) {
        spdlog::info("[JOURNAL] No quest selected");
        return 0;
    }

    spdlog::info("[JOURNAL] Getting formID member");
    RE::GFxValue formIDValue;
    if (!selectedEntry.GetMember("formID", &formIDValue) || !formIDValue.IsNumber()) {
        spdlog::warn("[JOURNAL] selectedEntry has no formID");
        return 0;
    }

    spdlog::info("[JOURNAL] Extracting FormID value");
    RE::FormID questID = static_cast<RE::FormID>(formIDValue.GetUInt());
    if (questID == 0) {
        spdlog::info("[JOURNAL] Quest FormID is 0");
        return 0;
    }

    spdlog::info("[JOURNAL] Found quest 0x{:X}", questID);
    return questID;
}

//=============================================================================
// Forward Declarations
//=============================================================================

class QuestNoteHUDMenu;

//=============================================================================
// Journal Note Helper
//=============================================================================

class JournalNoteHelper {
public:
    static JournalNoteHelper* GetSingleton() {
        static JournalNoteHelper instance;
        return &instance;
    }

    void OnJournalOpen();
    void OnJournalClose();

private:
    JournalNoteHelper() = default;
};

//=============================================================================
// Quest Note HUD Menu
//=============================================================================

class QuestNoteHUDMenu : public RE::IMenu {
public:
    static constexpr const char* MENU_NAME = "QuestNoteHUDMenu";

    QuestNoteHUDMenu();
    ~QuestNoteHUDMenu() override;

    static void Register();
    static void Show();
    static void Hide();
    static QuestNoteHUDMenu* GetInstance();

    void UpdateNoteIndicator(RE::FormID questID, const std::string& questName);
    void ClearNoteIndicator();
    void UpdateQuestTracking();  // Called on input events when journal is open

private:
    RE::GFxValue tutorialText_;
    RE::GFxValue noteIndicatorText_;
    RE::GFxValue tutorialClip_;
    RE::GFxValue indicatorClip_;

    // Quest tracking state
    RE::FormID currentQuestID_ = 0;
    std::chrono::steady_clock::time_point selectionTime_;
    std::chrono::steady_clock::time_point lastPollTime_;
    bool notificationShown_ = false;

    void InitializeTextFields();
};

// Menu implementations
QuestNoteHUDMenu::QuestNoteHUDMenu() {
    spdlog::info("[HUD] QuestNoteHUDMenu constructor called");

    // Set menu flags for passive overlay (non-interactive, doesn't block other menus)
    using Flag = RE::UI_MENU_FLAGS;

    menuFlags.set(Flag::kDontHideCursorWhenTopmost);
    depthPriority = 1; // TEST: BELOW Journal to see if this fixes freeze

    auto scaleformManager = RE::BSScaleformManager::GetSingleton();
    if (!scaleformManager) {
        spdlog::error("[HUD] Failed to get ScaleformManager");
        return;
    }

    // Load custom toast SWF
    scaleformManager->LoadMovie(this, uiMovie, "toast", RE::GFxMovieView::ScaleModeType::kNoBorder);

    if (!uiMovie) {
        spdlog::error("[HUD] Failed to load movie");
        return;
    }

    spdlog::info("[HUD] Movie loaded successfully");

    // CRITICAL: Disable mouse input on the entire movie so it doesn't block Journal
    if (uiMovie) {
        RE::GFxValue root;
        bool gotRoot = uiMovie->GetVariable(&root, "_root");
        spdlog::info("[HUD] GetVariable(_root) = {}", gotRoot);

        if (gotRoot) {
            root.SetMember("mouseEnabled", false);
            root.SetMember("mouseChildren", false);
            root.SetMember("_x", 5.0);   // Top-left with padding
            root.SetMember("_y", 5.0);
            spdlog::info("[HUD] Disabled mouse input and positioned at (5,5)");
        } else {
            spdlog::error("[HUD] Failed to get _root");
        }
    }

    InitializeTextFields();
    spdlog::info("[HUD] Constructor completed successfully");
}

QuestNoteHUDMenu::~QuestNoteHUDMenu() {
    // Clear GFxValue references to prevent dangling pointers
    tutorialText_.SetUndefined();
    noteIndicatorText_.SetUndefined();
    tutorialClip_.SetUndefined();
    indicatorClip_.SetUndefined();
    spdlog::info("[HUD] QuestNoteHUDMenu destructor called");
}

void QuestNoteHUDMenu::Register() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) {
        spdlog::error("[HUD] Failed to get UI singleton");
        return;
    }

    ui->Register(MENU_NAME, []() -> RE::IMenu* {
        return new QuestNoteHUDMenu();
    });

    spdlog::info("[HUD] QuestNoteHUDMenu registered");
}

void QuestNoteHUDMenu::Show() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) {
        spdlog::error("[HUD] Show: failed to get UI singleton");
        return;
    }

    // First time: actually show the menu
    if (!ui->IsMenuOpen(MENU_NAME)) {
        auto msgQueue = RE::UIMessageQueue::GetSingleton();
        if (msgQueue) {
            msgQueue->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
            spdlog::info("[HUD] Menu shown (first time)");
        }
    } else {
        // Menu already exists, just make clips visible
        auto instance = GetInstance();
        if (instance) {
            spdlog::info("[HUD] Setting clips to visible");
            instance->tutorialClip_.SetMember("_visible", true);
            instance->indicatorClip_.SetMember("_visible", true);
            spdlog::info("[HUD] Clips made visible");
        }
    }
}

void QuestNoteHUDMenu::Hide() {
    auto instance = GetInstance();
    if (!instance) {
        return;
    }

    // Just hide clips, don't destroy the menu (check if valid first)
    if (instance->tutorialClip_.IsObject()) {
        instance->tutorialClip_.SetMember("_visible", false);
    }
    if (instance->indicatorClip_.IsObject()) {
        instance->indicatorClip_.SetMember("_visible", false);
    }
    spdlog::info("[HUD] Clips hidden");
}

QuestNoteHUDMenu* QuestNoteHUDMenu::GetInstance() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) {
        return nullptr;
    }

    auto menu = ui->GetMenu<QuestNoteHUDMenu>(MENU_NAME);
    return menu ? menu.get() : nullptr;
}

void QuestNoteHUDMenu::UpdateNoteIndicator(RE::FormID questID, const std::string& questName) {
    if (noteIndicatorText_.IsUndefined() || !noteIndicatorText_.IsObject()) {
        spdlog::warn("[HUD] UpdateNoteIndicator: text field invalid");
        return;
    }

    std::string text = "[" + questName + "] - Note present";

    spdlog::info("[HUD] About to call SetText with: {}", text);
    noteIndicatorText_.SetText(text.c_str());
    spdlog::info("[HUD] SetText returned successfully");
}

void QuestNoteHUDMenu::ClearNoteIndicator() {
    spdlog::info("[HUD] ClearNoteIndicator() called");
    if (noteIndicatorText_.IsUndefined() || !noteIndicatorText_.IsObject()) {
        spdlog::info("[HUD] Note indicator text is invalid, returning");
        return;
    }

    spdlog::info("[HUD] About to call SetText(\"\")");
    noteIndicatorText_.SetText("");
    spdlog::info("[HUD] SetText(\"\") returned - Note indicator cleared");
}

void QuestNoteHUDMenu::UpdateQuestTracking() {
    spdlog::info("[HUD] UpdateQuestTracking() called");

    // Throttle Scaleform queries to 10 times/sec (GetCurrentQuestInJournal is expensive)
    auto now = std::chrono::steady_clock::now();
    auto pollElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPollTime_);
    if (pollElapsed.count() < 100) {
        spdlog::info("[HUD] UpdateQuestTracking() skipped (throttled)");
        return;  // Skip this frame
    }
    lastPollTime_ = now;

    spdlog::info("[HUD] About to call GetCurrentQuestInJournal()");
    // Get currently selected quest
    RE::FormID questID = GetCurrentQuestInJournal();
    spdlog::info("[HUD] GetCurrentQuestInJournal() returned 0x{:X}", questID);

    // Quest changed?
    if (questID != currentQuestID_) {
        spdlog::info("[HUD] Quest changed from 0x{:X} to 0x{:X}", currentQuestID_, questID);
        currentQuestID_ = questID;
        selectionTime_ = std::chrono::steady_clock::now();
        notificationShown_ = false;

        spdlog::info("[HUD] About to call ClearNoteIndicator()");
        // Clear indicator when quest changes
        ClearNoteIndicator();
        spdlog::info("[HUD] ClearNoteIndicator() returned");

        if (questID != 0) {
            spdlog::info("[HUD] Quest changed to 0x{:X}", questID);
        }
        spdlog::info("[HUD] UpdateQuestTracking() returning after quest change");
        return;
    }
    spdlog::info("[HUD] Quest unchanged, checking hover delay");

    // No quest selected or already shown?
    if (questID == 0 || notificationShown_) return;

    // Check if 0.5 seconds passed since selection (reuse 'now' from throttle check)
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - selectionTime_);

    if (elapsed.count() >= 500) {
        notificationShown_ = true;

        // Check if quest has note and update indicator
        if (NoteManager::GetSingleton()->HasNoteForQuest(questID)) {
            auto quest = RE::TESForm::LookupByID<RE::TESQuest>(questID);
            std::string questName = quest ? quest->GetName() : "Unknown Quest";
            UpdateNoteIndicator(questID, questName);
            spdlog::info("[HUD] Quest 0x{:X} has note", questID);
        }
    }
}

void QuestNoteHUDMenu::InitializeTextFields() {
    auto movie = uiMovie;
    if (!movie) {
        spdlog::error("[HUD] InitializeTextFields: no movie");
        return;
    }

    // Get root
    RE::GFxValue root;
    if (!movie->GetVariable(&root, "_root")) {
        spdlog::error("[HUD] InitializeTextFields: failed to get _root");
        return;
    }

    if (!root.IsObject()) {
        spdlog::error("[HUD] InitializeTextFields: _root is not an object");
        return;
    }

    // Attach ToastClip from library for tutorial
    RE::GFxValue attachArgs[3];
    attachArgs[0].SetString("ToastClip");           // Linkage ID
    attachArgs[1].SetString("tutorialInstance");    // Instance name
    attachArgs[2].SetNumber(1);                     // Depth

    spdlog::info("[HUD] Attempting attachMovie with linkage='ToastClip'");
    root.Invoke("attachMovie", &tutorialClip_, attachArgs, 3);

    spdlog::info("[HUD] attachMovie returned, checking if object... Type={}, IsObject={}",
        (int)tutorialClip_.GetType(), tutorialClip_.IsObject());

    if (!tutorialClip_.IsObject()) {
        spdlog::error("[HUD] InitializeTextFields: attachMovie failed - clip is not an object");
        return;
    }

    spdlog::info("[HUD] ToastClip attached successfully");

    // DIAGNOSTIC: Dump all members of the clip
    spdlog::info("[HUD] Dumping tutorialClip_ members:");
    RE::GFxValue memberNames;
    if (tutorialClip_.GetMember("__proto__", &memberNames)) {
        spdlog::info("[HUD]   Has __proto__");
    }

    // Try common members
    RE::GFxValue testMember;
    const char* commonMembers[] = {"tf", "_x", "_y", "_width", "_height", "createTextField", "_name", "_target"};
    for (const char* name : commonMembers) {
        if (tutorialClip_.GetMember(name, &testMember)) {
            spdlog::info("[HUD]   Found member: '{}', Type={}", name, (int)testMember.GetType());
        } else {
            spdlog::info("[HUD]   Missing member: '{}'", name);
        }
    }

    spdlog::info("[HUD] Calling createTextField");

    // Create TextField dynamically inside the clip using AS2 createTextField
    RE::GFxValue createArgs[6];
    createArgs[0].SetString("tf");      // instance name
    createArgs[1].SetNumber(1);         // depth
    createArgs[2].SetNumber(0);         // x
    createArgs[3].SetNumber(0);         // y
    createArgs[4].SetNumber(400);       // width
    createArgs[5].SetNumber(50);        // height
    tutorialClip_.Invoke("createTextField", &tutorialText_, createArgs, 6);

    spdlog::info("[HUD] createTextField returned, checking result... Type={}, IsObject={}",
        (int)tutorialText_.GetType(), tutorialText_.IsObject());

    if (!tutorialText_.IsObject()) {
        spdlog::error("[HUD] InitializeTextFields: createTextField failed - returned Type={}",
            (int)tutorialText_.GetType());
        return;
    }

    // Configure TextField properties before setting text
    tutorialText_.SetMember("embedFonts", false);  // Use system fonts
    tutorialText_.SetMember("autoSize", "left");
    tutorialText_.SetMember("selectable", false);

    // Set tutorial text
    tutorialText_.SetText("Quest Notes: Press , to add/edit");

    spdlog::info("[HUD] Tutorial text set successfully");

    // Position tutorial clip (relative to root at 5,5 = top-left)
    tutorialClip_.SetMember("_x", RE::GFxValue(0.0));
    tutorialClip_.SetMember("_y", RE::GFxValue(0.0));

    spdlog::info("[HUD] Tutorial text created");

    // Attach ToastClip from library for note indicator
    RE::GFxValue indicatorAttachArgs[3];
    indicatorAttachArgs[0].SetString("ToastClip");           // Linkage ID
    indicatorAttachArgs[1].SetString("indicatorInstance");   // Instance name
    indicatorAttachArgs[2].SetNumber(2);                     // Different depth
    root.Invoke("attachMovie", &indicatorClip_, indicatorAttachArgs, 3);

    if (!indicatorClip_.IsObject()) {
        spdlog::error("[HUD] InitializeTextFields: failed to attach indicator ToastClip");
        return;
    }

    // Create TextField dynamically inside the indicator clip
    RE::GFxValue indicatorCreateArgs[6];
    indicatorCreateArgs[0].SetString("tf");     // instance name
    indicatorCreateArgs[1].SetNumber(1);        // depth
    indicatorCreateArgs[2].SetNumber(0);        // x
    indicatorCreateArgs[3].SetNumber(0);        // y
    indicatorCreateArgs[4].SetNumber(400);      // width
    indicatorCreateArgs[5].SetNumber(50);       // height
    indicatorClip_.Invoke("createTextField", &noteIndicatorText_, indicatorCreateArgs, 6);

    if (!noteIndicatorText_.IsObject()) {
        spdlog::error("[HUD] InitializeTextFields: createTextField failed for indicator");
        return;
    }

    // Configure indicator TextField properties
    noteIndicatorText_.SetMember("embedFonts", false);  // Use system fonts
    noteIndicatorText_.SetMember("autoSize", "left");
    noteIndicatorText_.SetMember("selectable", false);

    // Set empty text initially
    noteIndicatorText_.SetText("");

    spdlog::info("[HUD] Indicator text set successfully");

    // Position indicator clip below tutorial (relative to root at 5,5 = top-left)
    indicatorClip_.SetMember("_x", RE::GFxValue(0.0));
    indicatorClip_.SetMember("_y", RE::GFxValue(30.0));  // Below tutorial text

    spdlog::info("[HUD] Note indicator text created - INITIALIZATION COMPLETE");
    spdlog::info("[HUD] InitializeTextFields() returning successfully");
}

//=============================================================================
// Journal Note Helper Implementation (after QuestNoteHUDMenu is defined)
//=============================================================================

// Recursive GFxValue dumper
void DumpGFxValue(const RE::GFxValue& val, const std::string& path, int depth = 0, int maxDepth = 3) {
    if (depth > maxDepth) {
        spdlog::info("{}... (max depth)", path);
        return;
    }

    std::string indent(depth * 2, ' ');

    if (val.IsUndefined()) {
        spdlog::info("{}{} = undefined", indent, path);
    } else if (val.IsNull()) {
        spdlog::info("{}{} = null", indent, path);
    } else if (val.IsBool()) {
        spdlog::info("{}{} = {} (bool)", indent, path, val.GetBool());
    } else if (val.IsNumber()) {
        spdlog::info("{}{} = {} (number)", indent, path, val.GetNumber());
    } else if (val.IsString()) {
        spdlog::info("{}{} = \"{}\" (string)", indent, path, val.GetString());
    } else if (val.IsObject()) {
        spdlog::info("{}{} = {{object, Type={}}}", indent, path, (int)val.GetType());

        // Try to get common children - expanded list based on SWF structure
        const char* commonChildren[] = {
            "_name", "_x", "_y", "_width", "_height",
            // Quest-related
            "questList", "QuestList", "Quest_mc", "questsTab", "QuestsTab",
            "Entry0", "Entry1", "Entry2",
            "selectedEntry", "SelectedEntry",
            // Common Flash containers
            "textField", "TextField", "background", "Background",
            "MovieClip", "Container", "Panel", "List",
            // Journal-specific guesses
            "JournalPanel", "QuestPanel", "Content", "MainPanel"
        };
        for (const char* child : commonChildren) {
            RE::GFxValue childVal;
            if (val.GetMember(child, &childVal)) {
                DumpGFxValue(childVal, path + "." + child, depth + 1, maxDepth);
            }
        }
    } else {
        spdlog::info("{}{} = Type={}", indent, path, (int)val.GetType());
    }
}

void JournalNoteHelper::OnJournalOpen() {
    spdlog::info("[HELPER] OnJournalOpen() called");

    // DIAGNOSTIC: Dump Journal Menu structure
    auto ui = RE::UI::GetSingleton();
    if (!ui) {
        spdlog::error("[HELPER] Failed to get UI singleton");
        return;
    }

    auto journalMenu = ui->GetMenu<RE::JournalMenu>();
    if (!journalMenu) {
        spdlog::error("[HELPER] Failed to get Journal Menu");
        return;
    }

    spdlog::info("[HELPER] === JOURNAL MENU DUMP ===");
    spdlog::info("[HELPER] JournalMenu pointer: {}", (void*)journalMenu.get());
    spdlog::info("[HELPER] menuFlags: 0x{:X}", journalMenu->menuFlags.underlying());
    spdlog::info("[HELPER] depthPriority: {}", journalMenu->depthPriority);
    spdlog::info("[HELPER] inputContext: 0x{:X}", journalMenu->inputContext.underlying());

    if (journalMenu->uiMovie) {
        spdlog::info("[HELPER] uiMovie exists: {}", (void*)journalMenu->uiMovie.get());

        // Try to get _root
        RE::GFxValue root;
        bool gotRoot = journalMenu->uiMovie->GetVariable(&root, "_root");
        spdlog::info("[HELPER] GetVariable(_root) = {}", gotRoot);

        if (gotRoot) {
            spdlog::info("[HELPER] _root Type: {}, IsObject: {}", (int)root.GetType(), root.IsObject());

            // Try common members
            RE::GFxValue testVal;
            const char* members[] = {"_x", "_y", "_width", "_height", "_name", "_target", "addChild", "createTextField", "mouseEnabled"};
            for (const char* member : members) {
                if (root.GetMember(member, &testVal)) {
                    spdlog::info("[HELPER]   _root.{} exists, Type={}", member, (int)testVal.GetType());
                } else {
                    spdlog::info("[HELPER]   _root.{} NOT FOUND", member);
                }
            }
        }
    } else {
        spdlog::error("[HELPER] uiMovie is NULL");
    }

    spdlog::info("[HELPER] === END DUMP ===");

    // RECURSIVE DUMP of _root structure
    if (journalMenu->uiMovie) {
        RE::GFxValue root;
        if (journalMenu->uiMovie->GetVariable(&root, "_root")) {
            spdlog::info("[HELPER] === RECURSIVE _ROOT DUMP (depth=3) ===");
            DumpGFxValue(root, "_root", 0, 3);
            spdlog::info("[HELPER] === END RECURSIVE DUMP ===");
        }
    }

    // SEARCH: Find an existing TextField in Journal to examine font configuration
    if (journalMenu->uiMovie) {
        RE::GFxValue root;
        if (journalMenu->uiMovie->GetVariable(&root, "_root")) {
            spdlog::info("[HELPER] === SEARCHING FOR EXISTING TEXTFIELD ===");

            // Try common paths where TextFields might exist
            const char* searchPaths[] = {
                "QuestList",
                "questList",
                "Quest_mc",
                "QuestsTab",
                "questsTab",
                "Entry0",
                "Entry1"
            };

            for (const char* path : searchPaths) {
                RE::GFxValue obj;
                if (root.GetMember(path, &obj)) {
                    spdlog::info("[HELPER] Found {}, Type={}", path, (int)obj.GetType());

                    // Try to find textField child
                    RE::GFxValue textField;
                    const char* textFieldNames[] = {"textField", "TextField", "text", "_txt"};
                    for (const char* tfName : textFieldNames) {
                        if (obj.GetMember(tfName, &textField)) {
                            spdlog::info("[HELPER] FOUND TEXTFIELD: {}.{}, Type={}", path, tfName, (int)textField.GetType());

                            // Dump ALL properties we can find
                            const char* props[] = {
                                "text", "htmlText", "textColor", "font",
                                "embedFonts", "html", "autoSize", "wordWrap",
                                "_x", "_y", "_width", "_height",
                                "textWidth", "textHeight"
                            };

                            for (const char* prop : props) {
                                RE::GFxValue propVal;
                                if (textField.GetMember(prop, &propVal)) {
                                    if (propVal.IsString()) {
                                        spdlog::info("[HELPER]   {}.{} = \"{}\" (Type={})", tfName, prop, propVal.GetString(), (int)propVal.GetType());
                                    } else if (propVal.IsNumber()) {
                                        spdlog::info("[HELPER]   {}.{} = {} (Type={})", tfName, prop, propVal.GetNumber(), (int)propVal.GetType());
                                    } else if (propVal.IsBool()) {
                                        spdlog::info("[HELPER]   {}.{} = {} (Type={})", tfName, prop, propVal.GetBool(), (int)propVal.GetType());
                                    } else {
                                        spdlog::info("[HELPER]   {}.{} exists (Type={})", tfName, prop, (int)propVal.GetType());
                                    }
                                }
                            }

                            break; // Found one, stop searching
                        }
                    }
                    if (textField.IsObject()) break; // Stop searching paths
                }
            }

            spdlog::info("[HELPER] === END TEXTFIELD SEARCH ===");
        }
    }

    // TEST: Try creating a TextField in Journal's _root
    if (journalMenu->uiMovie) {
        RE::GFxValue root;
        if (journalMenu->uiMovie->GetVariable(&root, "_root")) {
            spdlog::info("[HELPER] TEST: Attempting to create TextField in Journal's _root");

            RE::GFxValue textField;
            RE::GFxValue createArgs[6];
            createArgs[0].SetString("testTextField");  // name
            createArgs[1].SetNumber(999999);           // VERY high depth to be on absolute top
            createArgs[2].SetNumber(500);              // CENTER screen (3000/2 = 1500, but try 500)
            createArgs[3].SetNumber(400);              // Middle height (812/2 = 406)
            createArgs[4].SetNumber(600);              // LARGE width
            createArgs[5].SetNumber(100);              // LARGE height

            root.Invoke("createTextField", &textField, createArgs, 6);

            spdlog::info("[HELPER] createTextField returned, Type={}, IsObject={}",
                (int)textField.GetType(), textField.IsObject());

            if (textField.IsObject()) {
                // Create TextFormat object using movie (not root)
                RE::GFxValue textFormat;
                journalMenu->uiMovie->CreateObject(&textFormat, "TextFormat");

                spdlog::info("[HELPER] TextFormat CreateObject returned, Type={}", (int)textFormat.GetType());

                if (textFormat.IsObject()) {
                    textFormat.SetMember("font", "$EverywhereFont");
                    textFormat.SetMember("size", 20);     // REQUIRED or glyphs fail
                    textFormat.SetMember("color", 0xFF0000);

                    // DUMP TextFormat object after setting properties
                    spdlog::info("[HELPER] === TEXTFORMAT DUMP ===");
                    const char* tfProps[] = {"font", "size", "color", "bold", "italic", "underline"};
                    for (const char* prop : tfProps) {
                        RE::GFxValue propVal;
                        if (textFormat.GetMember(prop, &propVal)) {
                            if (propVal.IsString()) {
                                spdlog::info("[HELPER]   TF.{} = \"{}\"", prop, propVal.GetString());
                            } else if (propVal.IsNumber()) {
                                spdlog::info("[HELPER]   TF.{} = {}", prop, propVal.GetNumber());
                            } else if (propVal.IsBool()) {
                                spdlog::info("[HELPER]   TF.{} = {}", prop, propVal.GetBool());
                            } else {
                                spdlog::info("[HELPER]   TF.{} exists (Type={})", prop, (int)propVal.GetType());
                            }
                        }
                    }
                    spdlog::info("[HELPER] === END TEXTFORMAT DUMP ===");

                    // Apply defaultTextFormat *before setting text*
                    textField.SetMember("defaultTextFormat", textFormat);

                    spdlog::info("[HELPER] defaultTextFormat set");
                }

                // Now the usual settings
                textField.SetMember("embedFonts", true);
                textField.SetMember("selectable", false);
                textField.SetMember("autoSize", "left");

                // Now set the text
                textField.SetMember("text", "***QUEST NOTES TEST***");

                // Apply format to existing text using setTextFormat()
                if (textFormat.IsObject()) {
                    textField.Invoke("setTextFormat", nullptr, &textFormat, 1);
                    spdlog::info("[HELPER] setTextFormat() called to apply format to existing text");
                }

                spdlog::info("[HELPER] TextField configured with TextFormat");

                // DUMP all TextField properties after configuration
                spdlog::info("[HELPER] === TEXTFIELD DUMP AFTER CREATION ===");
                const char* props[] = {
                    "text", "htmlText", "textColor", "font",
                    "embedFonts", "html", "autoSize", "wordWrap", "selectable",
                    "_x", "_y", "_width", "_height",
                    "textWidth", "textHeight", "type",
                    "defaultTextFormat", "backgroundColor", "background"
                };
                for (const char* prop : props) {
                    RE::GFxValue propVal;
                    if (textField.GetMember(prop, &propVal)) {
                        if (propVal.IsString()) {
                            spdlog::info("[HELPER]   {} = \"{}\" (Type={})", prop, propVal.GetString(), (int)propVal.GetType());
                        } else if (propVal.IsNumber()) {
                            spdlog::info("[HELPER]   {} = {} (Type={})", prop, propVal.GetNumber(), (int)propVal.GetType());
                        } else if (propVal.IsBool()) {
                            spdlog::info("[HELPER]   {} = {} (Type={})", prop, propVal.GetBool(), (int)propVal.GetType());
                        } else {
                            spdlog::info("[HELPER]   {} exists (Type={})", prop, (int)propVal.GetType());
                        }
                    }
                }
                spdlog::info("[HELPER] === END TEXTFIELD DUMP ===");
            } else {
                spdlog::error("[HELPER] createTextField failed");
            }
        }
    }
}

void JournalNoteHelper::OnJournalClose() {
    spdlog::info("[HELPER] Journal closed (HUD disabled to prevent UI corruption)");
}

//=============================================================================
// Input Handler
//=============================================================================

class InputHandler : public RE::BSTEventSink<RE::InputEvent*> {
public:
    static InputHandler* GetSingleton() {
        static InputHandler instance;
        return &instance;
    }

    static void Register() {
        auto input = RE::BSInputDeviceManager::GetSingleton();
        if (input) {
            input->AddEventSink(GetSingleton());
            spdlog::info("[INPUT] Input handler registered");
        } else {
            spdlog::error("[INPUT] Failed to get input device manager");
        }
    }

    RE::BSEventNotifyControl ProcessEvent(
        RE::InputEvent* const* a_event,
        RE::BSTEventSource<RE::InputEvent*>*) override {

        // Track journal open/close (Update() now called from HUD menu's AdvanceMovie)
        static bool wasJournalOpen = false;
        auto player = RE::PlayerCharacter::GetSingleton();
        if (player && player->Is3DLoaded()) {
            auto ui = RE::UI::GetSingleton();
            bool isJournalOpen = ui && ui->IsMenuOpen("Journal Menu");

            if (isJournalOpen && !wasJournalOpen) {
                JournalNoteHelper::GetSingleton()->OnJournalOpen();
            } else if (!isJournalOpen && wasJournalOpen) {
                JournalNoteHelper::GetSingleton()->OnJournalClose();
            }

            wasJournalOpen = isJournalOpen;
        }

        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Process button events only
        for (auto event = *a_event; event; event = event->next) {
            if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
                continue;
            }

            auto buttonEvent = event->AsButtonEvent();
            if (!buttonEvent || !buttonEvent->IsDown()) {
                continue;
            }

            // Update quest tracking disabled - HUD menu causes UI corruption
            // TODO: Inject text directly into Journal Menu instead of separate IMenu

            // Check for comma key (scan code 0x33 = 51) - JOURNAL ONLY
            if (buttonEvent->idCode == 51) {
                OnQuestNoteHotkey();
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }

private:
    InputHandler() = default;

    void OnQuestNoteHotkey() {
        // MUST be in Journal Menu
        auto ui = RE::UI::GetSingleton();
        if (!ui || !ui->IsMenuOpen("Journal Menu")) {
            return;
        }

        // Get current quest
        RE::FormID questID = GetCurrentQuestInJournal();
        if (questID == 0) {
            RE::DebugNotification("No quest selected");
            return;
        }

        // Show note input dialog
        PapyrusBridge::ShowQuestNoteInput(questID);
    }
};

//=============================================================================
// Papyrus Bridge
//=============================================================================

namespace PapyrusBridge {
    // Show quest note input (called from C++ InputHandler)
    void ShowQuestNoteInput(RE::FormID questID) {
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

        // Call Papyrus to show text input dialog
        auto args = RE::MakeFunctionArguments(
            static_cast<std::int32_t>(questID),
            RE::BSFixedString(questName),
            RE::BSFixedString(existingText)
        );
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

        vm->DispatchStaticCall("PersonalNotes", "ShowQuestNoteInput", args, callback);
        spdlog::info("[PAPYRUS] Note input opened for quest 0x{:X}", questID);
    }

    // Save quest note (called from Papyrus)
    void SaveQuestNote(RE::StaticFunctionTag*, std::int32_t questIDSigned, RE::BSFixedString noteText) {
        // Papyrus passes int32, but FormIDs are unsigned - convert properly
        // Modded quest IDs like 0xFE000000+ will be negative in int32
        RE::FormID questID = static_cast<RE::FormID>(static_cast<std::uint32_t>(questIDSigned));

        if (questID == 0) {
            spdlog::warn("[NOTE] Invalid quest ID");
            return;
        }

        std::string text(noteText.c_str());
        NoteManager::GetSingleton()->SaveNoteForQuest(questID, text);

        if (text.empty()) {
            spdlog::info("[NOTE] Deleted note for quest 0x{:X}", questID);
        } else {
            spdlog::info("[NOTE] Saved note for quest 0x{:X}", questID);
        }

        RE::DebugNotification("Quest note saved!");
    }

    // Register native functions
    bool Register(RE::BSScript::IVirtualMachine* vm) {
        vm->RegisterFunction("SaveQuestNote", "PersonalNotesNative", SaveQuestNote);
        spdlog::info("[PAPYRUS] Native functions registered");
        return true;
    }
}

//=============================================================================
// Logging Setup
//=============================================================================

void SetupLog() {
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("PersonalNotes.log", true);
    auto logger = std::make_shared<spdlog::logger>("log", std::move(sink));
    spdlog::set_default_logger(std::move(logger));
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    spdlog::info("PersonalNotes v1.0 initialized");
}

//=============================================================================
// Plugin Entry Point
//=============================================================================

void MessageHandler(SKSE::MessagingInterface::Message* msg) {
    switch (msg->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        // Register Papyrus functions
        if (auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton()) {
            PapyrusBridge::Register(vm);
        } else {
            spdlog::error("[MESSAGE] Failed to get VM for Papyrus registration");
        }

        // Register input handler after game data is loaded
        InputHandler::Register();

        // Register HUD menu
        QuestNoteHUDMenu::Register();

        spdlog::info("[MESSAGE] kDataLoaded - Handlers registered");
        break;
    }
}

void InitializePlugin() {
    SetupLog();

    // Register serialization callbacks
    auto serialization = SKSE::GetSerializationInterface();
    if (serialization) {
        serialization->SetUniqueID(NoteManager::kDataKey);

        serialization->SetSaveCallback([](SKSE::SerializationInterface* intfc) {
            if (!intfc->OpenRecord(NoteManager::kDataKey, NoteManager::kSerializationVersion)) {
                spdlog::error("[SAVE] Failed to open save record");
                return;
            }
            NoteManager::GetSingleton()->Save(intfc);
        });

        serialization->SetLoadCallback([](SKSE::SerializationInterface* intfc) {
            NoteManager::GetSingleton()->Load(intfc);
        });

        serialization->SetRevertCallback([](SKSE::SerializationInterface* intfc) {
            NoteManager::GetSingleton()->Revert(intfc);
        });

        spdlog::info("Serialization registered");
    } else {
        spdlog::error("Failed to get serialization interface!");
    }

    // Register message handler
    if (auto messaging = SKSE::GetMessagingInterface()) {
        messaging->RegisterListener(MessageHandler);
        spdlog::info("Messaging registered");
    } else {
        spdlog::error("Failed to get messaging interface!");
    }

    // Initialize NoteManager
    auto mgr = NoteManager::GetSingleton();
    spdlog::info("NoteManager initialized | Count: {}", mgr->GetNoteCount());

    spdlog::info("Plugin initialized");
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    spdlog::info("Plugin loading...");
    spdlog::info("SKSE version: {}.{}.{}",
        skse->RuntimeVersion().major(),
        skse->RuntimeVersion().minor(),
        skse->RuntimeVersion().patch());

    SKSE::RegisterForAPIInitEvent(InitializePlugin);

    return true;
}
