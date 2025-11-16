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
    static constexpr RE::FormID GENERAL_NOTE_ID = 0xFFFFFFFF;  // Special ID for general notes

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

    // General note methods (not tied to a quest)
    std::string GetGeneralNote() const {
        return GetNoteForQuest(GENERAL_NOTE_ID);
    }

    void SaveGeneralNote(const std::string& text) {
        SaveNoteForQuest(GENERAL_NOTE_ID, text);
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
    void ShowGeneralNoteInput();
}

//=============================================================================
// Journal Quest Detection
//=============================================================================

RE::FormID GetCurrentQuestInJournal() {
    auto ui = RE::UI::GetSingleton();
    if (!ui || !ui->IsMenuOpen("Journal Menu")) {
        return 0;  // Not in journal
    }

    auto journalMenu = ui->GetMenu<RE::JournalMenu>();
    if (!journalMenu) {
        spdlog::error("[JOURNAL] Failed to get JournalMenu pointer");
        return 0;
    }

    // Access quest tab and get selected entry
    auto& rtData = journalMenu->GetRuntimeData();
    auto& questsTab = rtData.questsTab;

    if (!questsTab.unk18.IsObject()) {
        return 0;
    }

    // Get selectedEntry.formID
    RE::GFxValue selectedEntry;
    if (!questsTab.unk18.GetMember("selectedEntry", &selectedEntry) || !selectedEntry.IsObject()) {
        return 0;
    }

    RE::GFxValue formIDValue;
    if (!selectedEntry.GetMember("formID", &formIDValue) || !formIDValue.IsNumber()) {
        spdlog::warn("[JOURNAL] selectedEntry has no formID");
        return 0;
    }

    RE::FormID questID = static_cast<RE::FormID>(formIDValue.GetUInt());
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
    void UpdateTextField(RE::FormID questID, bool forceUpdate = false);

private:
    JournalNoteHelper() = default;

    RE::GFxValue noteTextField_;
    RE::GFxValue textFormat_;
    RE::GPtr<RE::JournalMenu> journalMenu_;
    RE::FormID lastQuestID_ = 0;  // Track last quest to detect changes
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
    noteIndicatorText_.SetText(text.c_str());
}

void QuestNoteHUDMenu::ClearNoteIndicator() {
    if (noteIndicatorText_.IsUndefined() || !noteIndicatorText_.IsObject()) {
        return;
    }

    noteIndicatorText_.SetText("");
}

void QuestNoteHUDMenu::UpdateQuestTracking() {
    // Throttle Scaleform queries to 10 times/sec (GetCurrentQuestInJournal is expensive)
    auto now = std::chrono::steady_clock::now();
    auto pollElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPollTime_);
    if (pollElapsed.count() < 100) {
        return;  // Skip this frame
    }
    lastPollTime_ = now;

    // Get currently selected quest
    RE::FormID questID = GetCurrentQuestInJournal();

    // Quest changed?
    if (questID != currentQuestID_) {
        currentQuestID_ = questID;
        selectionTime_ = std::chrono::steady_clock::now();
        notificationShown_ = false;

        // Clear indicator when quest changes
        ClearNoteIndicator();
        return;
    }

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

    // Store journal menu reference
    journalMenu_ = journalMenu;

    // Create TextField in Journal's _root
    if (journalMenu->uiMovie) {
        RE::GFxValue root;
        if (journalMenu->uiMovie->GetVariable(&root, "_root")) {
            RE::GFxValue textField;
            RE::GFxValue createArgs[6];
            createArgs[0].SetString("questNoteTextField");  // name
            createArgs[1].SetNumber(999999);                // VERY high depth to be on absolute top
            createArgs[2].SetNumber(5);                     // TOP-LEFT x position
            createArgs[3].SetNumber(5);                     // TOP-LEFT y position
            createArgs[4].SetNumber(600);                   // width
            createArgs[5].SetNumber(50);                    // height

            root.Invoke("createTextField", &textField, createArgs, 6);

            if (textField.IsObject()) {
                // Create TextFormat object
                RE::GFxValue textFormat;
                journalMenu->uiMovie->CreateObject(&textFormat, "TextFormat");

                if (textFormat.IsObject()) {
                    textFormat.SetMember("font", "$EverywhereBoldFont");
                    textFormat.SetMember("size", 20);
                    textFormat.SetMember("color", 0xFFFFFF);  // White

                    // Apply defaultTextFormat
                    textField.SetMember("defaultTextFormat", textFormat);
                }

                // Configure TextField
                textField.SetMember("embedFonts", true);
                textField.SetMember("selectable", false);
                textField.SetMember("autoSize", "left");
                textField.SetMember("text", "");

                // Apply format to existing text
                if (textFormat.IsObject()) {
                    textField.Invoke("setTextFormat", nullptr, &textFormat, 1);
                }

                // Store references
                noteTextField_ = textField;
                textFormat_ = textFormat;

                // Get initial quest and update TextField
                RE::FormID currentQuest = GetCurrentQuestInJournal();
                UpdateTextField(currentQuest);
                lastQuestID_ = currentQuest;  // Initialize tracking
            } else {
                spdlog::error("[HELPER] createTextField failed");
            }
        }
    }
}

