Scriptname PersonalNotesNative Hidden

; Native functions implemented in C++

; Called from PersonalNotes.psc to save quest note
; If noteText is empty, the note is deleted
Function SaveQuestNote(int questID, string noteText) Global Native

; Called from PersonalNotes.psc to save general note
; If noteText is empty, the note is deleted
Function SaveGeneralNote(string noteText) Global Native
