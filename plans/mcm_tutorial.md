# Skyrim SE – MCM Helper Tutorial for Managing an INI via MCM  
*A minimal “Hello World” example*

---

## 📌 Overview

This tutorial shows how to connect your existing **SKSE plugin with INI settings** to a **Mod Configuration Menu (MCM)** using **MCM Helper**.  
It assumes:
- You already have a working SKSE DLL.
- Your INI is stored in `Data/SKSE/Plugins/MyPlugin.ini`.
- You want a simple MCM that reads/writes this INI.

This is a minimal, working example focused strictly on **MCM implementation and INI binding**.

---

## 📁 Folder Structure

Your mod should contain:

```
Data/
 ├─ SKSE/
 │   └─ Plugins/
 │        └─ MyPlugin.ini
 ├─ interface/
 │   └─ translations/
 │        └─ MyPlugin_ENGLISH.txt
 └─ MCM/
     └─ Configs/
         └─ MyPlugin/
             ├─ config.json
             └─ settings.ini   (OPTIONAL – default values)
```

---

## 🧩 Step 1 — Create the MCM `config.json`

Create:  
`Data/MCM/Configs/MyPlugin/config.json`

```json
{
  "modName": "MyPlugin",
  "displayName": "My Plugin Settings",
  "pages": [
    {
      "label": "General",
      "sections": [
        {
          "label": "Basic Settings",
          "options": [
            {
              "type": "toggle",
              "label": "Enable Feature",
              "ini": "MyPlugin.ini",
              "section": "Main",
              "key": "bEnableFeature",
              "default": true
            },
            {
              "type": "slider",
              "label": "Feature Strength",
              "format": "{0}",
              "min": 0,
              "max": 100,
              "step": 1,
              "ini": "MyPlugin.ini",
              "section": "Main",
              "key": "iFeatureStrength",
              "default": 50
            }
          ]
        }
      ]
    }
  ]
}
```

### Explanation  
- `ini`: path relative to `Data/SKSE/Plugins/`
- `section`: INI section `[Main]`
- `key`: key inside that section
- MCM Helper automatically **reads & writes** the INI.  
No Papyrus script required.

---

## 🧩 Step 2 — (Optional) Provide default values in MCM `settings.ini`

Create:  
`Data/MCM/Configs/MyPlugin/settings.ini`

```
[Main]
bEnableFeature=1
iFeatureStrength=50
```

MCM Helper will use these defaults if the actual SKSE INI does not exist yet.

---

## 🧩 Step 3 — Add Translation File

Create:  
`Data/interface/translations/MyPlugin_ENGLISH.txt`

```
$MyPlugin_Title   My Plugin Settings
$EnableFeature    Enable Feature
$FeatureStrength  Feature Strength
```

Then in `config.json` use:

```json
"label": "$EnableFeature"
```

(Or stick with literal English text if you don’t want translations.)

---

## 🧩 Step 4 — How SKSE Reads the INI

Your existing SKSE plugin (C++ DLL) can continue reading:

```cpp
CSimpleIniA ini;
ini.SetUnicode();
ini.LoadFile("<path>/MyPlugin.ini");

bool enabled = ini.GetBoolValue("Main", "bEnableFeature", true);
int strength = ini.GetLongValue("Main", "iFeatureStrength", 50);
```

MCM Helper **writes to the same file**, so your plugin will read updated values on game load or when you re-read the INI.

---

## 🧩 Step 5 — (Optional) Force Reload on Menu Save

If your plugin needs immediate application after MCM changes, provide a Papyrus script:

`Data/scripts/source/MyPlugin_MCM.psc`

```papyrus
Scriptname MyPlugin_MCM extends MCM_ConfigBase

Event OnPageReset(string page)
    ; Called when user leaves a page
    MyPluginNative.ReloadINI()
EndEvent
```

But if your plugin re-checks the INI periodically or only at load, this is unnecessary.

---

## ✔ Final Notes

You can now:
- Add toggles, sliders, dropdowns, hotkeys.
- Bind all of them directly to INI entries.
- Let MCM Helper handle all file operations automatically.

This is the cleanest and most minimal workflow for MCM integration with a pre-existing SKSE plugin.
