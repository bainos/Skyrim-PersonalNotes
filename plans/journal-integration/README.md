# Journal Integration Research - Quest Detection

## Overview
Complete research on detecting which quest is currently viewed in Skyrim's Journal Menu using CommonLibSSE-NG APIs.

**Status**: FEASIBLE - Very High likelihood of implementation success

## Documents in This Folder

### 1. SUMMARY.txt (Start Here)
Quick overview of findings and recommendations. Contains:
- Executive summary with critical findings
- Feasibility ratings for each approach
- Recommended implementation path
- Next steps and timeline estimates

**Read this first for high-level understanding.**

### 2. RESEARCH_DETAILED.md
Comprehensive technical research. Contains:
- JournalMenu class structure breakdown
- Journal_QuestsTab analysis
- UI singleton access patterns
- Four different implementation approaches
- TESQuest class for quest data access
- GFxValue API reference for Scaleform
- Challenges and mitigation strategies

**Read this for deep technical understanding.**

### 3. IMPLEMENTATION_GUIDE.md
Practical code examples and patterns. Contains:
- Quick start code snippets
- Three different implementation methods with examples
- Complete SKSE plugin example
- GFxValue API reference with usage patterns
- SWF decompilation checklist
- Troubleshooting guide
- Performance considerations

**Read this when implementing the solution.**

## Key Findings Summary

### What We Can Do
YES - Detect which quest player is viewing in Journal when hotkey pressed

### How
Multiple viable approaches:
1. **UI Singleton Access** (VERY HIGH feasibility) - Get JournalMenu instance
2. **Scaleform Variables** (HIGH feasibility) - Query UI state directly
3. **TESQuest Lookup** (VERY HIGH feasibility) - Get quest data once you have FormID
4. **Direct Struct Access** (HIGH feasibility) - Access Journal_QuestsTab members

### Main Challenge
The exact **Scaleform variable paths** need to be discovered through SWF analysis. This is a one-time reverse engineering task (1-2 hours with JPEXS decompiler).

### Timeline
- Path discovery: 1-2 hours
- Implementation: 2-4 hours
- Testing: 1-2 hours
- **Total: 4-8 hours**

## Critical Files in CommonLibSSE-NG

These are the actual header files containing the APIs:

```
CommonLibSSE-NG/include/RE/J/JournalMenu.h       - Main menu class
CommonLibSSE-NG/include/RE/J/Journal_QuestsTab.h - Quest tab structure
CommonLibSSE-NG/include/RE/U/UI.h                - UI singleton
CommonLibSSE-NG/include/RE/G/GFxMovie.h          - Scaleform API
CommonLibSSE-NG/include/RE/G/GFxValue.h          - Scaleform value access
CommonLibSSE-NG/include/RE/T/TESQuest.h          - Quest data
CommonLibSSE-NG/include/RE/T/TESForm.h           - Form lookup
```

All located in: `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/`

## Implementation Recommendations

### Immediate (Step 1)
1. Read SUMMARY.txt for overview
2. Review GFxValue API in IMPLEMENTATION_GUIDE.md
3. Extract Journal.swf from Skyrim/Data/Interface/
4. Use JPEXS Free Flash Decompiler to find quest variables

### Development (Step 2)
1. Create test SKSE plugin with hotkey hook
2. Use example code from IMPLEMENTATION_GUIDE.md
3. Substitute discovered Scaleform paths
4. Add logging and error handling

### Testing (Step 3)
1. Test with various quest states
2. Verify across different menu positions
3. Check performance impact

## Example Code

### Get Current Quest (Quick Version)
```cpp
auto ui = RE::UI::GetSingleton();
auto journalMenu = ui->GetMenu<RE::JournalMenu>();
if (!journalMenu) return nullptr;

auto movieView = journalMenu->uiMovie;
if (!movieView) return nullptr;

RE::GFxValue questID;
// [Insert discovered Scaleform path here]
if (movieView->GetVariable(&questID, "[PATH]"))
{
    FormID id = (FormID)questID.GetNumber();
    return RE::TESForm::LookupByID<RE::TESQuest>(id);
}
```

## Next Actions

1. **THIS WEEK**: Discover Scaleform paths through SWF analysis
2. **NEXT WEEK**: Implement basic test plugin
3. **FOLLOWING WEEK**: Full implementation and testing

## Questions or Issues?

Refer to the appropriate document:
- Confused about overall approach? Read SUMMARY.txt
- Need technical details? Read RESEARCH_DETAILED.md
- Need code examples? Read IMPLEMENTATION_GUIDE.md
- Getting errors? Check IMPLEMENTATION_GUIDE.md troubleshooting section

## References

All source file paths are absolute paths in `/mnt/c/dev/Skyrim-Modding/CommonLibSSE-NG/include/RE/`

See each document for specific file references and line numbers.

---

**Last Updated**: November 13, 2025
**Status**: Research Complete - Ready for Implementation Phase
**Confidence Level**: Very High (95%+ feasibility)
