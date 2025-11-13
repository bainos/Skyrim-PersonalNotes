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
        spdlog::error("[JOURNAL] questsTab.unk18 is not an Object");
        return 0;
    }

    // Get selectedEntry.formID
    RE::GFxValue selectedEntry;
    if (!questsTab.unk18.GetMember("selectedEntry", &selectedEntry) || !selectedEntry.IsObject()) {
        spdlog::debug("[JOURNAL] No quest selected");
        return 0;
    }

    RE::GFxValue formIDValue;
    if (!selectedEntry.GetMember("formID", &formIDValue) || !formIDValue.IsNumber()) {
        spdlog::warn("[JOURNAL] selectedEntry has no formID");
        return 0;
    }

    RE::FormID questID = static_cast<RE::FormID>(formIDValue.GetUInt());
    if (questID == 0) {
        spdlog::debug("[JOURNAL] Quest FormID is 0");
        return 0;
    }

    spdlog::info("[JOURNAL] Found quest 0x{:X}", questID);
    return questID;
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

        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        for (auto event = *a_event; event; event = event->next) {
            if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
                continue;
            }

            auto buttonEvent = event->AsButtonEvent();
            if (!buttonEvent || !buttonEvent->IsDown()) {
                continue;
            }

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
