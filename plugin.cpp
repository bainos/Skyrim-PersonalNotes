#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <set>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>

#include "ENBSeriesAPI.h"
#include <iniparser/iniparser.h>

// Configuration
struct Config {
    std::set<RE::FormID> nightEyeSpellFormIDs;
    std::set<RE::FormID> nightEyeMGEFFormIDs;
    std::set<RE::FormID> nightEyeDispelMGEFFormIDs;

    static Config* GetSingleton() {
        static Config instance;
        return &instance;
    }

    void Load() {
        const char* iniPath = "Data/SKSE/Plugins/ENBNighteyeFix.ini";
        dictionary* ini = iniparser_load(iniPath);

        if (ini) {
            spdlog::info("Loading config from {}", iniPath);

            // Enumerate all keys in [General] section
            int nkeys = iniparser_getsecnkeys(ini, "general");
            if (nkeys > 0) {
                const char** keys = (const char**)malloc(nkeys * sizeof(const char*));
                if (keys) {
                    iniparser_getseckeys(ini, "general", keys);

                    for (int i = 0; i < nkeys; ++i) {
                        const char* key = keys[i];
                        const char* value = iniparser_getstring(ini, key, nullptr);

                        if (value) {
                            // Parse hex FormID
                            unsigned int formID = 0;
                            if (sscanf(value, "0x%X", &formID) == 1 || sscanf(value, "%X", &formID) == 1) {
                                nightEyeSpellFormIDs.insert(static_cast<RE::FormID>(formID));

                                // Extract spell name from key (after "general:")
                                const char* spellName = strchr(key, ':');
                                spellName = spellName ? spellName + 1 : key;
                                spdlog::info("  {} = 0x{:08X}", spellName, formID);
                            } else {
                                spdlog::warn("  Failed to parse value for key: {}", key);
                            }
                        }
                    }
                    free(keys);
                }
            }

            if (nightEyeSpellFormIDs.empty()) {
                spdlog::warn("  No valid Night Eye spells found, plugin will not function");
            }

            // Parse [MagicEffects] section for MGEF FormIDs
            int nMGEFKeys = iniparser_getsecnkeys(ini, "magiceffects");
            if (nMGEFKeys > 0) {
                spdlog::info("Loading Magic Effects from config:");
                const char** mgefKeys = (const char**)malloc(nMGEFKeys * sizeof(const char*));
                if (mgefKeys) {
                    iniparser_getseckeys(ini, "magiceffects", mgefKeys);

                    for (int i = 0; i < nMGEFKeys; ++i) {
                        const char* key = mgefKeys[i];
                        const char* value = iniparser_getstring(ini, key, nullptr);

                        if (value) {
                            unsigned int formID = 0;
                            if (sscanf(value, "0x%X", &formID) == 1 || sscanf(value, "%X", &formID) == 1) {
                                // Extract the effect name from key
                                const char* effectName = strchr(key, ':');
                                effectName = effectName ? effectName + 1 : key;

                                // Determine if this is a dispel effect based on naming convention
                                if (strstr(effectName, "Dispel") || strstr(effectName, "dispel")) {
                                    nightEyeDispelMGEFFormIDs.insert(static_cast<RE::FormID>(formID));
                                    spdlog::info("  {} (Dispel) = 0x{:08X}", effectName, formID);
                                } else {
                                    nightEyeMGEFFormIDs.insert(static_cast<RE::FormID>(formID));
                                    spdlog::info("  {} (Apply) = 0x{:08X}", effectName, formID);
                                }
                            } else {
                                spdlog::warn("  Failed to parse MGEF value for key: {}", key);
                            }
                        }
                    }
                    free(mgefKeys);
                }
            }

            iniparser_freedict(ini);
        } else {
            spdlog::warn("Config file not found: {}", iniPath);
        }
    }

