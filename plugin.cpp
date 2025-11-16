/**
 * PersonalNotes - SKSE Plugin for Skyrim SE/AE
 *
 * Simple in-game note-taking system using Extended Vanilla Menus for text input.
 * Notes persist across saves via SKSE co-save system.
 *
 * ERROR HANDLING STRATEGY:
 * - Functions that retrieve data return empty/default values on error (e.g., empty string, 0, nullptr)
 * - Functions with side effects return void and log errors internally
 * - Critical errors are logged with spdlog::error(), warnings with spdlog::warn()
 * - Serialization continues processing remaining data even if individual items fail
 * - User input is validated and sanitized before storage
 * - GFx lifecycle is managed with explicit initialization checks
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
// Version Information
//=============================================================================

#define PERSONAL_NOTES_VERSION_MAJOR 1
#define PERSONAL_NOTES_VERSION_MINOR 0
#define PERSONAL_NOTES_VERSION_PATCH 0

//=============================================================================
// Constants
//=============================================================================

namespace UIConstants {
    // TextField display constants
    constexpr int TEXTFIELD_TOP_DEPTH = 999999;      // Very high depth to render on absolute top
    constexpr int TEXTFIELD_DEFAULT_WIDTH = 600;     // Default width for text field
    constexpr int TEXTFIELD_DEFAULT_HEIGHT = 50;     // Default height for text field
}

namespace KeyCodes {
    // Keyboard scan codes
    constexpr uint32_t ARROW_UP = 200;
    constexpr uint32_t ARROW_DOWN = 208;

    // Mouse button codes
    constexpr uint32_t MOUSE_LEFT = 256;
}

//=============================================================================
// Note Utilities
//=============================================================================

namespace NoteUtils {
    // Maximum length for note text (prevent memory issues)
    constexpr size_t MAX_NOTE_LENGTH = 4096;

    /**
     * Validates note text for basic requirements.
     * @param text The text to validate
     * @param maxLength Maximum allowed length
     * @return true if valid, false otherwise
     */
    bool ValidateNoteText(const std::string& text, size_t maxLength = MAX_NOTE_LENGTH) {
        // Length check
        if (text.length() > maxLength) {
            spdlog::warn("[VALIDATE] Note text exceeds maximum length: {} > {}", text.length(), maxLength);
            return false;
        }
        return true;
    }

    /**
     * Sanitizes note text for safe storage and serialization.
     * - Enforces length limits
     * - Removes null bytes (can cause issues in C-string interop)
     *
     * @param input The raw input text
     * @return Sanitized text safe for storage
     */
    std::string SanitizeNoteText(const std::string& input) {
        // 1. Enforce length limits
        std::string sanitized = input.substr(0, MAX_NOTE_LENGTH);

        // 2. Remove null bytes (can cause issues in C-string interop)
        sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\0'),
                        sanitized.end());

        // Log if sanitization occurred
        if (sanitized.length() != input.length()) {
            spdlog::info("[SANITIZE] Note text sanitized: {} -> {} chars",
                         input.length(), sanitized.length());
        }

        return sanitized;
    }
}

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

/**
 * @class SettingsManager
 * @brief Manages plugin configuration loaded from INI file.
 *
 * Loads and validates settings from Data/SKSE/Plugins/PersonalNotes.ini
 * including UI positioning, text formatting, and hotkey configuration.
 * All loaded values are clamped to reasonable ranges.
 */
class SettingsManager {
public:
    /**
     * @brief Get the singleton instance.
     * @return Pointer to singleton instance (never null)
     */
    static SettingsManager* GetSingleton() {
        static SettingsManager instance;
        return &instance;
    }

    /**
     * @brief Load and validate settings from INI file.
     *
     * Reads configuration from Data/SKSE/Plugins/PersonalNotes.ini and
     * clamps all values to valid ranges (e.g., font sizes 8-72, positions within 4K bounds).
     */
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
        quickAccessScanCode = GetPrivateProfileIntW(L"Hotkey", L"iQuickAccessScanCode", 52, path);

        // Validate and clamp loaded values to reasonable ranges
        textFieldX = std::clamp(textFieldX, 0.0f, 3840.0f);      // Max 4K width
        textFieldY = std::clamp(textFieldY, 0.0f, 2160.0f);      // Max 4K height
        textFieldFontSize = std::clamp(textFieldFontSize, 8, 72);
        // textFieldColor: allow any value (0x000000 to 0xFFFFFF valid)

