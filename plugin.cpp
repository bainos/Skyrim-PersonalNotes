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

#include <windows.h>
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <ctime>

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
// Settings Manager
//=============================================================================

class SettingsManager {
public:
    static SettingsManager* GetSingleton() {
        static SettingsManager instance;
        return &instance;
    }

    void LoadSettings() {
        constexpr auto path = L"Data/SKSE/Plugins/PersonalNotes.ini";

        // Helper to read float from INI (no GetPrivateProfileFloat in Windows API)
        auto ReadFloat = [](const wchar_t* section, const wchar_t* key, float defaultValue, const wchar_t* iniPath) -> float {
            wchar_t buffer[32];
            GetPrivateProfileStringW(section, key, L"", buffer, 32, iniPath);
            if (buffer[0] == L'\0') {
                return defaultValue;
            }
            return std::wcstof(buffer, nullptr);
        };

        // TextField
        textFieldX = ReadFloat(L"TextField", L"fPositionX", 5.0f, path);
        textFieldY = ReadFloat(L"TextField", L"fPositionY", 5.0f, path);
        textFieldFontSize = GetPrivateProfileIntW(L"TextField", L"iFontSize", 20, path);
        textFieldColor = GetPrivateProfileIntW(L"TextField", L"iTextColor", 0xFFFFFF, path);

        // TextInput
        textInputWidth = GetPrivateProfileIntW(L"TextInput", L"iWidth", 500, path);
        textInputHeight = GetPrivateProfileIntW(L"TextInput", L"iHeight", 400, path);
        textInputFontSize = GetPrivateProfileIntW(L"TextInput", L"iFontSize", 14, path);
        textInputAlignment = GetPrivateProfileIntW(L"TextInput", L"iAlignment", 0, path);

        // Hotkey
        noteHotkeyScanCode = GetPrivateProfileIntW(L"Hotkey", L"iScanCode", 51, path);

        spdlog::info("[SETTINGS] Loaded from INI");
    }

    // TextField
    float textFieldX = 5.0f;
    float textFieldY = 5.0f;
    int textFieldFontSize = 20;
    int textFieldColor = 0xFFFFFF;

    // TextInput
    int textInputWidth = 500;
    int textInputHeight = 400;
    int textInputFontSize = 14;
    int textInputAlignment = 0;

    // Hotkey
    int noteHotkeyScanCode = 51;

private:
    SettingsManager() = default;
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
// Journal Note Helper Implementation
//=============================================================================

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
            auto settings = SettingsManager::GetSingleton();
            createArgs[0].SetString("questNoteTextField");  // name
            createArgs[1].SetNumber(999999);                // VERY high depth to be on absolute top
            createArgs[2].SetNumber(settings->textFieldX);  // TOP-LEFT x position
            createArgs[3].SetNumber(settings->textFieldY);  // TOP-LEFT y position
            createArgs[4].SetNumber(600);                   // width
            createArgs[5].SetNumber(50);                    // height

            root.Invoke("createTextField", &textField, createArgs, 6);

            if (textField.IsObject()) {
                // Create TextFormat object
                RE::GFxValue textFormat;
                journalMenu->uiMovie->CreateObject(&textFormat, "TextFormat");

                if (textFormat.IsObject()) {
                    textFormat.SetMember("font", "$EverywhereBoldFont");
                    textFormat.SetMember("size", settings->textFieldFontSize);
                    textFormat.SetMember("color", settings->textFieldColor);

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

        // Track journal open/close for JournalNoteHelper lifecycle
        auto player = RE::PlayerCharacter::GetSingleton();
        if (player && player->Is3DLoaded()) {
            auto ui = RE::UI::GetSingleton();
            bool isJournalOpen = ui && ui->IsMenuOpen("Journal Menu");

            if (isJournalOpen && !wasJournalOpen_) {
                JournalNoteHelper::GetSingleton()->OnJournalOpen();
            } else if (!isJournalOpen && wasJournalOpen_) {
                JournalNoteHelper::GetSingleton()->OnJournalClose();
            }

            wasJournalOpen_ = isJournalOpen;
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

                // Note hotkey - context-dependent behavior
                if (buttonEvent->IsDown() && keyCode == SettingsManager::GetSingleton()->noteHotkeyScanCode) {
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

    bool wasJournalOpen_ = false;  // Track journal state across events

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

        // Get TextInput settings
        auto settings = SettingsManager::GetSingleton();

        // Call Papyrus to show text input dialog
        auto args = RE::MakeFunctionArguments(
            static_cast<std::int32_t>(questID),
            RE::BSFixedString(questName),
            RE::BSFixedString(existingText),
            static_cast<std::int32_t>(settings->textInputWidth),
            static_cast<std::int32_t>(settings->textInputHeight),
            static_cast<std::int32_t>(settings->textInputFontSize),
            static_cast<std::int32_t>(settings->textInputAlignment)
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

        // Get TextInput settings
        auto settings = SettingsManager::GetSingleton();

        // Call Papyrus to show text input dialog
        auto args = RE::MakeFunctionArguments(
            std::move(RE::BSFixedString("")),           // questName (empty for general)
            std::move(RE::BSFixedString(existingText)),
            static_cast<std::int32_t>(settings->textInputWidth),
            static_cast<std::int32_t>(settings->textInputHeight),
            static_cast<std::int32_t>(settings->textInputFontSize),
            static_cast<std::int32_t>(settings->textInputAlignment)
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

        spdlog::info("[MESSAGE] kDataLoaded - Handlers registered");
        break;
    }
}

void InitializePlugin() {
    SetupLog();

    // Load settings from INI
    SettingsManager::GetSingleton()->LoadSettings();

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