    void AutoDiscoverMGEFs() {
        spdlog::info("Auto-discovering MGEFs from configured spells...");

        // Only auto-discover if no MGEFs were manually configured in INI
        bool hasManualMGEFs = !nightEyeMGEFFormIDs.empty() || !nightEyeDispelMGEFFormIDs.empty();

        if (hasManualMGEFs) {
            spdlog::info("Manual MGEFs configured in INI, skipping auto-discovery");
            return;
        }

        std::set<RE::FormID> discoveredMGEFs;

        for (auto spellFormID : nightEyeSpellFormIDs) {
            auto spell = RE::TESForm::LookupByID<RE::SpellItem>(spellFormID);
            if (!spell) {
                spdlog::warn("  Failed to lookup spell 0x{:X}", spellFormID);
                continue;
            }

            spdlog::info("  Spell 0x{:X}: found {} effects", spellFormID, spell->effects.size());

            for (auto effect : spell->effects) {
                if (effect && effect->baseEffect) {
                    RE::FormID mgefID = effect->baseEffect->formID;
                    discoveredMGEFs.insert(mgefID);
                    spdlog::info("    - MGEF 0x{:08X}", mgefID);
                }
            }
        }

        if (discoveredMGEFs.empty()) {
            spdlog::warn("No MGEFs discovered from spells!");
        } else {
            nightEyeMGEFFormIDs = discoveredMGEFs;
            spdlog::info("Auto-discovered {} Night Eye MGEFs", discoveredMGEFs.size());
        }

        // Always log final MGEF configuration
        spdlog::info("=== Final MGEF Configuration ===");
        spdlog::info("Night Eye MGEFs ({}):", nightEyeMGEFFormIDs.size());
        for (auto mgef : nightEyeMGEFFormIDs) {
            spdlog::info("  - 0x{:08X}", mgef);
        }
        spdlog::info("Dispel MGEFs ({}):", nightEyeDispelMGEFFormIDs.size());
        for (auto mgef : nightEyeDispelMGEFFormIDs) {
            spdlog::info("  - 0x{:08X}", mgef);
        }
        spdlog::info("================================");
    }
};

// Global ENB API pointer
ENB_API::ENBSDK1001* g_ENB = nullptr;

// Pending Night Eye state to set in callback
bool g_PendingNightEyeState = false;
bool g_HasPendingUpdate = false;

// Flag: We just toggled ON, expecting effect to apply (prevents race condition)
bool g_PendingToggleOn = false;

// Flag: REMOVED event fired, awaiting SpellCast to confirm toggle vs real removal
bool g_PendingRemoval = false;

/**
 * Night Eye state persistence
 */
struct NightEyeState {
    static constexpr std::uint32_t kDataKey = 'BNEF';
    static constexpr std::uint32_t kSerializationVersion = 2;  // Incremented for new data

    enum class LastEvent : std::uint32_t {
        None = 0,
        Equipped = 1,
        Unequipped = 2,
        EffectApplied = 3,
        EffectDispelled = 4
    };

    bool isActive = false;
    LastEvent lastEvent = LastEvent::None;

    static NightEyeState* GetSingleton() {
        static NightEyeState instance;
        return &instance;
    }

    void Save(SKSE::SerializationInterface* intfc) {
        if (!intfc->WriteRecordData(&isActive, sizeof(isActive))) {
            spdlog::error("Failed to write Night Eye isActive state");
            return;
        }
        if (!intfc->WriteRecordData(&lastEvent, sizeof(lastEvent))) {
            spdlog::error("Failed to write Night Eye lastEvent state");
        }
    }

    void Load(SKSE::SerializationInterface* intfc) {
        std::uint32_t type;
        std::uint32_t version;
        std::uint32_t length;

        while (intfc->GetNextRecordInfo(type, version, length)) {
            if (type == kDataKey) {
                if (version == 1) {
                    // Old format: only isActive
                    if (intfc->ReadRecordData(&isActive, sizeof(isActive))) {
                        spdlog::info("Loaded Night Eye state (v1): {}", isActive);
                        lastEvent = LastEvent::None;
                        // Reapply state to ENB
                        g_PendingNightEyeState = isActive;
                        g_HasPendingUpdate = true;
                    }
                } else if (version == kSerializationVersion) {
                    // New format: isActive + lastEvent
                    if (intfc->ReadRecordData(&isActive, sizeof(isActive)) &&
                        intfc->ReadRecordData(&lastEvent, sizeof(lastEvent))) {
                        spdlog::info("Loaded Night Eye state (v2): isActive={}, lastEvent={}",
                            isActive, static_cast<std::uint32_t>(lastEvent));
                        // Reapply state to ENB
                        g_PendingNightEyeState = isActive;
                        g_HasPendingUpdate = true;
                    }
                } else {
                    spdlog::warn("Unknown serialization version: {}", version);
                }
            }
        }
    }