        textInputWidth = std::clamp(textInputWidth, 200, 3840);
        textInputHeight = std::clamp(textInputHeight, 100, 2160);
        textInputFontSize = std::clamp(textInputFontSize, 8, 72);
        textInputAlignment = std::clamp(textInputAlignment, 0, 2);  // 0=left, 1=center, 2=right

        noteHotkeyScanCode = std::clamp(noteHotkeyScanCode, 0, 255);  // Valid scan code range
        quickAccessScanCode = std::clamp(quickAccessScanCode, 0, 255);  // Valid scan code range

        spdlog::info("[SETTINGS] Loaded and validated from INI");
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
    int quickAccessScanCode = 52;  // dot key

private:
    SettingsManager() = default;
};

//=============================================================================
// Note Manager
//=============================================================================

/**
 * @class NoteManager
 * @brief Thread-safe manager for quest and general notes with SKSE serialization.
 *
 * Manages a collection of notes indexed by quest FormID. Thread-safe for
 * concurrent read/write operations using shared_mutex. Notes are persisted
 * across game sessions via SKSE co-save system.
 *
 * @note Uses FormID 0xFFFFFFFF (GENERAL_NOTE_ID) for general notes not tied to specific quests.
 * @thread_safety All public methods are thread-safe.
 */
class NoteManager {
public:
    static constexpr std::uint32_t kDataKey = 'PNOT';  // PersonalNOTes
    static constexpr std::uint32_t kSerializationVersion = 2;
    static constexpr RE::FormID GENERAL_NOTE_ID = 0xFFFFFFFF;  // Special ID for general notes

    /**
     * @brief Get the singleton instance.
     * @return Pointer to singleton instance (never null, valid for program lifetime)
     */
    static NoteManager* GetSingleton() {
        static NoteManager instance;
        return &instance;
    }

    /**
     * @brief Retrieves note text for a specific quest.
     * @param questID The quest's FormID (use GENERAL_NOTE_ID for general notes)
     * @return Note text if exists, empty string otherwise
     * @thread_safety Thread-safe (uses shared lock)
     */
    [[nodiscard]] std::string GetNoteForQuest(RE::FormID questID) const {
        std::shared_lock lock(lock_);

        if (auto it = notesByQuest_.find(questID); it != notesByQuest_.end()) {
            return it->second.text;
        }
        return "";
    }

    /**
     * @brief Saves or updates a note for a quest.
     * @param questID The quest's FormID (0 is invalid, GENERAL_NOTE_ID for general notes)
     * @param text Note text to save (empty string deletes the note)
     * @thread_safety Thread-safe (uses unique lock)
     * @note Input is validated and sanitized before storage
     */
    void SaveNoteForQuest(RE::FormID questID, const std::string& text) {
        // Validate FormID
        if (questID == 0) {
            spdlog::warn("[NOTE] Invalid quest ID: 0");
            return;
        }

        // Validate quest exists (except for GENERAL_NOTE_ID)
        if (questID != GENERAL_NOTE_ID) {
            auto quest = RE::TESForm::LookupByID<RE::TESQuest>(questID);
            if (!quest) {
                spdlog::warn("[NOTE] Quest 0x{:X} not found, saving note anyway", questID);
                // Allow saving anyway - quest might be from another plugin
            }
        }

        std::unique_lock lock(lock_);

        if (text.empty()) {
            // Empty text = delete note
            notesByQuest_.erase(questID);
        } else {
            // Sanitize input text before storage
            std::string sanitizedText = NoteUtils::SanitizeNoteText(text);

            Note note(sanitizedText, questID);
            notesByQuest_[questID] = note;
        }
    }

    /**
     * @brief Checks if a note exists for a quest.
     * @param questID The quest's FormID
     * @return true if note exists, false otherwise
     * @thread_safety Thread-safe (uses shared lock)
     */
    [[nodiscard]] bool HasNoteForQuest(RE::FormID questID) const {
        std::shared_lock lock(lock_);
        return notesByQuest_.find(questID) != notesByQuest_.end();
    }

    /**
     * @brief Deletes a note for a quest.
     * @param questID The quest's FormID
     * @thread_safety Thread-safe (uses unique lock)
     */
    void DeleteNoteForQuest(RE::FormID questID) {
        std::unique_lock lock(lock_);
        notesByQuest_.erase(questID);
    }

