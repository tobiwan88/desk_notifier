# Haiku: ERC/DRC triage

Input: a JSON list of KiCad ERC/DRC violations ({file}). For each, classify:
- "auto_fix": a mechanical fix exists (move a label, widen a track, refill a zone); give the fix.
- "judgment": needs a design decision; say what the decision is.
- "waiver": acceptable as-is; give the reason a reviewer would accept.
JSON only: [{"type": "...", "items": [...], "class": "...", "fix_or_reason": "..."}]
Never classify a clearance, short, unconnected or hole error as "waiver".
