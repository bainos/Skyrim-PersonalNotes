# Skyrim SE – MCM Helper Tutorial for Managing an INI via MCM  
*Extended version including sliders, color picker, keymap, and dropdown*

---

## 📌 Overview

This tutorial explains how to connect your **SKSE plugin INI settings** to a **Mod Configuration Menu (MCM)** using **MCM Helper**, focusing on the following control types:

- Sliders  
- Color Picker (`color`)  
- Keymap / Hotkey (`keymap`)  
- Dropdown / Enum (`enum`)  

It assumes your SKSE plugin already reads and writes:

```
SKSE/Plugins/PersonalNotes.ini
```

---

## 📁 Folder Structure (Relative to Mod Root)

```
<ModRoot>/
 ├─ SKSE/
 │   └─ Plugins/
 │        └─ PersonalNotes.ini
 ├─ interface/
 │   └─ translations/
 │        └─ PersonalNotes_ENGLISH.txt
 └─ MCM/
     └─ Configs/
         └─ PersonalNotes/
             ├─ config.json
             └─ settings.ini
```

---

## 🧩 Step 1 — Create the MCM `config.json`

**Path:**
```
MCM/Configs/PersonalNotes/config.json
```

```json
{
  "modName": "PersonalNotes",
  "displayName": "Personal Notes",
  "pages": [
    {
      "label": "General",
      "sections": [
        {
          "label": "UI Settings",
          "options": [
            {
              "type": "slider",
              "label": "Font Size",
              "format": "{0}",
              "min": 10,
              "max": 60,
              "step": 1,
              "ini": "PersonalNotes.ini",
              "section": "UI",
              "key": "iFontSize",
              "default": 20
            },
            {
              "type": "slider",
              "label": "Window Width",
              "format": "{0}",
              "min": 200,
              "max": 1200,
              "step": 5,
              "ini": "PersonalNotes.ini",
              "section": "UI",
              "key": "iWindowWidth",
              "default": 600
            },
            {
              "type": "slider",
              "label": "Window Height",
              "format": "{0}",
              "min": 200,
              "max": 1200,
              "step": 5,
              "ini": "PersonalNotes.ini",
              "section": "UI",
              "key": "iWindowHeight",
              "default": 400
            },
            {
              "type": "color",
              "label": "Text Color",
              "ini": "PersonalNotes.ini",
              "section": "UI",
              "key": "iTextColor",
              "default": "0xFFFFFF"
            },
            {
              "type": "enum",
              "label": "Text Alignment",
              "options": [
                { "label": "Left",   "value": 0 },
                { "label": "Center", "value": 1 },
                { "label": "Right",  "value": 2 }
              ],
              "ini": "PersonalNotes.ini",
              "section": "UI",
              "key": "iAlignment",
              "default": 0
            }
          ]
        },
        {
          "label": "Hotkeys",
          "options": [
            {
              "type": "keymap",
              "label": "Open Notes Hotkey",
              "ini": "PersonalNotes.ini",
              "section": "Hotkeys",
              "key": "iOpenHotkey",
              "default": 51
            }
          ]
        }
      ]
    }
  ]
}
```

---

## 🧩 Step 2 — Optional Defaults in `settings.ini`

**Path:**
```
MCM/Configs/PersonalNotes/settings.ini
```

```
[UI]
iFontSize=20
iWindowWidth=600
iWindowHeight=400
iTextColor=0xFFFFFF
iAlignment=0

[Hotkeys]
iOpenHotkey=51
```

---

## 🧩 Step 3 — Translation File

**Path:**
```
interface/translations/PersonalNotes_ENGLISH.txt
```

```
$PersonalNotes_Title Personal Notes
$FontSize Font Size
$WindowWidth Window Width
$WindowHeight Window Height
$TextColor Text Color
$Alignment Text Alignment
$OpenHotkey Open Notes Hotkey
```

---

## 🧩 Step 4 — How Your SKSE Plugin Reads These

Example C++ snippet:

```cpp
CSimpleIniA ini;
ini.SetUnicode();
ini.LoadFile("Data/SKSE/Plugins/PersonalNotes.ini");

int fontSize = ini.GetLongValue("UI", "iFontSize", 20);
int width = ini.GetLongValue("UI", "iWindowWidth", 600);
int height = ini.GetLongValue("UI", "iWindowHeight", 400);

int color = ini.GetLongValue("UI", "iTextColor", 0xFFFFFF);
int align = ini.GetLongValue("UI", "iAlignment", 0);

int hotkey = ini.GetLongValue("Hotkeys", "iOpenHotkey", 51);
```

---

## ✔ Final Notes

You now have:
- Sliders ✔  
- Color picker ✔  
- Keymap support ✔  
- Dropdown (enum) ✔  
- Updated file paths ✔  

This file is your downloadable, updated tutorial.