    /**
     * @brief Get general note (not tied to any quest).
     * @return General note text, empty if none
     * @thread_safety Thread-safe
     */
    [[nodiscard]] std::string GetGeneralNote() const {
        return GetNoteForQuest(GENERAL_NOTE_ID);
    }

    /**
     * @brief Save general note (not tied to any quest).
     * @param text Note text to save
     * @thread_safety Thread-safe
     */
    void SaveGeneralNote(const std::string& text) {
        SaveNoteForQuest(GENERAL_NOTE_ID, text);
    }

    /**
     * @brief Get all notes as a map.
     * @return Copy of all notes indexed by quest FormID
     * @warning Returns a copy - expensive for large note collections
     * @thread_safety Thread-safe (uses shared lock)
     */
    [[nodiscard]] std::unordered_map<RE::FormID, Note> GetAllNotes() const {
        std::shared_lock lock(lock_);
        return notesByQuest_;
    }

    /**
     * @brief Get total number of notes.
     * @return Count of all stored notes
     * @thread_safety Thread-safe (uses shared lock)
     */
    [[nodiscard]] size_t GetNoteCount() const {
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

        spdlog::info("[SAVE] Saved {} notes (version {})", count, kSerializationVersion);
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
                    spdlog::warn("[LOAD] Version 1 save data found (expected v{}). Legacy format not compatible. Skipping.", kSerializationVersion);
                    continue;
                }
                if (version != kSerializationVersion) {
                    spdlog::warn("[LOAD] Unknown save version: {} (expected v{}). Skipping.", version, kSerializationVersion);
                    continue;
                }

