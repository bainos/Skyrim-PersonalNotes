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

; Called from C++ when quick access hotkey pressed (dot key by default)
; Parameters:
;   questNames - Array of quest names (or "General Note" or "--- Export All Notes ---")
;   notePreviews - Array of note text previews (for list display)
;   noteTexts - Array of full note texts
;   questIDs - Array of quest FormIDs (-1 for general note, -2 for export action)
;   width, height, fontSize, alignment - TextInput settings from INI
Function ShowNotesListMenu(string[] questNames, string[] notePreviews, string[] noteTexts, int[] questIDs, int width, int height, int fontSize, int alignment) Global
    ; Show list menu
    Int selectedIndex = ExtendedVanillaMenus.ListMenu(questNames, notePreviews, "Personal Notes", "$Select", "$Cancel")

    ; User cancelled
    If selectedIndex < 0 || selectedIndex >= questNames.Length
        Return
    EndIf

    ; Get selected note details
    Int questID = questIDs[selectedIndex]
    String questName = questNames[selectedIndex]
    String existingText = noteTexts[selectedIndex]

    ; Check for special actions
    If questID == -2
        ; Export all notes action
        PersonalNotesNative.ExportAllNotes()
        Return
    EndIf

    ; Show edit dialog using existing functions
    If questID == -1
        ; General note (FormID 0xFFFFFFFF as signed int32)
        ShowGeneralNoteInput("", existingText, width, height, fontSize, alignment)
    Else
        ; Quest note
        ShowQuestNoteInput(questID, questName, existingText, width, height, fontSize, alignment)
    EndIf
EndFunction
