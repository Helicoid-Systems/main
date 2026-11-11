# Mac design correction — 20 October 2026

The supplied screen recording exposed weak composition beyond the existing functional blockers: a narrow grouped form inside a large window, a server-address field presented like a settings value, missing setup headings, and technical errors squeezed into a value column.

The correction changes four native presentation files only: `HelicoidManagerApp.swift`, `ConnectionViews.swift`, `GuidedSetupView.swift`, and `SetupWindowChrome.swift`. It preserves the surrounding uncommitted work and supplied artwork.

- Connection uses a small left-aligned identity/heading and a labeled full-width native input. The floating grouped card is gone.
- Verification and sign-in use the same heading alignment. Editable sign-in fields have visible labels above them.
- Hosting uses native radio choices with relevant symbols, a task heading and one short availability note.
- Preflight uses ordinary status rows. Long explanations expand under Details instead of wrapping inside a narrow value column.
- Compact window content sizes follow the actual page: Connect 520×220, hosting choice 520×280, SSH 520×260, checks 520×420. These are presentation sizes, not platform acceptance. Scrolling handles expanded details and the footer remains separate.
- One window geometry owner receives the setup page's height through a SwiftUI preference. Phase resizing preserves the window's top edge and does not use an animation timer.

Verification: arm64 release build passed; the existing 18 Manager presentation/session tests passed; strict ad-hoc package signature and ZIP integrity checks passed. The final packaged Connect starts with its field focused. Both hosting choices and the availability note fit without a scrollbar. The real read-only local check displayed five Passed results and Product Ports Needs Attention; its underlying privilege failure was preserved.

These observations do not establish complete visual/accessibility acceptance. Native captures can show transformed background thumbnails. The correction does not complete connection-profile adoption, authentication, installation, capture or control, and changes no server, trust, worker or authority behavior. No full screen family is promoted to runtime Verified.

[Artifact/source manifest](native-ui-design-correction-2026-10-20.json) records the exact new development ZIP, source patch, hashes and limits. The earlier checkpoint and its hashes remain separate.
