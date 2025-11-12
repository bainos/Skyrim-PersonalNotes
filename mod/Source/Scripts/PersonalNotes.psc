Scriptname PersonalNotes Hidden

; Called from C++ when hotkey pressed
Function RequestInput() Global
    ; Show text input via Extended Vanilla Menus (synchronous call)
    String result = ExtendedVanillaMenus.TextInput("Enter Note:", "")

    ; If user entered text, send to C++
    If result != ""
        PersonalNotesNative.OnNoteReceived(result)
    EndIf
EndFunction
