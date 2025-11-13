Scriptname PersonalNotesNative Hidden

; Native function implemented in C++

; Called from PersonalNotes.psc to save quest note
; If noteText is empty, the note is deleted
Function SaveQuestNote(int questID, string noteText) Global Native