                LoadNotesData(intfc);
            }
        }
    }

    void LoadNotesData(SKSE::SerializationInterface* intfc) {
        // Read note count
        std::uint32_t count = 0;
        if (!intfc->ReadRecordData(&count, sizeof(count))) {
            spdlog::error("[LOAD] Failed to read note count");
            return;  // Now safe - won't break record iteration
        }

        std::uint32_t loadedCount = 0;
        std::uint32_t failedCount = 0;

        // Read each note
        for (std::uint32_t i = 0; i < count; ++i) {
            Note note;
            if (note.Load(intfc)) {
                notesByQuest_[note.questID] = note;
                loadedCount++;
            } else {
                spdlog::error("[LOAD] Failed to load note {}/{}", i + 1, count);
                failedCount++;
                // Continue loading remaining notes instead of failing completely
            }
        }

        if (failedCount > 0) {
            spdlog::warn("[LOAD] Loaded {}/{} notes successfully ({} failed, version {})",
                         loadedCount, count, failedCount, kSerializationVersion);
        } else {
            spdlog::info("[LOAD] Loaded {}/{} notes successfully (version {})", loadedCount, count, kSerializationVersion);
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
    void ShowNotesListMenu();
}

//=============================================================================
// Journal Quest Detection
//=============================================================================

[[nodiscard]] RE::FormID GetCurrentQuestInJournal() {
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

/**
 * @class JournalNoteHelper
 * @brief Manages UI overlay in Journal Menu showing note status.
 *
 * Creates and manages a TextField in the Journal Menu that displays whether
 * the selected quest has an associated note. Handles lifecycle (open/close)
 * and updates text based on quest selection changes.
 */
class JournalNoteHelper {
public:
    /**
     * @brief Get the singleton instance.
     * @return Pointer to singleton instance (never null)
     */
    static JournalNoteHelper* GetSingleton() {
        static JournalNoteHelper instance;
        return &instance;
    }

    /**
     * @brief Initialize UI elements when Journal Menu opens.
     *
     * Creates TextField overlay with configured position and styling.
     * Stores references to GFx objects and journal menu.
     */
    void OnJournalOpen();

    /**
     * @brief Cleanup when Journal Menu closes.
     *
     * Clears GFx object references and resets state tracking.
     */
    void OnJournalClose();

    /**
     * @brief Update TextField to reflect note status for current quest.
     * @param questID The currently selected quest FormID (0 = none selected)
     * @param forceUpdate If true, update even if quest hasn't changed
     */
    void UpdateTextField(RE::FormID questID, bool forceUpdate = false);

private:
    JournalNoteHelper() = default;

    /**
     * Checks if the helper is properly initialized and ready to use.
     * @return true if TextField and menu references are valid
     */
    bool IsInitialized() const {
        return noteTextField_.IsObject() && journalMenu_ != nullptr;
    }

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
            createArgs[0].SetString("questNoteTextField");           // name
            createArgs[1].SetNumber(UIConstants::TEXTFIELD_TOP_DEPTH);   // VERY high depth to be on absolute top
            createArgs[2].SetNumber(settings->textFieldX);           // TOP-LEFT x position
            createArgs[3].SetNumber(settings->textFieldY);           // TOP-LEFT y position
            createArgs[4].SetNumber(UIConstants::TEXTFIELD_DEFAULT_WIDTH);   // width
            createArgs[5].SetNumber(UIConstants::TEXTFIELD_DEFAULT_HEIGHT);  // height

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
    if (!IsInitialized()) {
        return; // Helper not properly initialized
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
            bool isJournalOpen = IsJournalCurrentlyOpen();

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
                bool inJournal = IsJournalCurrentlyOpen();
                uint32_t keyCode = buttonEvent->idCode;

                if (inJournal) {
                    // Update TextField on navigation RELEASE (arrow keys, mouse clicks)
                    // Use IsUp() so Journal processes the input first, then we read the updated selection
                    if (buttonEvent->IsUp()) {
                        if (keyCode == KeyCodes::ARROW_UP ||
                            keyCode == KeyCodes::ARROW_DOWN ||
                            keyCode == KeyCodes::MOUSE_LEFT) {
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

                // Quick access hotkey - list all notes (outside journal only)
                if (buttonEvent->IsDown() && keyCode == SettingsManager::GetSingleton()->quickAccessScanCode) {
                    if (!inJournal) {
                        // Show list of all notes
                        PapyrusBridge::ShowNotesListMenu();
                    }
                }
            }
            // Handle mouse move events (hover detection in Journal)
            else if (eventType == RE::INPUT_EVENT_TYPE::kMouseMove) {
                if (IsJournalCurrentlyOpen()) {
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

    /**
     * Helper to check if journal menu is currently open.
     * @return true if Journal Menu is open
     */
    [[nodiscard]] bool IsJournalCurrentlyOpen() const {
        auto ui = RE::UI::GetSingleton();
        return ui && ui->IsMenuOpen("Journal Menu");
    }

    void OnQuestNoteHotkey() {
        // MUST be in Journal Menu
        if (!IsJournalCurrentlyOpen()) {
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
// Papyrus Bridge Utilities
//=============================================================================

/**
 * Converts Papyrus int32 to FormID.
 * Papyrus VM represents FormIDs as signed int32, but the engine uses uint32.
 * High-value FormIDs (e.g., 0xFE000000+ for modded forms) become negative.
 * This performs proper reinterpretation of the bit pattern.
 *
 * @param papyrusID The signed int32 from Papyrus
 * @return The properly converted FormID
 */
inline RE::FormID PapyrusIntToFormID(std::int32_t papyrusID) {
    return static_cast<RE::FormID>(static_cast<std::uint32_t>(papyrusID));
}

//=============================================================================
// Papyrus Bridge
//=============================================================================

namespace PapyrusBridge {
    /**
     * @brief Show quest note input dialog.
     * @param questID The quest FormID to associate the note with
     *
     * Called from C++ InputHandler when hotkey pressed in Journal Menu.
     * Displays Extended Vanilla Menus text input with existing note content.
     */
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

    /**
     * @brief Save quest note callback (called from Papyrus).
     * @param questIDSigned Quest FormID as signed int32 (Papyrus representation)
     * @param noteText Note text from user input
     *
     * Native function registered for Papyrus. Converts FormID and saves note.
     */
    void SaveQuestNote(RE::StaticFunctionTag*, std::int32_t questIDSigned, RE::BSFixedString noteText) {
        // Convert Papyrus int32 to FormID (handles modded forms with high FormIDs)
        RE::FormID questID = PapyrusIntToFormID(questIDSigned);

        if (questID == 0) {
            spdlog::warn("[NOTE] Invalid quest ID");
            return;
        }

        std::string text{noteText.c_str()};
        NoteManager::GetSingleton()->SaveNoteForQuest(questID, text);

        // Update TextField to reflect new note state immediately (force update even if same quest)
        JournalNoteHelper::GetSingleton()->UpdateTextField(questID, true);

        RE::DebugNotification("Quest note saved!");
    }

    /**
     * @brief Show general note input dialog.
     *
     * Called from C++ InputHandler when hotkey pressed during gameplay (not in Journal).
     * Displays Extended Vanilla Menus text input with existing general note content.
     */
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
            RE::BSFixedString(""),           // questName (empty for general)
            RE::BSFixedString(existingText),
            static_cast<std::int32_t>(settings->textInputWidth),
            static_cast<std::int32_t>(settings->textInputHeight),
            static_cast<std::int32_t>(settings->textInputFontSize),
            static_cast<std::int32_t>(settings->textInputAlignment)
        );

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        vm->DispatchStaticCall("PersonalNotes", "ShowGeneralNoteInput", args, callback);
    }

    /**
     * @brief Save general note callback (called from Papyrus).
     * @param noteText Note text from user input
     *
     * Native function registered for Papyrus. Saves general note not tied to any quest.
     */
    void SaveGeneralNote(RE::StaticFunctionTag*, RE::BSFixedString noteText) {
        std::string text(noteText.c_str());
        NoteManager::GetSingleton()->SaveGeneralNote(text);

        RE::DebugNotification("General note saved!");
    }

    /**
     * @brief Show list menu of all saved notes (quick access).
     *
     * Called from C++ InputHandler when quick access hotkey pressed.
     * Retrieves all notes and displays them in a selectable list menu.
     */
    void ShowNotesListMenu() {
        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            spdlog::error("[PAPYRUS] Failed to get VM");
            return;
        }

        // Get all notes
        auto notes = NoteManager::GetSingleton()->GetAllNotes();
        if (notes.empty()) {
            RE::DebugNotification("No notes saved");
            return;
        }

        // Build arrays for Papyrus
        std::vector<RE::BSFixedString> questNames;
        std::vector<RE::BSFixedString> notePreviews;
        std::vector<RE::BSFixedString> noteTexts;
        std::vector<std::int32_t> questIDs;

        for (const auto& [questID, note] : notes) {
            // Quest name
            if (questID == NoteManager::GENERAL_NOTE_ID) {
                questNames.push_back(RE::BSFixedString("General Note"));
            } else {
                auto quest = RE::TESForm::LookupByID<RE::TESQuest>(questID);
                std::string questName = quest ? quest->GetName() : "Unknown Quest";
                questNames.push_back(RE::BSFixedString(questName));
            }

            // Note preview (first 50 chars for list display)
            std::string preview = note.text.length() > 50
                ? note.text.substr(0, 50) + "..."
                : note.text;
            notePreviews.push_back(RE::BSFixedString(preview));

            // Full note text (for editing)
            noteTexts.push_back(RE::BSFixedString(note.text));

            // Quest ID
            questIDs.push_back(static_cast<std::int32_t>(questID));
        }

        // Get TextInput settings
        auto settings = SettingsManager::GetSingleton();

        // Call Papyrus to show list menu
        auto args = RE::MakeFunctionArguments(
            std::move(questNames),
            std::move(notePreviews),
            std::move(noteTexts),
            std::move(questIDs),
            static_cast<std::int32_t>(settings->textInputWidth),
            static_cast<std::int32_t>(settings->textInputHeight),
            static_cast<std::int32_t>(settings->textInputFontSize),
            static_cast<std::int32_t>(settings->textInputAlignment)
        );
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

        vm->DispatchStaticCall("PersonalNotes", "ShowNotesListMenu", args, callback);
    }

    /**
     * @brief Register native Papyrus functions.
     * @param vm Papyrus virtual machine
     * @return true on success
     *
     * Registers SaveQuestNote and SaveGeneralNote as native functions
     * callable from Papyrus scripts.
     */
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
    spdlog::info("PersonalNotes v{}.{}.{} initialized",
                 PERSONAL_NOTES_VERSION_MAJOR,
                 PERSONAL_NOTES_VERSION_MINOR,
                 PERSONAL_NOTES_VERSION_PATCH);
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

    spdlog::info("PersonalNotes v{}.{}.{} loading...",
                 PERSONAL_NOTES_VERSION_MAJOR,
                 PERSONAL_NOTES_VERSION_MINOR,
                 PERSONAL_NOTES_VERSION_PATCH);
    spdlog::info("SKSE version: {}.{}.{}",
        skse->RuntimeVersion().major(),
        skse->RuntimeVersion().minor(),
        skse->RuntimeVersion().patch());

    SKSE::RegisterForAPIInitEvent(InitializePlugin);

    return true;
}
