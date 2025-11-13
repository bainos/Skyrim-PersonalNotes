Scriptname PersonalNotes Hidden

; Called from C++ when comma pressed in Journal Menu
; Parameters:
;   questID - FormID of the quest
;   questName - Display name of the quest
;   existingText - Current note text (empty string if no note exists)
Function ShowQuestNoteInput(int questID, string questName, string existingText) Global
    ; Build prompt with quest name
    String prompt = "Note for: " + questName

    ; Log for debugging
    Debug.Trace("[PersonalNotes] Opening text input for quest " + questID + ", existing text: '" + existingText + "'")

    ; Show text input (pre-filled with existing text if any)
    String result = ExtendedVanillaMenus.TextInput(prompt, existingText)

    ; NOTE: Known issue - keyboard may freeze after EVM closes
    ; Workaround: Open console (~) then close it to restore input
    ; No Papyrus API exists to reset input state programmatically

    ; Filter out special cancel string from ExtendedVanillaMenus
    If result == "EVM_TextInput_Cancelled"
        ; User cancelled - do nothing
        Debug.Trace("[PersonalNotes] User cancelled text input")
        Return
    EndIf

    ; Save the note (empty result = delete)
    Debug.Trace("[PersonalNotes] Saving note: '" + result + "'")
    PersonalNotesNative.SaveQuestNote(questID, result)
EndFunction