    void Revert(SKSE::SerializationInterface*) {
        isActive = false;
        lastEvent = LastEvent::None;
    }
};

// ENB Callback function
void WINAPI ENBCallback(ENB_SDK::ENBCallbackType calltype) {
    if (calltype == ENB_SDK::ENBCallbackType::ENBCallback_PostLoad ||
        calltype == ENB_SDK::ENBCallbackType::ENBCallback_EndFrame) {

        // Check for real removal by polling actual effect state
        if (g_PendingRemoval) {
            auto player = RE::PlayerCharacter::GetSingleton();
            auto state = NightEyeState::GetSingleton();

            if (player && state->isActive) {
                // Check if player actually has the Night Eye effect
                bool hasActiveEffect = false;
                auto magicTarget = player->AsMagicTarget();

                if (magicTarget) {
                    auto activeEffects = magicTarget->GetActiveEffectList();
                    if (activeEffects) {
                        auto& mgefIDs = Config::GetSingleton()->nightEyeMGEFFormIDs;
                        for (auto& ae : *activeEffects) {
                            if (ae && ae->effect && ae->effect->baseEffect) {
                                if (mgefIDs.find(ae->effect->baseEffect->formID) != mgefIDs.end()) {
                                    hasActiveEffect = true;
                                    break;
                                }
                            }
                        }
                    }
                }

                // If effect is gone AND we're still marked active, it's a real removal
                if (!hasActiveEffect) {
                    spdlog::warn("[POLL] Night Eye effect not found on player - real removal detected");

                    state->isActive = false;
                    state->lastEvent = NightEyeState::LastEvent::EffectDispelled;
                    g_PendingNightEyeState = false;
                    g_HasPendingUpdate = true;
                    g_PendingRemoval = false;  // Clear flag

                    RE::DebugNotification("Night Eye: OFF");
                    spdlog::info("[STATE] After real removal: isActive={}, g_PendingToggleOn={}, g_PendingRemoval={}",
                        state->isActive, g_PendingToggleOn, g_PendingRemoval);
                }
            }
        }

        if (g_HasPendingUpdate && g_ENB) {
            ENB_SDK::ENBParameter param;
            param.Type = ENB_SDK::ENBParameterType::ENBParam_BOOL;
            param.Size = 4;
            *reinterpret_cast<BOOL*>(param.Data) = g_PendingNightEyeState ? TRUE : FALSE;

            if (g_ENB->SetParameter(nullptr, "ENBEFFECT.FX", "KNActive", &param)) {
                spdlog::info("ENB: KNActive = {}", g_PendingNightEyeState);
                g_HasPendingUpdate = false;
            }
        }
    }
}

void SetupLog() {
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("BainosNighteyeFix.log", true);
    auto logger = std::make_shared<spdlog::logger>("log", std::move(sink));
    spdlog::set_default_logger(std::move(logger));
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    spdlog::info("BainosNighteyeFix v16 - Auto-Discovery MGEF");
}

/**
 * Handler 1: Detects when Night Eye power is equipped/unequipped (logging only)
 */
