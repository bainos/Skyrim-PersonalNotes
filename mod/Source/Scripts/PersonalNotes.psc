Scriptname PersonalNotes Hidden

; Called from C++ when comma hotkey pressed
Function RequestInput() Global
    ; Show text input via Extended Vanilla Menus (synchronous call)
    String result = ExtendedVanillaMenus.TextInput("Enter Note:", "")

    ; If user entered text, send to C++
    If result != ""
        PersonalNotesNative.OnNoteReceived(result)
    EndIf
EndFunction

; Called from C++ when dot hotkey pressed
Function ShowNotesList() Global
    Int count = PersonalNotesNative.GetNoteCount()

    ; Handle empty notes
    If count == 0
        String[] emptyOptions = New String[1]
        emptyOptions[0] = "OK"
        ExtendedVanillaMenus.MessageBox("No notes yet.\n\nPress comma (,) to add a note.", emptyOptions)
        Return
    EndIf

    ; Build note list for display
    String[] noteOptions = Utility.CreateStringArray(count)
    String[] noteInfos = Utility.CreateStringArray(count)

    Int i = 0
    While i < count
        noteOptions[i] = FormatNoteForList(i)
        noteInfos[i] = PersonalNotesNative.GetNoteText(i)
        i += 1
    EndWhile

    ; Show list menu
    Int selected = ExtendedVanillaMenus.ListMenu(noteOptions, noteInfos, "Personal Notes", "Select", "Cancel")

    ; If user selected a note, show submenu
    If selected >= 0
        ShowNoteSubmenu(selected)
    EndIf
EndFunction

; Format a note for list display
String Function FormatNoteForList(Int index) Global
    String text = PersonalNotesNative.GetNoteText(index)
    String context = PersonalNotesNative.GetNoteContext(index)
    String timestamp = PersonalNotesNative.GetNoteTimestamp(index)

    ; Truncate text to 40 chars
    If StringUtil.GetLength(text) > 40
        text = StringUtil.Substring(text, 0, 40) + "..."
    EndIf

    Return "[" + timestamp + "] " + context + ": " + text
EndFunction

; Show submenu for selected note
Function ShowNoteSubmenu(Int index) Global
    ; Get note data
    String noteText = PersonalNotesNative.GetNoteText(index)
    String noteContext = PersonalNotesNative.GetNoteContext(index)
    String noteTimestamp = PersonalNotesNative.GetNoteTimestamp(index)

    ; Format full note for display
    String fullNote = "[" + noteTimestamp + "] " + noteContext + "\n\n" + noteText

    ; Show options
    String[] options = New String[3]
    options[0] = "Delete Note"
    options[1] = "Back to List"
    options[2] = "Close"

    Int choice = ExtendedVanillaMenus.MessageBox(fullNote, options, True)

    If choice == 0
        ; Delete
        ConfirmDelete(index)
    ElseIf choice == 1
        ; Back to List
        ShowNotesList()
    EndIf
    ; If choice == 2 or -1 (Close/Cancel), just exit
EndFunction

; Confirm deletion of a note
Function ConfirmDelete(Int index) Global
    String[] options = New String[2]
    options[0] = "Cancel"
    options[1] = "Yes, Delete"

    Int choice = ExtendedVanillaMenus.MessageBox("Delete this note?", options, True)

    If choice == 1
        ; User confirmed - delete note
        PersonalNotesNative.DeleteNote(index)
        Debug.Notification("Note deleted")
        ShowNotesList()
    Else
        ; User canceled - return to note view
        ShowNoteSubmenu(index)
    EndIf
EndFunction
