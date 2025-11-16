# ModSettings
ModSettings are a convenient storage mechanism for mod settings that mods can use to store data.

All mod settings must be defined in MCM\Config\ModName\settings.ini.

User settings live in MCM\Settings\ModName.ini. This file is automatically created and updated by the MCM.

ModSettings are recommended if the setting should persist for the game installation, and if it is desirable for users to share configuration files with others.

The data type for each setting is determined based on the first letter. Always ensure that you are using one of these prefixes.

b: Bool
f: Float
i: Int
r: Color (Comma-separated R,G,B values; Papyrus helper functions available from ColorComponent script)
s: String
u: Unsigned Int

## Example
```
{
  "id": "fSliderValue:Main",
  "text": "Demo Slider",
  "type": "slider",
  "help": "A standard slider with constraints. Range: 1-100, Step: 1.",
  "valueOptions": {
    "min": 1,
    "max": 100,
    "step": 1,
    "sourceType": "ModSettingFloat"
  }
}
```
Papyrus functions are available for retrieving and setting ModSettings, either locally in your config script or globally from the MCM script.

### Config script:

```
; Obtains the value of a mod setting.
int Function GetModSettingInt(string a_settingName) native
bool Function GetModSettingBool(string a_settingName) native
float Function GetModSettingFloat(string a_settingName) native
string Function GetModSettingString(string a_settingName) native

; Sets the value of a mod setting.
Function SetModSettingInt(string a_settingName, int a_value) native
Function SetModSettingBool(string a_settingName, bool a_value) native
Function SetModSettingFloat(string a_settingName, float a_value) native
Function SetModSettingString(string a_settingName, string a_value) native
```

### MCM script:

```
; Obtains the value of a mod setting.
int Function GetModSettingInt(string a_modName, string a_settingName) native global
bool Function GetModSettingBool(string a_modName, string a_settingName) native global
float Function GetModSettingFloat(string a_modName, string a_settingName) native global
string Function GetModSettingString(string a_modName, string a_settingName) native global

; Sets the value of a mod setting.
Function SetModSettingInt(string a_modName, string a_settingName, int a_value) native global
Function SetModSettingBool(string a_modName, string a_settingName, bool a_value) native global
Function SetModSettingFloat(string a_modName, string a_settingName, float a_value) native global
Function SetModSettingString(string a_modName, string a_settingName, string a_value) native global
```