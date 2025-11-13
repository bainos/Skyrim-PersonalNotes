Scriptname PersonalNotes Hidden

; Called from C++ when comma pressed in Journal Menu
; Parameters:
;   questID - FormID of the quest
;   questName - Display name of the quest
;   existingText - Current note text (empty string if no note exists)
Function ShowQuestNoteInput(int questID, string questName, string existingText) Global
    ; Build prompt with quest name
    String prompt = "Note for: " + questName

    ; Show text input (pre-filled with existing text if any)
    String result = ExtendedVanillaMenus.TextInput(prompt, existingText)

    ; Filter out special cancel string from ExtendedVanillaMenus
    If result == "EVM_TextInput_Cancelled"
        ; User cancelled - do nothing
        Return
    EndIf

    ; Save the note (empty result = delete)
    PersonalNotesNative.SaveQuestNote(questID, result)
EndFunction
