---
status: accepted
date: 2026-10-09
---

# main coordinates separate implementation repositories

The user chose main as the work repository with application repositories checked out as submodules. main owns product planning, architecture decisions, integration checks, and a tested release manifest; exact gitlinks pin the implementation versions. Each app must still build independently using versioned shared artifacts or pinned source dependencies, because sibling-relative builds would make the nominally separate repositories depend on this checkout. The initial four-repository layout is proposed, and no submodules have been created yet.