void JournalNoteHelper::OnJournalClose() {
    // Clear references
    noteTextField_.SetUndefined();
    textFormat_.SetUndefined();
    journalMenu_ = nullptr;
    lastQuestID_ = 0;  // Reset tracking
}

void JournalNoteHelper::UpdateTextField(RE::FormID questID, bool forceUpdate) {
    if (!noteTextField_.IsObject()) {
        return; // TextField not initialized
    }

    // Only update if quest changed (prevent spam), unless forced
    if (!forceUpdate && questID == lastQuestID_) {
        return;
    }

    lastQuestID_ = questID;  // Track current quest

    std::string message;

    if (questID == 0) {
        // No quest selected - clear text
        message = "";
    } else {
        // Check if note exists for this quest
        bool hasNote = NoteManager::GetSingleton()->HasNoteForQuest(questID);

        if (hasNote) {
            message = "Press , to edit note";
        } else {
            message = "Press , to add note";
        }
    }

    // Update text
    noteTextField_.SetMember("text", message.c_str());

    // Reapply format (needed after text change)
    if (textFormat_.IsObject()) {
        noteTextField_.Invoke("setTextFormat", nullptr, &textFormat_, 1);
    }
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

        // Process input events
        for (auto event = *a_event; event; event = event->next) {
            auto eventType = event->eventType;

            // Handle button events (keyboard, mouse clicks)
            if (eventType == RE::INPUT_EVENT_TYPE::kButton) {
                auto buttonEvent = event->AsButtonEvent();
                if (!buttonEvent) {
                    continue;
                }

                // Check if in Journal Menu
                auto ui = RE::UI::GetSingleton();
                bool inJournal = ui && ui->IsMenuOpen("Journal Menu");
                uint32_t keyCode = buttonEvent->idCode;

                if (inJournal) {
                    // Update TextField on navigation RELEASE (arrow keys, mouse clicks)
                    // Use IsUp() so Journal processes the input first, then we read the updated selection
                    // Arrow Up = 200, Arrow Down = 208, Mouse clicks = 256
                    if (buttonEvent->IsUp()) {
                        if (keyCode == 200 || keyCode == 208 || keyCode == 256) { // Up, Down, or Left Mouse
                            RE::FormID questID = GetCurrentQuestInJournal();
                            JournalNoteHelper::GetSingleton()->UpdateTextField(questID);
                        }
                    }
                }

                // Comma key (scan code 51) - context-dependent behavior
                if (buttonEvent->IsDown() && keyCode == 51) {
                    if (inJournal) {
                        // In Journal Menu → Quest note
                        OnQuestNoteHotkey();
                    } else {
                        // During gameplay → General note
                        PapyrusBridge::ShowGeneralNoteInput();
                    }
                }
            }
            // Handle mouse move events (hover detection in Journal)
            else if (eventType == RE::INPUT_EVENT_TYPE::kMouseMove) {
                auto ui = RE::UI::GetSingleton();
                if (ui && ui->IsMenuOpen("Journal Menu")) {
                    // Mouse moved in Journal - check if quest selection changed
                    RE::FormID questID = GetCurrentQuestInJournal();
                    JournalNoteHelper::GetSingleton()->UpdateTextField(questID);
                    // Note: UpdateTextField has built-in change detection to prevent spam
                }
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

        // Update TextField to reflect new note state immediately (force update even if same quest)
        JournalNoteHelper::GetSingleton()->UpdateTextField(questID, true);

        RE::DebugNotification("Quest note saved!");
    }

    // Show general note input (called from C++ InputHandler)
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
    }

    // Save general note (called from Papyrus)
    void SaveGeneralNote(RE::StaticFunctionTag*, RE::BSFixedString noteText) {
        std::string text(noteText.c_str());
        NoteManager::GetSingleton()->SaveGeneralNote(text);

        RE::DebugNotification("General note saved!");
    }

    // Register native functions
    bool Register(RE::BSScript::IVirtualMachine* vm) {
        vm->RegisterFunction("SaveQuestNote", "PersonalNotesNative", SaveQuestNote);
        vm->RegisterFunction("SaveGeneralNote", "PersonalNotesNative", SaveGeneralNote);
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