class EquipHandler : public RE::BSTEventSink<RE::TESEquipEvent> {
public:
    static EquipHandler* Get() {
        static EquipHandler instance;
        return &instance;
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESEquipEvent* event,
        RE::BSTEventSource<RE::TESEquipEvent>*) override {

        if (!event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player || event->actor.get() != player) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto& spellIDs = Config::GetSingleton()->nightEyeSpellFormIDs;
        if (spellIDs.find(event->baseObject) != spellIDs.end()) {
            auto state = NightEyeState::GetSingleton();

            if (event->equipped) {
                spdlog::info("[EQUIP] Night Eye power EQUIPPED (FormID: 0x{:X})", event->baseObject);
                RE::DebugNotification("Night Eye: EQUIPPED");

                // Update state tracking only (no ENB trigger)
                state->lastEvent = NightEyeState::LastEvent::Equipped;
            }
            else {
                spdlog::info("[UNEQUIP] Night Eye power UNEQUIPPED (FormID: 0x{:X})", event->baseObject);
                RE::DebugNotification("Night Eye: UNEQUIPPED");

                // Update state tracking only (no ENB trigger)
                state->lastEvent = NightEyeState::LastEvent::Unequipped;
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};

/**
 * Handler 2: Detects when Night Eye spell is cast (z key pressed)
 */
class SpellCastHandler : public RE::BSTEventSink<RE::TESSpellCastEvent> {
public:
    static SpellCastHandler* Get() {
        static SpellCastHandler instance;
        return &instance;
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESSpellCastEvent* event,
        RE::BSTEventSource<RE::TESSpellCastEvent>*) override {

        if (!event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        spdlog::info("[DEBUG] TESSpellCastEvent fired - Spell: 0x{:X}", event->spell);

        auto player = RE::PlayerCharacter::GetSingleton();
        auto caster = event->object ? event->object.get() : nullptr;

        if (!player || !caster || caster != player) {
            spdlog::info("[DEBUG] Not player cast, ignoring");
            return RE::BSEventNotifyControl::kContinue;
        }

        spdlog::info("[DEBUG] Player cast detected - Spell: 0x{:X}", event->spell);

        // Check if this is one of our configured Night Eye spells
        auto& spellIDs = Config::GetSingleton()->nightEyeSpellFormIDs;
        if (spellIDs.find(event->spell) == spellIDs.end()) {
            spdlog::info("[DEBUG] Spell 0x{:X} not in configured list", event->spell);
            return RE::BSEventNotifyControl::kContinue;
        }

        spdlog::info("[DEBUG] Spell 0x{:X} is configured Night Eye spell", event->spell);

        // Lookup the spell to check its effects
        auto spell = RE::TESForm::LookupByID<RE::SpellItem>(event->spell);
        if (!spell) {
            spdlog::error("[DEBUG] Failed to lookup spell 0x{:X}", event->spell);
            return RE::BSEventNotifyControl::kContinue;
        }

        spdlog::info("[DEBUG] Spell lookup successful, checking {} effects", spell->effects.size());

        // Check if spell contains any Night Eye MGEFs
        auto& mgefIDs = Config::GetSingleton()->nightEyeMGEFFormIDs;
        spdlog::info("[DEBUG] Checking against {} configured MGEFs", mgefIDs.size());
        bool hasNightEyeMGEF = false;

        for (auto effect : spell->effects) {
            if (effect && effect->baseEffect) {
                spdlog::info("[DEBUG] Spell effect MGEF: 0x{:X}", effect->baseEffect->formID);

                if (mgefIDs.find(effect->baseEffect->formID) != mgefIDs.end()) {
                    hasNightEyeMGEF = true;
                    spdlog::info("[SPELL CAST] Night Eye spell cast (Spell: 0x{:X}, MGEF: 0x{:X})",
                        event->spell, effect->baseEffect->formID);
                    break;
                }
            }
        }

        if (!hasNightEyeMGEF) {
            spdlog::warn("[DEBUG] No matching Night Eye MGEF found in spell effects");
        }

        if (hasNightEyeMGEF) {
            auto state = NightEyeState::GetSingleton();

            // Check if REMOVED event just fired (toggle confirmation)
            if (g_PendingRemoval) {
                spdlog::info("[SPELL CAST] Confirmed toggle - REMOVED event preceded SpellCast");
                g_PendingRemoval = false;  // Clear flag
            }

            // Log current state machine
            spdlog::info("[STATE] Before toggle: isActive={}, g_PendingToggleOn={}, g_PendingRemoval={}",
                state->isActive, g_PendingToggleOn, g_PendingRemoval);

            // Toggle behavior: Night Eye is a toggle power
            // isActive reflects state BEFORE this toggle (unchanged by REMOVED)
            bool newState = !state->isActive;

            spdlog::info("[SPELL CAST] Toggling Night Eye: {} -> {}", state->isActive, newState);
            RE::DebugNotification(newState ? "Night Eye: Activating..." : "Night Eye: Deactivating...");

            // Toggle ENB effect
            state->isActive = newState;
            state->lastEvent = NightEyeState::LastEvent::EffectApplied;
            g_PendingNightEyeState = newState;
            g_HasPendingUpdate = true;

            // If toggling ON, set flag to ignore old effect removal
            if (newState) {
                g_PendingToggleOn = true;
                spdlog::info("[SPELL CAST] Set PendingToggleOn flag, awaiting effect confirmation");
            }

            // Log new state machine
            spdlog::info("[STATE] After toggle: isActive={}, g_PendingToggleOn={}, g_PendingRemoval={}",
                state->isActive, g_PendingToggleOn, g_PendingRemoval);
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};

/**
 * Handler 3: Detects when active effects are applied/removed
 */
class ActiveEffectHandler : public RE::BSTEventSink<RE::TESActiveEffectApplyRemoveEvent> {
public:
    static ActiveEffectHandler* Get() {
        static ActiveEffectHandler instance;
        return &instance;
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESActiveEffectApplyRemoveEvent* event,
        RE::BSTEventSource<RE::TESActiveEffectApplyRemoveEvent>*) override {

        if (!event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto player = RE::PlayerCharacter::GetSingleton();
        auto target = event->target ? event->target.get() : nullptr;

        if (!player || !target || target != player) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Get player's active effects list
        auto magicTarget = player->GetMagicTarget();
        if (!magicTarget) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto activeEffects = magicTarget->GetActiveEffectList();
        if (!activeEffects) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Find the active effect with matching unique ID
        auto& mgefIDs = Config::GetSingleton()->nightEyeMGEFFormIDs;

        for (auto ae : *activeEffects) {
            if (ae && ae->usUniqueID == event->activeEffectUniqueID) {
                // Check if this is a Night Eye effect
                if (ae->effect && ae->effect->baseEffect) {
                    auto mgefFormID = ae->effect->baseEffect->formID;

                    if (mgefIDs.find(mgefFormID) != mgefIDs.end()) {
                        auto state = NightEyeState::GetSingleton();

                        // Log state machine on entry
                        spdlog::info("[STATE] ActiveEffect event: isActive={}, g_PendingToggleOn={}, isApplied={}",
                            state->isActive, g_PendingToggleOn, event->isApplied);

                        if (event->isApplied) {
                            // Effect confirmed
                            if (g_PendingToggleOn) {
                                spdlog::info("[ACTIVE EFFECT] Night Eye effect APPLIED - Confirmed ON (MGEF: 0x{:X})", mgefFormID);
                                RE::DebugNotification("Night Eye: ON");
                            } else if (!state->isActive) {
                                // Toggle OFF: effect applied but we're in OFF state
                                spdlog::info("[ACTIVE EFFECT] Night Eye effect APPLIED - Confirmed OFF (MGEF: 0x{:X})", mgefFormID);
                                RE::DebugNotification("Night Eye: OFF");
                            } else {
                                // Unknown case - just log
                                spdlog::info("[ACTIVE EFFECT] Night Eye effect APPLIED (MGEF: 0x{:X})", mgefFormID);
                            }
                        } else {
                            // Effect removed
                            if (g_PendingToggleOn) {
                                // Old effect removal during toggle ON - ignore and clear flag
                                spdlog::info("[ACTIVE EFFECT] Night Eye effect REMOVED - Ignoring (old effect, pending toggle ON) (MGEF: 0x{:X})", mgefFormID);
                                g_PendingToggleOn = false;
                                spdlog::info("[STATE] After clearing flag: isActive={}, g_PendingToggleOn={}", state->isActive, g_PendingToggleOn);
                            } else {
                                // Potential removal - set pending flag, don't change state yet
                                // Could be toggle OFF (SpellCast will follow) or real removal (polling will detect)
                                g_PendingRemoval = true;
                                spdlog::info("[ACTIVE EFFECT] Night Eye effect REMOVED - Pending removal detected, awaiting toggle confirmation (MGEF: 0x{:X})", mgefFormID);
                                spdlog::info("[STATE] Pending removal: isActive={}, g_PendingToggleOn={}, g_PendingRemoval={}",
                                    state->isActive, g_PendingToggleOn, g_PendingRemoval);
                            }
                        }
                        break;
                    }
                }
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};

/**
 * Check for state desync between serialized data and actual in-game effects
 */
void CheckForStateDesync() {
    auto player = RE::PlayerCharacter::GetSingleton();
    auto state = NightEyeState::GetSingleton();
    auto config = Config::GetSingleton();

    if (!player) {
        spdlog::error("Player not available for desync check");
        return;
    }

    // Check if player actually has any Night Eye effect active
    bool hasActiveNightEye = false;
    auto magicTarget = player->AsMagicTarget();

    if (magicTarget) {
        auto activeEffects = magicTarget->GetActiveEffectList();
        if (activeEffects) {
            for (auto& ae : *activeEffects) {
                if (ae && ae->effect && ae->effect->baseEffect) {
                    auto mgefFormID = ae->effect->baseEffect->formID;
                    if (config->nightEyeMGEFFormIDs.find(mgefFormID) != config->nightEyeMGEFFormIDs.end()) {
                        hasActiveNightEye = true;
                        spdlog::info("[DESYNC CHECK] Found active Night Eye effect (MGEF: 0x{:X})", mgefFormID);
                        break;
                    }
                }
            }
        }
    }

    spdlog::info("[DESYNC CHECK] Serialized isActive={}, Actual hasEffect={}", state->isActive, hasActiveNightEye);

    // If mismatch detected, correct to reality
    if (hasActiveNightEye != state->isActive) {
        spdlog::warn("[DESYNC DETECTED] Correcting state: {} -> {}", state->isActive, hasActiveNightEye);
        state->isActive = hasActiveNightEye;
        g_PendingNightEyeState = hasActiveNightEye;
        g_HasPendingUpdate = true;
        RE::DebugNotification("Night Eye: State corrected (desync detected)");
    } else {
        spdlog::info("[DESYNC CHECK] State is synchronized");
    }
}

void MessageHandler(SKSE::MessagingInterface::Message* msg) {
    switch (msg->type) {
    case SKSE::MessagingInterface::kPostLoad:
    {
        g_ENB = reinterpret_cast<ENB_API::ENBSDK1001*>(
            ENB_API::RequestENBAPI(ENB_API::SDKVersion::V1001));

        if (g_ENB) {
            spdlog::info("ENB API obtained - SDK v{}, ENB v{}",
                g_ENB->GetSDKVersion(), g_ENB->GetVersion());
            g_ENB->SetCallbackFunction(ENBCallback);
        }
        else {
            spdlog::warn("ENB API not available");
        }
        break;
    }

    case SKSE::MessagingInterface::kDataLoaded:
    {
        // Auto-discover MGEFs from configured spells (must happen after forms are loaded)
        Config::GetSingleton()->AutoDiscoverMGEFs();

        auto events = RE::ScriptEventSourceHolder::GetSingleton();
        if (events) {
            events->AddEventSink(EquipHandler::Get());
            events->AddEventSink(SpellCastHandler::Get());
            events->AddEventSink(ActiveEffectHandler::Get());
            spdlog::info("All event handlers registered (Equip, SpellCast, ActiveEffect)");
        } else {
            spdlog::error("Failed to get ScriptEventSourceHolder!");
        }
        break;
    }

    case SKSE::MessagingInterface::kPostLoadGame:
    {
        // Check for state desync after save game loads (must happen after serialization Load)
        CheckForStateDesync();
        break;
    }
    }
}

void InitializePlugin() {
    SetupLog();
    Config::GetSingleton()->Load();

    // Register serialization callbacks
    auto serialization = SKSE::GetSerializationInterface();
    if (serialization) {
        serialization->SetUniqueID(NightEyeState::kDataKey);
        serialization->SetSaveCallback([](SKSE::SerializationInterface* intfc) {
            if (!intfc->OpenRecord(NightEyeState::kDataKey, NightEyeState::kSerializationVersion)) {
                spdlog::error("Failed to open save record");
                return;
            }
            NightEyeState::GetSingleton()->Save(intfc);
            });
        serialization->SetLoadCallback([](SKSE::SerializationInterface* intfc) {
            NightEyeState::GetSingleton()->Load(intfc);
            });
        serialization->SetRevertCallback([](SKSE::SerializationInterface* intfc) {
            NightEyeState::GetSingleton()->Revert(intfc);
            });
        spdlog::info("Serialization registered");
    }

    if (auto messaging = SKSE::GetMessagingInterface()) {
        messaging->RegisterListener(MessageHandler);
        spdlog::info("Messaging registered");
    }

    spdlog::info("Plugin initialized");
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    SKSE::RegisterForAPIInitEvent(InitializePlugin);
    return true;
}