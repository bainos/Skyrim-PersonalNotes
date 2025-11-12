# Project Plans

## Active Plans (v2)

**Current project specifications:**

1. **[PROJECT_OVERVIEW_V2.md](PROJECT_OVERVIEW_V2.md)** - Main project overview
   - Simplified design: text input focus only
   - No journal integration
   - 2-phase implementation
   - 7-8 day timeline

2. **[Phase1_PRD_V2.md](Phase1_PRD_V2.md)** - Core note system
   - Hotkey detection
   - Extended Vanilla Menus integration
   - Note storage and persistence
   - 3-5 days

3. **[Phase2_PRD_V2.md](Phase2_PRD_V2.md)** - Display system
   - Message box display
   - Console command
   - Note formatting
   - 2-3 days

4. **[FEASIBILITY_ANALYSIS.md](FEASIBILITY_ANALYSIS.md)** - Technical feasibility study
   - CommonLibSSE-NG capability analysis
   - Identifies blockers and solutions
   - Extended Vanilla Menus discovery

## Deprecated Plans

**Location:** `deprecated/`

Original journal integration design (4 phases, 15-20 days):
- `PROJECT_OVERVIEW.md` - Original ambitious design
- `Phase1_PRD.md` - Foundation
- `Phase2_PRD.md` - Hotkey and input
- `Phase3_PRD.md` - Journal integration (too complex)
- `Phase4_PRD.md` - Virtual quest (not feasible)

**Why deprecated:** Journal integration requires:
- Deep Scaleform hooking
- Quest description modification (read-only BSFixedString)
- Virtual quest creation (no API exists)
- High complexity and fragility

**Decision:** Pivot to simpler text input focused design (v2).

## Reading Order

1. **FEASIBILITY_ANALYSIS.md** - Understand technical constraints
2. **PROJECT_OVERVIEW_V2.md** - Current project goals
3. **Phase1_PRD_V2.md** - Implementation details (Phase 1)
4. **Phase2_PRD_V2.md** - Implementation details (Phase 2)

## Key Decisions

| Decision | Rationale |
|----------|-----------|
| Use Extended Vanilla Menus | Solves text input problem, no SWF needed |
| Skip journal integration | Too complex, fragile, high risk |
| 2-phase design | Simple, achievable in 7-8 days |
| Minimal Papyrus | Only for C++↔EVM bridge |
| Message box display | Simple, effective, no UI work needed |

## Project Status

- **Phase:** Planning complete
- **Next:** Begin Phase 1 implementation
- **Complexity:** 2/10
- **Timeline:** 7-8 days
