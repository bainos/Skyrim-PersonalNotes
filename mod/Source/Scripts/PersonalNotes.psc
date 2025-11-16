Scriptname PersonalNotes Hidden

; Called from C++ when comma pressed in Journal Menu
; Parameters:
;   questID - FormID of the quest
;   questName - Display name of the quest
;   existingText - Current note text (empty string if no note exists)
;   width, height, fontSize, alignment - TextInput settings from INI
Function ShowQuestNoteInput(int questID, string questName, string existingText, int width, int height, int fontSize, int alignment) Global
    ; Build prompt with quest name
    String prompt = "Note for: " + questName

    ; Show text input (pre-filled with existing text if any)
    String result = ExtendedVanillaMenus.TextInput(prompt, existingText, Width = width, Height = height, align = alignment, FontSize = fontSize)

    ; Filter out special cancel string from ExtendedVanillaMenus
    If result == "EVM_TextInput_Cancelled"
        ; User cancelled - do nothing
        Return
    EndIf

    ; Save the note (empty result = delete)
    PersonalNotesNative.SaveQuestNote(questID, result)
EndFunction

; Called from C++ when comma pressed during gameplay (not in Journal)
; Parameters:
;   questName - Empty string for general notes
;   existingText - Current general note text (empty string if no note exists)
;   width, height, fontSize, alignment - TextInput settings from INI
Function ShowGeneralNoteInput(string questName, string existingText, int width, int height, int fontSize, int alignment) Global
    ; Show text input with "Personal Notes" prompt
    String result = ExtendedVanillaMenus.TextInput("Personal Notes", existingText, Width = width, Height = height, align = alignment, FontSize = fontSize)

    ; Filter out special cancel string from ExtendedVanillaMenus
    If result == "EVM_TextInput_Cancelled"
        ; User cancelled - do nothing
        Return
    EndIf

    ; Save the general note (empty result = delete)
    PersonalNotesNative.SaveGeneralNote(result)
EndFunction
