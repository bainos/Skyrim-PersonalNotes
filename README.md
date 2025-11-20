# Personal Notes

**An in-game note-taking system for Skyrim SE/AE**

Never forget quest details, item locations, or your personal reminders again. Personal Notes lets you write and manage notes directly in-game, with full backup support.

---

## Features

### Quest Notes
- **Press `,` (comma) while viewing a quest** in the Journal Menu to add or edit a note
- Notes are tied to specific quests - perfect for tracking objectives, locations, or tips
- Visual indicator shows "Press , to add note" or "Press , to edit note"

### General Notes
- **Press `,` (comma) during gameplay** to create notes not tied to any quest
- Great for shopping lists, general reminders, or anything else
- Access from anywhere in the game world

### Quick Access & Backup
- **Press `.` (dot) during gameplay** to view all your notes in one list
- Browse all quest and general notes with previews
- **Export all notes to JSON** with one click from the quick access menu
- Automatic backup with timestamp and character name: `Dragonborn_notes_2025-11-17_14-30-00.json`
- Import notes from backup to restore or transfer to another character

### Persistence
- Notes automatically save with your game (SKSE co-save system)
- Each character has separate notes
- Notes persist across all your saves for that character
- Never lose your notes when loading or saving

---

## Usage

### Taking Notes
1. **For Quest Notes**: Open Journal Menu > Select a quest > Press `,` (comma)
2. **For General Notes**: During gameplay > Press `,` (comma)
3. Type your note in the text dialog
4. Submit to save (or leave blank to delete)

### Viewing All Notes
1. Press `.` (dot) during gameplay
2. Browse the list with previews
3. Select a note to edit it
4. Or select "--- Export All Notes ---" to create a backup

### Backup & Restore
**Export:**
- Press `.` (dot) > Select "--- Export All Notes ---"
- Backup saved to: `Data/SKSE/Plugins/PersonalNotes/backup/`
- Filename format: `<CharacterName>_notes_<timestamp>.json`

**Import:**
- Copy a backup JSON file to: `Data/SKSE/Plugins/PersonalNotes/import/notes.json`
- Load any save > notes automatically imported and merged
- Import file is deleted after successful import

---

## Requirements

- **Skyrim SE/AE** (1.5.97 or 1.6.x)
- **SKSE64** ([Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/30379))
- **Address Library for SKSE** ([Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/32444))
- **Extended Vanilla Menus** ([Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/107512))

---

## Installation

1. Install requirements (SKSE, Address Library, Extended Vanilla Menus)
2. Install Personal Notes with your mod manager
3. Launch game via SKSE

**Manual Installation:**
- Extract to `Data/` folder
- Files go to: `Data/SKSE/Plugins/`, `Data/Scripts/`

---

## Configuration

Edit `Data/SKSE/Plugins/PersonalNotes.ini` to customize:

### Hotkeys
```ini
[Hotkey]
iScanCode=51              ; Comma key (add/edit notes)
iQuickAccessScanCode=52   ; Dot key (quick access menu)
```

### UI Customization
```ini
[TextField]
fPositionX=5.0            ; Journal notification X position
fPositionY=5.0            ; Journal notification Y position
iFontSize=20              ; Font size for journal notification
iTextColor=0xFFFFFF       ; Text color (hex RGB)

[TextInput]
iWidth=500                ; Note editor width
iHeight=400               ; Note editor height
iFontSize=14              ; Note editor font size
iAlignment=0              ; 0=left, 1=center, 2=right
```

**Scan codes**: [DirectX Scan Code Reference](https://www.creationkit.com/index.php?title=Input_Script#DXScanCodes)

---

## File Locations

- **Plugin**: `Data/SKSE/Plugins/PersonalNotes.dll`
- **Config**: `Data/SKSE/Plugins/PersonalNotes.ini`
- **Logs**: `Data/SKSE/Plugins/PersonalNotes/PersonalNotes.log`
- **Backups**: `Data/SKSE/Plugins/PersonalNotes/backup/`
- **Import**: `Data/SKSE/Plugins/PersonalNotes/import/notes.json`
- **Saves**: Notes stored in SKSE co-save (`.skse` files alongside your saves)

---

## Tips

- **Empty notes are deleted** - leave the text blank and submit to remove a note
- **Backup regularly** - use quick access to export your notes
- **Transfer notes** - export from one character, import to another
- **Each character is isolated** - starting a new game gives you a fresh slate
- **Notes are per-save** - each character's saves share the same notes

---

## Compatibility

- **No conflicts** - uses SKSE co-save system, no ESP/ESM file
- **Safe to install/uninstall** mid-playthrough
- Works with any quest mod or load order

**Note**: Some mods with hotkeys (e.g., Wheeler) may still trigger when editing notes. This is because those mods don't check for modal dialogs. Personal Notes blocks its own hotkeys correctly when the note editor is open.

---

## Source Code

Available on GitHub: [Personal Notes](https://github.com/yourusername/Skyrim-PersonalNotes)

---

## Credits

Built with:
- **CommonLibSSE-NG** - SKSE plugin framework
- **Extended Vanilla Menus** - UI dialogs and menus

---

## Support

- **Issues**: Report on [Nexus Mods](https://www.nexusmods.com/skyrimspecialedition/mods/XXXXX) or [GitHub](https://github.com/yourusername/Skyrim-PersonalNotes/issues)
- **Logs**: Check `Data/SKSE/Plugins/PersonalNotes/PersonalNotes.log` for errors

## Open Source

This mod is fully open source. The project consists of multiple repositories:

- **[Skyrim-Modding](https://github.com/bainos/Skyrim-Modding)** - Main project repository with all components as submodules
- **[Skyrim-ENBNighteyeFix](https://github.com/bainos/Skyrim-ENBNighteyeFix)** - SKSE plugin and ENB shader patch
- **[commonlibsse-ng-ae-vcpkg](https://github.com/bainos/commonlibsse-ng-ae-vcpkg)** - Custom vcpkg registry with enb-api port
- **[enb-api](https://github.com/bainos/enb-api)** - ENB Series SDK with CMake configuration
- **[CommonLibSSE-NG](https://github.com/bainos/CommonLibSSE-NG)** - Fork updated to work with current dependency libraries (October 2025)
