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
#include <shared_mutex>
#include <ctime>

//=============================================================================
// Data Structures
//=============================================================================

struct Note {
    std::string text;
    std::time_t timestamp;
    std::string context;

    Note() : timestamp(0) {}
    Note(const std::string& t, const std::string& c)
        : text(t), timestamp(std::time(nullptr)), context(c) {}

    bool Save(SKSE::SerializationInterface* intfc) const {
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

        // Write context length and context
        std::uint32_t contextLen = static_cast<std::uint32_t>(context.size());
        if (!intfc->WriteRecordData(&contextLen, sizeof(contextLen))) {
            return false;
        }
        if (contextLen > 0 && !intfc->WriteRecordData(context.data(), contextLen)) {
            return false;
        }

        return true;
    }

    bool Load(SKSE::SerializationInterface* intfc) {
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

        // Read context
        std::uint32_t contextLen = 0;
        if (!intfc->ReadRecordData(&contextLen, sizeof(contextLen))) {
            return false;
        }
        if (contextLen > 0) {
            context.resize(contextLen);
            if (!intfc->ReadRecordData(context.data(), contextLen)) {
                return false;
            }
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
    static constexpr std::uint32_t kSerializationVersion = 1;

    static NoteManager* GetSingleton() {
        static NoteManager instance;
        return &instance;
    }

    void AddNote(const std::string& text, const std::string& context) {
        std::unique_lock lock(lock_);

        Note note(text, context);
        notes_.push_back(note);

        spdlog::info("[NOTE] Added: '{}' | Context: '{}' | Total: {}",
            text, context, notes_.size());
    }

    std::vector<Note> GetAllNotes() const {
        std::shared_lock lock(lock_);
        return notes_;
    }

    size_t GetNoteCount() const {
        std::shared_lock lock(lock_);
        return notes_.size();
    }

    void Save(SKSE::SerializationInterface* intfc) {
        std::shared_lock lock(lock_);

        // Write note count
        std::uint32_t count = static_cast<std::uint32_t>(notes_.size());
        if (!intfc->WriteRecordData(&count, sizeof(count))) {
            spdlog::error("[SAVE] Failed to write note count");
            return;
        }

        // Write each note
        for (const auto& note : notes_) {
            if (!note.Save(intfc)) {
                spdlog::error("[SAVE] Failed to write note");
                return;
            }
        }

        spdlog::info("[SAVE] Saved {} notes", count);
    }

    void Load(SKSE::SerializationInterface* intfc) {
        std::unique_lock lock(lock_);
        notes_.clear();

        std::uint32_t type;
        std::uint32_t version;
        std::uint32_t length;

        while (intfc->GetNextRecordInfo(type, version, length)) {
            if (type == kDataKey) {
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
                        notes_.push_back(note);
                    } else {
                        spdlog::error("[LOAD] Failed to load note {}", i);
                    }
                }

                spdlog::info("[LOAD] Loaded {} notes", notes_.size());
            }
        }
    }

    void Revert(SKSE::SerializationInterface*) {
        std::unique_lock lock(lock_);
        notes_.clear();
        spdlog::info("[REVERT] Cleared all notes (new game)");
    }

private:
    NoteManager() = default;

    std::vector<Note> notes_;
    mutable std::shared_mutex lock_;
};

//=============================================================================
// Forward Declarations
//=============================================================================

namespace PapyrusBridge {
    void RequestTextInput();
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

            // Check for comma key (scan code 0x33 = 51)
            if (buttonEvent->idCode == 51) {
                OnHotkeyPressed();
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }

private:
    InputHandler() = default;

    void OnHotkeyPressed() {
        // Don't trigger if console or menu is open
        auto ui = RE::UI::GetSingleton();
        if (ui && (ui->IsMenuOpen(RE::Console::MENU_NAME) || ui->GameIsPaused())) {
            return;
        }

        spdlog::info("[HOTKEY] Insert key pressed");

        // Trigger Papyrus text input
        PapyrusBridge::RequestTextInput();
    }
};

//=============================================================================
// Papyrus Bridge
//=============================================================================

namespace PapyrusBridge {
    // Called from Papyrus when user submits note text
    void OnNoteReceived(RE::StaticFunctionTag*, RE::BSFixedString noteText) {
        if (noteText.empty()) {
            spdlog::info("[PAPYRUS] Empty note received, ignoring");
            return;
        }

        // Get context
        auto player = RE::PlayerCharacter::GetSingleton();
        std::string context = "General";

        if (player && player->GetParentCell()) {
            auto cell = player->GetParentCell();
            if (cell->GetName() && cell->GetName()[0] != '\0') {
                context = cell->GetName();
            }
        }

        // Add note
        std::string text(noteText.c_str());
        NoteManager::GetSingleton()->AddNote(text, context);

        spdlog::info("[PAPYRUS] Note received: '{}' | Context: '{}'", text, context);
        RE::DebugNotification("Note saved!");
    }

    // Request text input from player (called from C++ InputHandler)
    void RequestTextInput() {
        spdlog::info("[PAPYRUS] Requesting text input...");

        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            spdlog::error("[PAPYRUS] Failed to get VM");
            return;
        }

        auto policy = vm->GetObjectHandlePolicy();
        if (!policy) {
            spdlog::error("[PAPYRUS] Failed to get policy");
            return;
        }

        // Call PersonalNotes.RequestInput()
        auto args = RE::MakeFunctionArguments();
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

        vm->DispatchStaticCall("PersonalNotes", "RequestInput", args, callback);
        spdlog::info("[PAPYRUS] RequestInput dispatched");
    }

    // Register native functions
    bool Register(RE::BSScript::IVirtualMachine* vm) {
        vm->RegisterFunction("OnNoteReceived", "PersonalNotesNative", OnNoteReceived);
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
