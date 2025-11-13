Scriptname PersonalNotesNative Hidden

; Native functions implemented in C++

; Called from PersonalNotes.psc to send note text to C++
Function OnNoteReceived(string noteText) Global Native

; Get total number of notes
Int Function GetNoteCount() Global Native

; Get note text by index
String Function GetNoteText(int index) Global Native

; Get note context by index
String Function GetNoteContext(int index) Global Native

; Get formatted timestamp by index (MM/DD HH:MM)
String Function GetNoteTimestamp(int index) Global Native

; Delete note by index
Function DeleteNote(int index) Global Native
