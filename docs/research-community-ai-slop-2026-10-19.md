# What communities call “AI slop” in apps: posts, images and counterexamples

**Level: L3, expanded under the user's explicit search budget. Date: 19 October 2026. Online provider: Context.dev only.**

## Scope and completed work

**User constraints:** build on the existing repository research; investigate what people on X, Reddit and other platforms say or link as app/software/interface slop; include image examples; perform at least 200 searches, with permission to expand where useful.

**Assumptions:** the purpose is to improve Helicoid's product and interface decisions, rather than identify or shame developers. “Slop” is an unstable community label, not a technical classification. This is research only.

**Completed in this research run:**

- **200 successful searches with 200 distinct query strings and 200 distinct request IDs**, separate from the earlier report's 200-search exercise. Two exploratory searches were also made and are excluded from that main count.
- **2,000 result entries; 1,495 distinct raw URL strings**, or 1,371 after stripping query/fragment and trailing slash. These are discovery counts, not independent opinions or fully read pages.
- **61 selected source resources retrieved:** 27 Reddit pages, 12 X pages and 22 other pages/files. Three completed Context.dev batches retrieved 58 resources; three selected resources were retrieved individually. A CSV page yielded only its header, so its linked raw file was retrieved separately. One source-code response was clipped after already providing the relevant matching implementation.
- **13 selected screenshot captures visually inspected.** Eleven provide interface/example context; one is an isolated control with insufficient context, and one is an anime illustration excluded from interface evidence. An additional blank Reddit-shell capture and two byte-retrieval probes did not provide usable visual evidence.
- No app, installer, infrastructure or DNS changes. No software was installed from any source.

The [earlier general review](/Users/barni/Documents/Github/main/docs/research-ai-slop-2026-10-19.md) covers usability and platform guidance. This report adds **community reactions paired with concrete examples and replies**. Searches stop here because the requested minimum is met and the main categories have examples, counterexamples and explicit limits; “unlimited” is permission to investigate gaps, not an obligation to keep issuing redundant calls.

## What “slop” means in these discussions

People use the same word for at least three different complaints:

1. **Recognizable template styling:** interchangeable layouts, gradients, card grids, familiar fonts and icons. The [UXDesign pattern thread][R05], [Nikunj Kothari's shadcn post][X02] and [Lobsters discussion][O02] contain direct examples.
2. **Unconsidered product work:** busy or incoherent screens, unclear actions, missing states, platform mismatch and insufficient maintenance. The [farming-game discussion][R26], [Mac app giveaways thread][R07] and [product-coherence essay][X10] make these concerns explicit.
3. **Distrust or opposition to AI involvement:** a provenance allegation substitutes for an explanation. The [study-app follow-up][R02], [productivity-app discussion][R20] and [selfhosted moderation exchange][R21] illustrate that dispute.

These overlap, but they need different responses. Changing purple to beige can address a stylistic association while leaving the product problem intact. A specific complaint about an unclear button is actionable; “this is AI” alone is not a diagnosis.

**Evidence strength:** high confidence that the cited people expressed the recorded views; qualitative confidence that themes recur within this deliberately selected corpus. No claim about the percentage of all users holding them, and no inference that appearance proves who wrote an app.

## Recurring criticism, with a practical translation

| What people point at | Direct community evidence | Useful question for an app review | Limit |
| --- | --- | --- | --- |
| Familiar purple/indigo gradients, dark panels and glows | [Study-app follow-up][R02], [Alex Albert][X03], [UXDesign thread][R05] | Is color serving meaning, hierarchy or a chosen identity? | A legitimate brand/system accent is not a defect |
| Repeated cards, nested rectangles, equal emphasis | [Study dashboard][R01], [game comparison][R04], [Siteplane explanation][R23] | What should the user see first, and what can recede? | Cards can be appropriate functional groups |
| Decorative icon boxes and arbitrary symbols | [UXDesign thread][R05], [icon-set comparison][R04] | Does the icon help recognition, and does it belong to the same family? | A standard functional icon is not filler; [X09][X09] lacks enough context to judge |
| Fonts and page structures that feel interchangeable | [UXDesign thread][R05], [shadcn post][X02], [Lobsters][O02] | Is the composition appropriate for this task and platform? | Do not replace a readable system font just to be different |
| Colored left/top borders and badge-heavy panels | [UXDesign screenshot][R05], [designer's illustrated article][O14] | Does that treatment express priority/state, or repeat decoration? | Both are observers' associations, not provenance evidence |
| Everything visible simultaneously | [Study dashboard][R01], [22-job menu][R26], [designer's guide][O10] | What is relevant at this point in the task? | Hiding necessary information can also make work harder |
| Busy visuals with no clear action | [Weather mockup][R19], [farming screenshot and replies][R26] | Can a new user identify the action without coaching? | The weather thread's navigation criticism concerns multiple screens; only its first image was visually inspected here |
| Web-dashboard conventions in a Mac utility | [Deleted Mac post's surviving replies][R09], [Kanban discussion][R10] | Are toolbar, window and navigation choices appropriate for Mac use? | Deleted media and untested framework allegations restrict the conclusion |
| Attractive demos that fail in actual use | [Mac low-quality warning][R08], [Mac giveaways discussion][R07] | Do primary flows, errors and persistence work in the shipped app? | Firsthand reports were not independently reproduced |
| Lots of features without a coherent journey | [Product Slop essay][X10], [productivity-app discussion][R20] | Does each feature support the core reason to use the product? | Essay/opinion, not a controlled comparison |
| Generic slogans instead of showing value | [Art Whisper screenshot feedback][R24], [designer's guide][O10] | Does the screen show the result users came for? | Art Whisper feedback includes a later product pitch |
| Missing empty/error/loading states | [Designer's guide][O10] | What happens before data exists, when permission is denied or a connection fails? | A proposed quality check, not proof about every AI-built app |
| Weak support, updates or accountability | [Mac giveaways][R07], [selfhosted replies][R21], [MacRumors discussion][O03] | Can the maintainer explain, repair and support the product? | Predictions of abandonment are not demonstrated outcomes |

The actionable pattern is **a mismatch between surface and purpose**. That is this report's interpretation of the examples, not a machine-learning explanation of why models behave that way.

## Image and discussion catalog

Images remain with their original publishers. Links can expire, require login or redirect; they were not downloaded or rehosted. “Inspected” below means the named viewport/image was returned by Context.dev and visually examined, not that the application was run. Other gallery frames are linked without claiming they were inspected.

### 1. A study dashboard, then its revised version

[Original thread][R01] · [original image](https://i.redd.it/my25mxa8c6rg1.jpeg) · [follow-up thread][R02] · [revised first image](https://i.redd.it/a0dk559vbw5h1.jpg) · [follow-up's original-version image](https://i.redd.it/tajcmc9vbw5h1.jpg)

**What people say:** daily, weekly, streak and monthly summaries compete on the same screen; others focus on the blue/purple palette and rounded boxes. The author replies that the layout was their own choice and that a Schedule control anticipated an unfinished feature. The follow-up includes objections to unexplained insults.

**What was seen:** the first screenshot in each thread, with multiple stacked progress/calendar regions. Other follow-up frames were not inspected.

**Useful lesson:** critique the information hierarchy and unfinished navigation independently of authorship. A palette change alone cannot settle whether those summaries belong together.

### 2. “Left borders like these”

[Discussion][R05] · [linked panel image](https://preview.redd.it/what-are-the-top-ai-slop-design-patterns-v0-jt4qcnnrqsqg1.png?width=870&format=png&auto=webp&s=81bd49831438a3617c3b86cedc6254fdc69f6743)

The author singles out colored left-edge lines; replies name emoji sidebars, familiar fonts and default kits. **Inspected:** the displayed excerpt of dense dark panels with colored left borders. This is an association made by commenters; the screenshot does not establish that the panels are unusable or generated.

**Useful lesson:** use a colored edge when it communicates something, not because every section needs embellishment.

### 3. An “anti-slop” after-example gets called slop

[Original tool post][R16] · [before/after image](https://i.redd.it/ruy8kh9w568h1.png) · [author's revision and reply][R25]

![Tool author's before/after example, disputed by commenters](https://i.redd.it/ruy8kh9w568h1.png)

**Inspected:** a purple/dark generic landing-page example beside a cream/green invoice layout. Replies criticize the replacement palette, display type and recurring card look. The author's follow-up acknowledges that the first example traded one recognizable default for another.

The image's scanner score is the tool's own output, **not an independently established design-quality score**.

**Useful lesson:** do not replace one house style with another and call the review complete. This is also evidence that the label changes with fashion.

### 4. A game's item-art before/after

[Thread][R04] · [inspected icon-sheet comparison](https://i.redd.it/ew8uo8caz8uh1.png) · [additional UI comparison, linked only](https://preview.redd.it/i-tried-to-fix-the-ai-slop-look-of-my-game-before-vs-after-v0-c6ha4bdh87uh1.png?width=1920&format=png&auto=webp&s=0abe1eb261e9ea02211fd83d8bc5a5d6534eb43d)

The developer reports criticism of inconsistent asset styles, nested rectangles, orange/brown coloring and game-inappropriate UI. They describe moving toward matched icon sheets and fewer boxes. Replies still identify scale inconsistency and an anatomical error; the author acknowledges a missed correction.

**Inspected:** the first item-sheet comparison, not all later UI images. **Useful lesson:** inspect the whole asset family for consistent weight, scale and detail; a shared generation prompt is not final visual QA. These fantasy-art specifics do not imply generating bespoke art for Mac permission controls.

### 5. The farming game with 22 possible jobs

[Original critique][R03] · [linked gameplay/UI thread][R26] · [image](https://i.redd.it/blasmwor09vh1.png)

![Farming-game UI discussed in the linked critique](https://i.redd.it/blasmwor09vh1.png)

**What people say:** the menu exposes too many possible jobs; action elements are hard to recognize; red warnings dominate. The author explains that crop entries are buttons but read like warning text, and acknowledges unfinished text/layout issues. Another commenter defends the UI.

**Inspected:** the gameplay image, including multiple dense floating panels and a long red-warning-heavy list.

**Useful lesson:** present actions relevant now, make buttons recognizable, and keep warnings from drowning out the task. This is a more specific diagnosis than “looks AI-generated.”

### 6. Siteplane: reduction explained in terms of the user's work

[Thread][R23] · [final image](https://i.redd.it/ybf8rfqqwwvh1.png) · [first generated version, linked only](https://i.redd.it/041c2dqqwwvh1.png)

The author explains why a live site preview, operational status and pending edits deserve priority, and why history is behind a button. They put analytics and bookings lower in the hierarchy.

**Inspected:** the first/final gallery image. **Not established:** measured improvement, the unseen original's exact defects, or the author's model-performance assertions.

**Useful lesson:** a redesign should explain what users need first, rather than only listing new colors, fonts and radii.

### 7. An elaborate SSX-inspired weather mockup

[Thread][R19] · [first image](https://i.redd.it/lv3y5vbbrzlh1.png) · [second screen, linked only](https://i.redd.it/cua9acybrzlh1.png) · [third screen, linked only](https://i.redd.it/ntehwvgcrzlh1.png)

**What people say:** clutter, unclear focus and inconsistent navigation; some accuse the author of understating AI use. The author agrees that attention is pulled in many directions.

**Inspected:** the first illustrated notebook/sticker-style phone mockup. It is visually unlike a purple SaaS dashboard.

**Useful lesson:** novelty is not enough; readability and a stable task structure still matter. The commenters' accessibility allegation was not verified with assistive technology.

### 8. Bruno's welcome screen is called AI slop

[Post][X04] · [image](https://pbs.twimg.com/media/HR_J7LJacAAX-3S?format=webp&name=large)

**Inspected:** a welcome modal with a dog mark, an emphasized import choice and two secondary action cards. The poster uses its appearance to draw a boundary between assistance and slop.

**Not established:** who designed/coded it, whether it predates the association, whether users find those choices clear, or whether the app's behavior is poor. Search discovery included a historical-design rebuttal, but that rebuttal was not independently verified and is not adopted as fact here.

**Useful lesson:** even a concrete screenshot and confident accusation do not establish provenance. Review its actual choices on their merits.

### 9. A designer's solar/light-app comparison

[Post][X07] · [comparison image](https://pbs.twimg.com/media/HMqDO2CagAA8UVP?format=webp&name=large)

![Designer's before/after comparison; content and lighting state also differ](https://pbs.twimg.com/media/HMqDO2CagAA8UVP?format=webp&name=large)

The designer calls the left design slop and presents their redesign on the right. **Inspected:** both images in the composite. The redesign still has a gradient and cards; emphasis and composition change.

**Important limit:** the example changes time/lighting state, displayed values and content. It is a promotional before/after, not a controlled usability test or proof of conversion improvement.

**Useful lesson:** “remove gradients/cards” is too crude. Ask what the screen emphasizes and whether the comparison preserves the same task.

### 10. A dashboard explicitly linked by a critic elsewhere

[Lobsters comment thread][O02] · [dashboard's original post][R27] · [image](https://i.redd.it/ndkgdvrm07lg1.png)

A Lobsters commenter links this dashboard while describing large numbers in small dark boxes as a recognizable generated style. The original author describes a local usage/cost tool and says they like dashboards.

**Inspected:** its first screenshot, including the statistic strip and charts. Data accuracy, usefulness and operation were not tested.

**Useful lesson:** a critic's stylistic association and a creator's actual use case can both exist. Do not remove useful charts or summaries solely to evade a label.

### 11. A generated Windows promotional screenshot

[Windows Latest post][X05] · [linked image](https://pbs.twimg.com/media/HDe8YT7bsAAu2X_?format=webp&name=large)

The post criticizes a promotional Windows image for details that do not match the actual interface. **Image linked and text retrieved; pixels not separately inspected here.** The attribution to Microsoft's marketing and generation method remains the poster's claim in this report.

**Useful lesson:** product evidence should show actual product output. This is a marketing-image example, not proof that the running Windows interface is AI-generated.

### 12. Art Whisper: show the useful result first

[App Store screenshot critique][R24] · [linked image](https://i.redd.it/pxzyom91rxch1.png)

A commenter suggests leading with the artwork/audio result rather than a slogan and empty camera action; another notes a mismatch between app-icon character and screenshot styling. The creator welcomes the feedback.

**Image linked; pixels not separately inspected.** One commenter later promotes a tool, so the exchange is not treated as independent conversion research.

**Useful lesson:** show the payoff of using the app, not just its entry point.

### Image exclusions and counterexamples

- [Misbah Syed's post][X09] provides an isolated “Image” control crop. It was inspected, but there is insufficient surrounding context to label the symbol arbitrary or harmful.
- [The Evangelion GUI criticism][X08] contains relevant textual criticism of decorative graphics, but the returned attached image is an anime illustration. It is excluded as UI screenshot evidence.
- [A designer reports human-made work being accused of AI][R13] and links thumbnails they say were made in 2022–2023. This is their account, not verified authorship. It is included as a warning against treating appearance as proof.
- [Peter Steinberger][X12] uses “AI slop” self-deprecatingly while describing an open-source SwiftUI app. That is not an independent negative app review.
- [The deleted Mac post][R09] retains useful comments about navigation, but its missing original media cannot support a fresh visual judgment.
- Ads, avatars, achievement badges, unrelated recommendations and “people also ask” content were excluded from the example catalog. The [EDHREC scrape][R18] lacked the critical comments suggested by discovery snippets; it is not used as verified backlash evidence.

## Existing community research discovered, and why its numbers need care

The [vibecoded-design-tells repository][O13] is unusually relevant: it publishes a claimed Reddit corpus, linked quote bank, tables and scripts. Its documentation reports about 3.2 million scanned posts, with 3,033 comments from 125 selected threads used for the comment ranking. **Those are the repository author's figures, not our searches, our analyzed sample or independently reproduced counts.**

I inspected its [method notes][O19], [comment counting code][O18], [matching implementation][O22], [quote bank][O20] and [raw result table][O21]. This supports a narrower reading than “research proves these designs are slop”:

- The code counts regex mentions. Its matching function does not determine whether a comment endorses, criticizes, quotes or rejects the named feature.
- The quote bank itself includes advice to use shadcn. A feature mention therefore is not automatically a complaint about that feature.
- The raw table separates mentions from a negative-word co-occurrence count. That separate regex still does not establish feature-specific sentiment.
- The inspected comment-counting routine sums matching comments; it does not itself weight a pattern by how widely it appears across different threads. Other collection/deduplication stages were not fully audited.
- Selected threads, search terms and available archives shape the sample. Neither these figures nor our search result counts estimate all users' preferences.
- The author's agent-based verification and scanner score are not independent human validation or measured AI-authorship detection accuracy.
- The original CSV page exposed only its header. The linked raw CSV was retrieved to confirm the actual columns and values.
- The full corpus and analysis were **not rerun**. No claimed false-positive rate, percentage or ranking is adopted as a validated result.

**Use it as a source-finding resource and inspect the linked comments, not as a universal style prohibition or a CI quality certificate.** Its [revised tool post][R25] also explicitly recognizes the cream/serif/sage replacement-default problem.

A separate [Show HN scoring article][O14] provides concrete image examples and a deterministic pattern-scoring approach. Its reported sample size, thresholds and manual false-positive estimate are the author's claims, not independently validated prevalence or provenance measurements.

## Contradictions that should change how we use this research

**“Default components are slop” versus familiar controls.** [X][X02] and [Threads][O04] posts associate common libraries with generated output; [Lobsters][O02], [UI_Design][R15] and [Hacker News][O01] replies note historical template sameness and the usefulness of familiar patterns. For a Mac utility, changing system typography or standard controls simply to look unusual would confuse style differentiation with product quality.

**“Use more icons” versus icon filler.** The [farming critique][R26] needs clearer actions; [other replies][R03] suggest recognizable game icons. [UXDesign][R05] and [asset feedback][R04] object to decoration or inconsistent assets. Use consistent, meaningful symbols with short accessible labels, rather than swapping every sentence for an unlabeled glyph.

**“Less clutter” versus forced minimalism.** The [study dashboard][R01] and [weather mockup][R19] elicit overload criticism. Yet the [dashboard creator][R27] has a real reason to inspect several metrics. The right amount of information depends on the task; hiding important states is not automatically an improvement.

**“Avoid purple” versus “beige is now slop too.”** The [tool revision][R25] makes this explicit. Build a deliberate product language; do not chase a moving list of forbidden colors.

**“Looks impressive” versus works over time.** [Mac users][R07] distinguish visual flash from bugs, maintenance and support. [MacRumors participants][O03] debate accountability and fresh-install testing. These are useful questions, but anecdotes here were not reproduced against installed binaries.

**“The community agrees” versus incentives and moderation context.** Tool sellers, app makers and designers promote products in these threads. The [selfhosted moderator reply][R21] distinguishes constructive criticism from abusive replies. Promotional incentives and missing context must remain visible.

## What this suggests for Helicoid

These are recommendations, not changes implemented or runtime findings from this research.

| Area | Concrete direction | Evidence to request before calling it improved |
| --- | --- | --- |
| Computer settings | Keep the Manager setup window language; use a compact utility-sized layout, two clear permission rows, consistent meaningful icons and short action labels | Actual packaged window in active/inactive, light/dark and keyboard/VoiceOver use |
| Permission actions | Prefer familiar native controls and readable state. Keep concise permission purpose where needed; remove repeated essays and redundant per-row settings gear controls | Real request/denial/revocation path, not a checkbox or screenshot |
| Computer menu | Prioritize current connection/access state and the next relevant action. Keep technical details in a status/details surface | A glance answers whether access is active and what the user can do next |
| Manager inventory | Make computer identity, actual preview/status and one clear primary action dominate. Avoid ornamental metric strips and repeated badges | Real inventory, empty/offline/denied/stale states and long names |
| Detail/control | Keep view-only and explicit control states unmistakable. Avoid visually equal competing actions | Focus, minimize, release and reconnect behavior exercised against actual effects |
| Icons/color | Reuse one symbol vocabulary; reserve emphasis for action and meaningful state. Keep useful system colors/materials/type | Readability, accessible names and consistent meaning across both apps |
| Copy | Remove generic welcome/product prose and repeated reassurance; retain operator identity, disclosure, permission purpose and important state | Users understand the task and its consequences without an explanation |
| Product scope | Finish the central workflow before adding placeholders, dashboards or unrelated tools | Installed end-to-end evidence and maintenance/recovery evidence |
| Marketing/release evidence | Show real builds and real pixels, with honest unavailable states and verified scope | Reproducible artifacts and appropriately bounded acceptance records |

The user's prior compact/native preference is context, not something this community sample independently validates. The report does not assert that the historical screenshots match today's installed build.

## Research limits and acceptance

**VERIFIED:** this turn's search/request ledger, selected source retrievals, the named screenshot observations, and the distinction between authors' claims and observed content.

**NOT VERIFIED:** a representative public consensus, any visual AI-authorship classifier, an app's implementation or safety based on its screenshot, reported third-party bugs, claimed dataset totals or statistics, conversion improvements, or Helicoid runtime/UI acceptance.

Searches deliberately sought negative and contradictory opinions, so selection bias is substantial. English-language indexing dominates. Reddit/X yielded substantially more direct app/interface evidence than the smaller social platforms. A targeted platform query sometimes returned off-platform or irrelevant results; those results are not counted as relevant community evidence merely because the request succeeded.

Comments are ranked and partial, not full thread histories. Votes/views change and are not used as evidentiary weights. Relative dates shown by Reddit were preserved as relative rather than invented exact dates. Deleted media, incomplete replies and gallery frames were disclosed above. No YouTube video was watched; its page metadata/text was retrieval evidence only. No new accounts, posts, reactions, monitors or tool installations were created.

The stopping criterion was met: at least 200 distinct successful searches, specific linked examples, inspected images, separate behavior/native-app concerns, contradictory evidence, a source ledger, and actionable recommendations. No additional broad searches were needed to resolve those answer slots.

## Selected-source ledger

All resources below were retrieved on **19 October 2026**. Publication dates are exposed metadata or explicit page text; a dash means no exact date is claimed. Labels identify sources, not quality scores. “Retrieved” does not mean complete thread/video/app coverage.

| ID | Source | Published | Coverage / evidence boundary |
| --- | --- | --- | --- |
| R01 | [Screenshot with detailed layout feedback][R01] | — | Body, replies and first screenshot inspected; other content excluded. |
| R02 | [Follow-up screenshots after changes][R02] | — | Body/replies and first revised screenshot inspected; other gallery frames linked only. |
| R03 | [Rebuilding game UI by hand][R03] | — | Body/replies read; follows link to R26; ad image excluded. |
| R04 | [Game before/after comparison][R04] | — | Body/replies read; first icon comparison inspected. |
| R05 | [UX designers' pattern discussion][R05] | — | Body/replies and panel example inspected. |
| R06 | [Webdesign tells and counterexamples][R06] | — | Body/replies read; rounded-corner neuroscience claim not adopted. |
| R07 | [Mac app giveaways][R07] | — | Body/replies read; app-quality anecdotes not reproduced. |
| R08 | [Mac app low-quality warning][R08] | — | Body/replies read; crashes/monetization anecdotes not reproduced. |
| R09 | [Mac app appearance feedback][R09] | — | Original post deleted; surviving replies only. |
| R10 | [Kanban Pro native-claim dispute][R10] | — | Body/replies read; native/Electron dispute not independently tested. |
| R11 | [Mac vibe-coding ethical debate][R11] | — | Body/replies read; ethics discussion, no visual verdict. |
| R12 | [MacApps moderation rules][R12] | — | Moderator post/replies read; historical policy, not current rule verification. |
| R13 | [Human-made work accused of AI][R13] | — | Creator account/replies read; authorship and image dates not independently verified. |
| R14 | [Objection to indiscriminate labeling][R14] | — | Original deleted; surviving argument only. |
| R15 | [Websites looking like slop][R15] | — | Body/replies read; historical/design claims remain opinion. |
| R16 | [Unslop-ui tool and before/after replies][R16] | — | Body/replies and comparison inspected; scanner efficacy not validated. |
| R17 | [Claude default appearance criticism][R17] | — | Creator/tool promotion and replies read; no installs. |
| R18 | [EDHREC redesign announcement][R18] | — | Announcement retrieved; backlash comments absent from this response. |
| R19 | [Weather UI critique][R19] | — | Body/replies and first mockup inspected; no accessibility runtime test. |
| R20 | [Productivity app quality discussion][R20] | — | Body/replies read; no prevalence claim. |
| R21 | [Selfhosted moderation and quality][R21] | — | OP and moderator reply read; external vulnerability allegations not adopted. |
| R22 | [Lovable style drift][R22] | — | Body/replies read; style-drift report not reproduced. |
| R23 | [Siteplane dashboard critique][R23] | — | Author's rationale and first/final screenshot inspected; no measured comparison. |
| R24 | [App Store screenshots critique][R24] | — | Body/replies read; image linked only; later product promotion noted. |
| X01 | [DesignCourse definition][X01] | 2026-07-24 | Designer opinion read; performance/superiority assertions not adopted. |
| X02 | [shadcn association complaint][X02] | 2025-08-15 | Original association complaint read; no image evidence. |
| X03 | [Alex Albert on purple defaults][X03] | 2025-11-12 | Original post read; linked image not separately inspected. |
| X04 | [Disputed UI provenance][X04] | 2026-09-12 | Post and Bruno image inspected; provenance unverified. |
| X05 | [Claimed generated Windows promotional screenshot][X05] | 2026-03-15 | Post/media URL read; image not separately inspected; attribution remains claim. |
| X06 | [Fable redesign promotion][X06] | 2026-06-10 | Promotional post retrieved; video not watched. |
| X07 | [Client redesign comparison][X07] | 2026-07-08 | Post/comparison inspected; different displayed conditions limit comparison. |
| X08 | [Evangelion GUIs critique][X08] | 2026-03-10 | Text read; attached anime illustration excluded from UI evidence. |
| X09 | [Random icon criticism][X09] | 2026-07-11 | Text/crop inspected; insufficient context for verdict. |
| X10 | [Feature works, product does not][X10] | 2026-08-18 | Essay read; author promotes related skills; product advice, not experiment. |
| X11 | [Attention to detail][X11] | 2026-08-14 | Short opinion retrieved; no measured historical claim. |
| X12 | [Self-described AI-built SwiftUI app][X12] | 2025-06-05 | Self-description read; app not installed/tested. |
| O01 | [Hacker News AI Aesthetic discussion][O01] | — | Discussion excerpts read; speculative model explanations not adopted. |
| O02 | [Lobsters AI Aesthetic discussion][O02] | 2026-07-30 | Discussion read; linked dashboard followed to R27. |
| O03 | [MacRumors Mac apps discussion][O03] | 2026-08-28 | Forum discussion read; app/safety anecdotes not independently reproduced. |
| O04 | [Threads icon-default advice][O04] | — | Original advice read; no evidence library replacement improves usability. |
| O05 | [Velox design promotion][O05] | 2026-09-16 | Original promotional post read; AI-generated headline and related feed excluded. |
| O06 | [Product Hunt readability discussion][O06] | — | Author's discussion read; opinion, not productivity measurement. |
| O07 | [IndieHackers design advice][O07] | 2026-08-12 | Creator's proposed workflow read; no efficacy guarantee adopted. |
| O08 | [Unslop-ui repository][O08] | — | README/examples index read; no skill installed. |
| O09 | [Interactive before/after field guide][O09] | — | README/taxonomy claims read; no scanner installed or validated. |
| O10 | [Designer firsthand field guide][O10] | 2026-07-23 | Illustrated guide read; opinions/model explanations not empirical proof. |
| O11 | [Video on visual-slop fixes][O11] | — | Page metadata/text retrieved; no video watched. |
| O12 | [Hacker News UI slop game][O12] | — | Brief game submission/replies read; game not executed. |
| O13 | [Claimed community-analysis dataset][O13] | — | README/method claims read; corpus not rerun. |
| R25 | [Updated unslop-ui feedback][R25] | — | Author acknowledges replacement default; replies read; demo not executed. |
| O14 | [Design Slop linked by Lobsters][O14] | 2026-04-20 | Illustrated author analysis read; reported scoring/false-positive rates not validated. |
| O15 | [Performative UI linked by Lobsters][O15] | — | Satirical component catalog read; satire not empirical prevalence. |
| R26 | [Game UI linked in critique replies][R26] | — | Body/replies and gameplay screenshot inspected. |
| R27 | [Dashboard explicitly linked as example][R27] | — | Body/replies and first dashboard screenshot inspected; data not tested. |
| O16 | [Interactive kill-ai-slop guide][O16] | — | Multilingual field guide retrieved; no exhaustive demo evaluation. |
| O17 | [Comment-level ranking table][O17] | — | GitHub CSV view yielded header only; followed raw link to O21. |
| O18 | [Comment analysis implementation][O18] | — | Counting code inspected; not executed. |
| O19 | [Dataset method and graph notes][O19] | — | Method/figures read; source author's claims, not reproduced. |
| O20 | [Linked comment quote bank][O20] | — | Selected quote-bank sections inspected; linked quotes include endorsements. |
| O21 | [Raw comment ranking table][O21] | — | Raw CSV retrieved; numbers not independently reproduced. |
| O22 | [Tell matching implementation][O22] | — | Matching implementation inspected in clipped response; not full pipeline audit. |

## Visual-evidence receipt

Context.dev screenshot request IDs identify the actual captures examined. This list excludes the blank-shell probe. The X08 illustration and X09 crop are retained here for audit but excluded from the eleven contextual interface examples.

| Source | Screenshot request ID | Observed coverage |
| --- | --- | --- |
| [X07][X07] | `ed5ad0ad-03a3-4c6e-94cc-77b29706dc66` | Before/after comparison inspected; changed data and scene, not a controlled usability comparison. |
| [R02][R02] | `1297f444-cab5-49a7-a330-d1e71f0a77d6` | First revised study-dashboard gallery image inspected; other gallery images only linked. |
| [R01][R01] | `27d029e0-c58c-4cfd-a212-b92745e0ad07` | Original study-dashboard screenshot inspected. |
| [R23][R23] | `ff47c979-539b-4f3a-8723-df1fd93032f0` | First (final) dashboard gallery image inspected; original version only linked. |
| [R04][R04] | `0204998f-b230-4256-9621-48e246e98689` | First icon-sheet before/after inspected; other UI comparisons only linked. |
| [R19][R19] | `9419db4b-ce07-460b-aa60-7ffa778dcf49` | First weather-mockup gallery image inspected; other screens only linked. |
| [X04][X04] | `62cfeb2f-e81c-4956-8018-e7fc581e5e1e` | Bruno welcome/onboarding screenshot inspected; provenance not established. |
| [X09][X09] | `13ba60c9-8c48-4ddc-b2d9-9f7ddcf3fe8a` | Isolated Image control crop inspected; too little context for a quality verdict. |
| [X08][X08] | `4c2ff4d0-522c-4234-9a22-20849a40c31b` | Anime illustration, not the proposed GUI; excluded from interface image evidence. |
| [R05][R05] | `224a32e9-44eb-4aa8-a654-ffd4e6a69ee5` | Colored left-border example inspected; dense panel excerpt, not a tested app. |
| [R16][R16] | `a171287a-7732-48cc-8654-bd583c2dda45` | Tool author's before/after image inspected; accompanying objections retained. |
| [R26][R26] | `63b0af0a-4b32-41b8-85ca-e09157398265` | Farming game screenshot inspected; dense menus and red warnings visible. |
| [R27][R27] | `4108a55f-b12b-45ee-a1e6-ce8a201747a0` | Dashboard linked by Lobsters commenter inspected; usefulness not tested. |

## Source-retrieval and search audit

The main ledger below contains **200 distinct successful new queries**; all requested ten results and returned ten. Two successful exploratory requests are additional: `930346b6-05f3-45c0-9810-2c3a64478ac2` (Reddit screenshot discovery) and `44f8d74f-98a2-493b-9177-35eeffcdf403` (X interface discovery). They are excluded from the 200-row count. No failed main search is counted as success.

The three completed source batches were:

| Batch | Completed resources | Submission request ID |
| --- | --- | --- |
| `batch_1995dd6c5f415c73d8e1dace41bcec54` | 47 | `fc65b395-08eb-4837-9f26-46e877b56816` |
| `batch_0e5c886d7adf256ad57f5d46a998b6e2` | 7 | `c7fab79c-a8ac-4264-b4eb-db2fe6e662c3` |
| `batch_650f20d088f006f019185a07c86235f0` | 4 | `f1c1cfc0-9c12-4e8b-8cc8-74bb67e5fb49` |

The three individual selected-page request IDs were R01 `fd089d3b-f486-4a0c-9df6-387ffab632eb`, O21 `359affa2-df7e-4d25-ba21-26ecae90df7c` and O22 `75e7debc-d28c-4867-9260-be7cdb04ef8e`. Batch item IDs match the selected-source labels; paginated response IDs are retained in the temporary evidence receipt.

The receipt `/tmp/helicoid-community-slop-20261019-evidence.json` retains queries, ranked result entries, retrieved text, page coverage and request metadata. It omits screenshot bytes and account balances. It is temporary, not a published dataset or a reproducible frozen copy of the external sites. The durable report preserves the query/request ledger and source/image links. No total credit cost is inferred from account balances.

Query scopes comprised 70 Reddit-targeted queries, 50 X/Twitter-targeted queries, and 80 covering other platforms or cross-platform subjects. A site operator was a discovery aid, not a guarantee every result came from that host. Of the 2,000 raw result entries, 601 were Reddit URLs and 434 X URLs; repeat URLs and irrelevant hits are included in those discovery figures and were not treated as independent observations.

### Reddit: explicit screenshot critiques

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 1 | site:reddit.com "slop" "UI" "screenshots" | 10 | `98a01fd7-281f-443f-a5fd-77c269f11366` |
| 2 | site:reddit.com "AI slop" "before" "after" design | 10 | `34361f7f-5e44-48a3-b7dc-d226bd5fec20` |
| 3 | site:reddit.com "AI slop" "dashboard" image | 10 | `1a29caa2-ffc3-49b6-8134-3a6dfe98b65c` |
| 4 | site:reddit.com "vibecoded" "looks" "screenshot" | 10 | `6dcf75b1-aa4e-4be5-830e-a20a8af05803` |
| 5 | site:reddit.com "AI generated" "roast" "UI" | 10 | `9b5ca7d3-944a-48d9-8b7c-f2bba331238c` |
| 6 | site:reddit.com "AI slop" "landing page" critique | 10 | `bce59d97-6fe1-4761-9716-620a84cfb70c` |
| 7 | site:reddit.com "AI slop" "app design" feedback | 10 | `56356972-bbde-4cdc-8f51-be64cdad0c7d` |
| 8 | site:reddit.com "slop UI" "example" | 10 | `c5c75068-3f47-4cb9-88ac-11806997fa17` |
| 9 | site:reddit.com "slop" "redesign" "interface" | 10 | `1d261dc0-550f-41fb-9e9d-e0233610c0ca` |
| 10 | site:reddit.com "AI look" "my app" screenshots | 10 | `95638872-1f06-42ca-8480-663b76a58707` |

### Reddit: Mac apps and platform fit

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 11 | site:reddit.com/r/macapps "slop" design | 10 | `1661efdf-02aa-4f29-aed8-e281bf13fb90` |
| 12 | site:reddit.com/r/macapps "vibe coded" quality | 10 | `e23ec23a-6910-49c8-9f08-f2afa26d0162` |
| 13 | site:reddit.com/r/macapps "AI generated" interface | 10 | `0d1300fa-9981-4ab6-b4bb-9c729aa644b4` |
| 14 | site:reddit.com/r/macapps "native" "Electron" "slop" | 10 | `03b01fa0-a1cc-4034-956b-ac6f4224067e` |
| 15 | site:reddit.com/r/MacOS "AI slop" apps | 10 | `75a5e18d-a761-4dfa-8944-43698bda5669` |
| 16 | site:reddit.com/r/SwiftUI "AI" "ugly" | 10 | `16c2dc7c-4b02-472f-b852-11644b6208f2` |
| 17 | site:reddit.com/r/macapps "AI" "subscription" "wrapper" | 10 | `a9fd5466-a915-44d4-bdbe-70168ea077ea` |
| 18 | site:reddit.com/r/macapps "vibecoded" screenshot | 10 | `57d67412-9f5b-45b5-92f7-9a32b130ff52` |
| 19 | site:reddit.com/r/macapps "low effort" apps design | 10 | `9143f76b-771c-4e33-b1d3-6c8c76cb068f` |
| 20 | site:reddit.com/r/macapps "AI" "polish" app | 10 | `6b225848-1818-41d5-8cb4-babd791f08ea` |

### Reddit: visual tells

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 21 | site:reddit.com "AI slop" "purple" | 10 | `ad5515b4-fc1a-4265-a75e-336f43cd8bf5` |
| 22 | site:reddit.com "AI slop" "gradient" | 10 | `6167ef11-8e20-444a-bacf-20152484a394` |
| 23 | site:reddit.com "AI slop" "cards" | 10 | `3f8d49fb-7be6-419e-8702-1b72ed6ea17f` |
| 24 | site:reddit.com "AI slop" "rounded" | 10 | `b1fe12d9-c987-4a6a-afc5-ea32a3e04c60` |
| 25 | site:reddit.com "AI slop" "emoji" UI | 10 | `59bddef4-8287-4a61-93f4-f8c9527af7c3` |
| 26 | site:reddit.com "AI slop" "font" interface | 10 | `7fbc8867-275d-4d40-b513-b98fb8368cb1` |
| 27 | site:reddit.com "AI slop" "glassmorphism" | 10 | `a1663126-25b6-43e2-8b43-45832bd69ce3` |
| 28 | site:reddit.com "AI slop" "spacing" design | 10 | `df846cea-7d7c-4c1d-9cf7-185d8a0f5c10` |
| 29 | site:reddit.com "AI slop" "icons" screenshot | 10 | `de80ad9b-2b9d-4f4b-8e22-61873e3da79c` |
| 30 | site:reddit.com "AI slop" "dark mode" design | 10 | `bd98c690-d024-44f7-a93c-f4d69b524b8f` |

### Reddit: designer communities

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 31 | site:reddit.com/r/UXDesign "AI slop" interface | 10 | `48f1623e-0878-4045-9b03-07dfe705d460` |
| 32 | site:reddit.com/r/UI_Design "slop" screenshot | 10 | `d122c4fa-733c-4458-8ee0-42d3d1b8dfcb` |
| 33 | site:reddit.com/r/web_design "AI slop" examples | 10 | `a3cb155a-b2ee-4232-bf60-7925246e06e3` |
| 34 | site:reddit.com/r/design "AI slop" apps | 10 | `cc3adfc5-781f-46f6-a28a-128f0315befc` |
| 35 | site:reddit.com/r/graphic_design "slop" "website" | 10 | `a0ad77ae-7751-41e4-82b4-d8c8134f06e9` |
| 36 | site:reddit.com/r/SideProject "AI slop" design | 10 | `38ab3b09-969f-44b4-aa58-a16aa84873f9` |
| 37 | site:reddit.com/r/SaaS "slop" "design" feedback | 10 | `e05bc23d-af8b-4ec9-b39f-8e770d4c5186` |
| 38 | site:reddit.com/r/vibecoding "slop" "critique" | 10 | `cbd9ab33-fb5c-4ed8-847e-6a082d20ca1e` |
| 39 | site:reddit.com/r/ClaudeCode "UI" "generic" design | 10 | `bbf57d1a-128c-474a-a9b5-5f3396ebdf0a` |
| 40 | site:reddit.com/r/Frontend "slop" interfaces | 10 | `50834759-1b03-4e9f-a8f6-0d39e209a431` |

### Reddit: behavior and broken products

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 41 | site:reddit.com "vibe coded" "buttons" "work" | 10 | `8b0bdcd4-af7d-429a-bc74-5487336df514` |
| 42 | site:reddit.com "AI slop" "broken" app | 10 | `d0a3db98-4d42-455d-82fe-e4a161c4b4a0` |
| 43 | site:reddit.com "AI slop" "accessibility" | 10 | `7f8682d6-c767-4b5b-be27-9ff0c2410ea7` |
| 44 | site:reddit.com "AI slop" "slow" app | 10 | `cc015a29-80a4-4070-9ff8-5dee180c487f` |
| 45 | site:reddit.com "vibecoded" "lost" "data" | 10 | `c3d52639-4b35-4807-84d8-42994fae2ac3` |
| 46 | site:reddit.com "AI slop" "permissions" app | 10 | `95dd82cd-d2cc-4a62-8fdb-97524e96330b` |
| 47 | site:reddit.com "AI slop" "onboarding" | 10 | `5a511152-5951-4402-8a26-2c40c25cd977` |
| 48 | site:reddit.com "AI slop" "settings" | 10 | `57422b5d-9a40-4a47-89a0-9ca510b4e9a4` |
| 49 | site:reddit.com "AI slop" "error" software | 10 | `0dbd6fbc-b23c-46dd-b2c4-78254ce5ca12` |
| 50 | site:reddit.com "vibe coding" "keyboard" app | 10 | `5c490c61-b2e6-4cfa-949b-f60ca2323220` |

### Reddit: disagreement and misidentification

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 51 | site:reddit.com "called AI slop" "hand" | 10 | `2cb0e711-2954-46c0-86a7-0a3345619d1b` |
| 52 | site:reddit.com "AI slop" "before AI" design | 10 | `0e39f778-8991-4e99-9409-8a81d6a5a904` |
| 53 | site:reddit.com "AI slop" "shadcn" unfair | 10 | `a107d608-12f6-417a-9dda-526dd5469e53` |
| 54 | site:reddit.com "AI slop" "looks good" | 10 | `dff88192-eaf0-495e-bf47-810359dfde73` |
| 55 | site:reddit.com "AI slop" "human" "design" | 10 | `ba1d477d-fbaf-42cd-af0c-741719a1dd1f` |
| 56 | site:reddit.com "AI slop" "gatekeeping" UI | 10 | `6f0e89b0-674d-4ebb-ba7b-cd857fbc3a12` |
| 57 | site:reddit.com "AI slop" "minimalist" design | 10 | `f797cf7c-8aad-452a-bd3d-c628db19dd34` |
| 58 | site:reddit.com "AI slop" "not AI" interface | 10 | `130f93e0-0f73-48a9-9667-d3a5afbd6989` |
| 59 | site:reddit.com "vibecoded" "beautiful" native | 10 | `10d0705a-eb50-4f59-aabf-d7b224b15492` |
| 60 | site:reddit.com "slop" "functionality" "design" app | 10 | `02a9ca50-f3f0-42d1-9989-72e5fed7dc38` |

### X: screenshot and example callouts

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 61 | site:x.com "AI slop" "UI" screenshot | 10 | `e736a671-9ffe-4fab-be0f-161958eded10` |
| 62 | site:x.com "AI slop" "design" image | 10 | `89759bf6-2a22-4a58-8aab-61159e3bb7e3` |
| 63 | site:x.com "slop" "this UI" | 10 | `6a61ddd7-8bf1-4d2f-b98e-3c40c313a14f` |
| 64 | site:x.com "vibe coded" "looks like" | 10 | `7607d18b-9083-4599-80b6-c9d1fcefca26` |
| 65 | site:x.com "AI slop" "dashboard" | 10 | `0850d54f-16e5-426d-9ddf-be34db6c7fa3` |
| 66 | site:x.com "AI slop" "landing page" | 10 | `5d61af73-f730-44fb-a80d-7a73c0d33257` |
| 67 | site:x.com "AI slop" "app" screenshot | 10 | `75e2bd46-b735-4692-8ff4-0bb7f3889956` |
| 68 | site:x.com "AI slop" "redesign" | 10 | `102a6da7-4a89-433f-9ee3-aa585ca8deb6` |
| 69 | site:x.com "AI slop" "interface" example | 10 | `ce875a4b-e401-4b54-ac31-a1d6e0940dd4` |
| 70 | site:twitter.com "AI slop" "UI" | 10 | `a569ea1e-577e-4909-8c4f-896ff927aa5f` |

### X: visual motifs

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 71 | site:x.com "AI slop" "purple" | 10 | `47ad2b97-528d-4b28-8b19-eb7d7bf371f6` |
| 72 | site:x.com "AI slop" "gradient" | 10 | `f9d9056f-7d37-4bd8-80a6-90e04d273319` |
| 73 | site:x.com "AI slop" "cards" | 10 | `01a6c0ae-e3a6-4cd5-bac7-344d2fdc4bf6` |
| 74 | site:x.com "AI slop" "rounded" | 10 | `1b1c344a-ddd0-4eeb-aabd-044ca9c12790` |
| 75 | site:x.com "AI slop" "font" | 10 | `c53a10b4-f6b2-409c-893d-136d2d13106b` |
| 76 | site:x.com "AI slop" "icons" | 10 | `2e829c45-30ce-4843-b6cf-197937e6f8c0` |
| 77 | site:x.com "AI slop" "glass" | 10 | `a30e17f0-a592-47d1-847f-fd0b47f5a036` |
| 78 | site:x.com "AI slop" "spacing" | 10 | `4d76b32e-730e-4f26-8dd1-9b4b2bbe2b4b` |
| 79 | site:x.com "AI slop" "shadcn" | 10 | `8216e5ab-b0e5-4d16-9f9f-e8f23b6b69ee` |
| 80 | site:x.com "AI slop" "bento" | 10 | `21f5e4cc-c590-4c05-b455-8a4f0f2f44d6` |

### X: Mac software and product behavior

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 81 | site:x.com "AI slop" "macOS" | 10 | `87656573-1e00-4feb-8e10-301943c181a8` |
| 82 | site:x.com "AI slop" "Mac app" | 10 | `3ee6f51b-25fc-482f-bdac-1450510a8f67` |
| 83 | site:x.com "vibe coded" "native" app | 10 | `bba4ae6d-9af0-4aee-abed-24cdf78b7517` |
| 84 | site:x.com "AI slop" "Electron" | 10 | `e14d2f65-158b-4267-a43b-521e37d51569` |
| 85 | site:x.com "AI slop" "SwiftUI" | 10 | `0f3827a3-658b-4818-80af-5d64dc54506d` |
| 86 | site:x.com "AI slop" "buttons" | 10 | `048a93a7-4c9c-4d2a-858e-2183bd0027d9` |
| 87 | site:x.com "AI slop" "accessibility" | 10 | `79ac5804-1c22-4702-bd93-87f04ce69496` |
| 88 | site:x.com "AI slop" "product" "works" | 10 | `a47f7fe1-3855-48f4-90f1-cd71e1682050` |
| 89 | site:x.com "AI slop" "onboarding" | 10 | `885ac25f-0ec6-458c-93c8-5fc39441876b` |
| 90 | site:x.com "AI slop" "settings" | 10 | `a28caa76-ba67-4151-bc9f-3177d457bc52` |

### X: disagreement and original criticism

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 91 | site:x.com "AI slop" "Bruno" | 10 | `924a01a5-ee94-47f8-a6af-6c90347be958` |
| 92 | site:x.com "AI slop" "before" "design" | 10 | `cc5ea2e0-5153-4fa3-8d54-6e5b2b758a3b` |
| 93 | site:x.com "AI slop" "taste" | 10 | `0a2a297b-1df1-44b3-b7cc-244f0fe9bf31` |
| 94 | site:x.com "AI slop" "not AI" | 10 | `f85d9585-4012-4ddf-862f-a55b949d1b02` |
| 95 | site:x.com "AI slop" "handcrafted" | 10 | `32e4446d-0dd4-4e7f-b7c6-100e660b1c09` |
| 96 | site:x.com "AI slop" "Linear" | 10 | `151d2eb6-120b-40ef-bd5b-1ef4488b255e` |
| 97 | site:x.com "AI slop" "Vercel" | 10 | `b320ea12-b203-4ffa-860c-54e64a62a725` |
| 98 | site:x.com "AI slop" "same" "website" | 10 | `4a7a5a3e-4c2b-4570-b6d5-6c38885fdc64` |
| 99 | site:x.com "AI slop" "slopification" | 10 | `5b31fa5c-b1c5-4632-a897-b5368e3cdbbe` |
| 100 | site:x.com "AI slop" "design engineer" | 10 | `8fb79647-06c5-451b-881d-fc74856501aa` |

### Hacker News: app criticism and demonstrations

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 101 | site:news.ycombinator.com "AI slop" "UI" | 10 | `53d1fbcb-3a4e-4d0d-824d-9b762b28fb78` |
| 102 | site:news.ycombinator.com "AI slop" "macOS" | 10 | `637e930d-fd04-463d-8e4c-ca5d85fdfcaf` |
| 103 | site:news.ycombinator.com "AI slop" "screenshots" | 10 | `086eb952-5921-4805-b2ac-ac8a98000bad` |
| 104 | site:news.ycombinator.com "vibe coded" "design" | 10 | `64b3af64-6128-4109-a118-6c6a7b8c90a1` |
| 105 | site:news.ycombinator.com "AI slop" "buttons" | 10 | `3c44031e-34e9-428e-8272-f03e184e5bf3` |
| 106 | site:news.ycombinator.com "AI slop" "purple" | 10 | `ff651c5a-b97c-4fcd-a9fd-b087b6f8593e` |
| 107 | site:news.ycombinator.com "AI slop" "native" | 10 | `9199c781-2917-400f-98b7-067b3241160b` |
| 108 | site:news.ycombinator.com "AI slop" "website" "same" | 10 | `c9fe0c56-9031-45b1-bbcb-0e113c7f0c21` |
| 109 | site:news.ycombinator.com "AI slop" "Show HN" | 10 | `aa32466c-c874-42c8-a65c-14abcc5e2262` |
| 110 | site:news.ycombinator.com "AI slop" "polish" | 10 | `2e041dd8-7c82-4ad7-994e-e1afbf9e622a` |

### Bluesky and Mastodon: design reactions

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 111 | site:bsky.app "AI slop" "UI" | 10 | `681a0791-4384-43aa-ae5b-c466143d7af2` |
| 112 | site:bsky.app "AI slop" "design" | 10 | `40b03863-ff1a-4dcf-a7f9-3970de761854` |
| 113 | site:bsky.app "AI slop" "app" | 10 | `9e10398f-a6bc-49fe-b273-826efd8751e9` |
| 114 | site:bsky.app "AI slop" "purple" | 10 | `42b300aa-aaaa-4fc2-a651-9126f2f5bf6d` |
| 115 | site:bsky.app "AI slop" "macOS" | 10 | `0141f542-5400-4170-9596-49c75113ba39` |
| 116 | site:mastodon.social "AI slop" "UI" | 10 | `f93b697e-9e19-426c-8ba3-8770026010d7` |
| 117 | site:mastodon.social "AI slop" "app" | 10 | `bfca9ecf-1de8-43b5-b624-39c29bd825e4` |
| 118 | site:fosstodon.org "AI slop" "software" | 10 | `a0966930-e14c-4af1-a09a-fbcae96efd0b` |
| 119 | site:indieweb.social "AI slop" "design" | 10 | `5748288b-8047-408b-9a07-233d60f3e693` |
| 120 | site:hachyderm.io "AI slop" "interface" | 10 | `fd403977-693e-4ff8-a364-c35ed1936cfc` |

### Design showcases and professional discussion

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 121 | site:dribbble.com "AI slop" design | 10 | `3820c21e-d560-4058-8c7f-dfca5863a00f` |
| 122 | site:behance.net "AI slop" interface | 10 | `875cfc82-5735-4b24-a149-448414b895c0` |
| 123 | site:designernews.co "AI" "slop" | 10 | `8f602431-3523-42a4-b991-5816069767fc` |
| 124 | site:figma.com "AI slop" UI | 10 | `13dda2a5-a28c-455a-b113-1a83f1d93b5e` |
| 125 | site:linkedin.com/posts "AI slop" "UI" | 10 | `eb6dd3ae-5488-41aa-989f-e1d2f02116d5` |
| 126 | site:linkedin.com/posts "AI slop" "purple" | 10 | `587a2e92-d5fb-43ea-98da-82924d0bfc2f` |
| 127 | site:threads.com "AI slop" "design" | 10 | `0b6aa5f3-8108-4cc6-997b-0c4a7d6cb9ae` |
| 128 | site:threads.net "AI slop" "UI" | 10 | `cf142b9c-7153-4458-aee1-fdde04238fdd` |
| 129 | site:producthunt.com "AI slop" design | 10 | `4d3f5be8-50ea-4764-bb0e-08fa9571e9ca` |
| 130 | site:indiehackers.com "AI slop" "design" | 10 | `8ffd22ad-6909-4f39-ade0-3d28fb37a98d` |

### GitHub: linked critique and anti-slop rules

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 131 | site:github.com "AI slop" "UI" screenshots | 10 | `a88de8ac-7b3a-4c83-9090-c33a9367dce9` |
| 132 | site:github.com "slop" "purple gradients" | 10 | `6c4113e3-041f-40e9-b864-c6d1419c33e3` |
| 133 | site:github.com "AI slop" "macOS" | 10 | `06d13482-ac3b-47d1-897c-0df67c0f2264` |
| 134 | site:github.com "AI slop" "design" examples | 10 | `5f11f0dd-089f-4798-97ce-78ec6e3d2258` |
| 135 | site:github.com "unslop" "UI" | 10 | `46bf11f5-8e50-4702-ba43-1d5665c7d8df` |
| 136 | site:github.com "AI slop" "bento" | 10 | `243ba917-6fc8-486a-a105-43b476d97ac6` |
| 137 | site:github.com "AI slop" "accessibility" | 10 | `f80ebd7e-c07f-42df-acf4-c3641436ab78` |
| 138 | site:github.com "AI slop" "before" "after" | 10 | `4b7e9e47-40a5-43bd-89fe-06a76f753276` |
| 139 | site:github.com "AI slop" "shadcn" | 10 | `faf64d5a-0fa4-427e-baeb-19229a887547` |
| 140 | site:github.com "AI slop" "design skills" | 10 | `aeb174e7-c814-40ae-b313-14cc465e17e2` |

### Creator posts and essays with examples

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 141 | "AI slop" "UI" "screenshots" -site:reddit.com -site:x.com | 10 | `da7d71ca-376b-4e72-86ac-2c3e9f81869b` |
| 142 | "AI slop" "web design" "examples" -site:reddit.com | 10 | `3d5a6eef-1cd0-4c0c-b00e-079a00feecc1` |
| 143 | "AI slop" "mac apps" criticism | 10 | `1c1faf36-f4d6-4d51-831f-f84f9bd25f17` |
| 144 | "AI slop" "design" "before and after" | 10 | `bcec83d2-cd6d-40e8-b51e-0b5b3cbc7146` |
| 145 | "AI slop" "interface" "images" critique | 10 | `b3cd43b4-0186-4cc9-96f3-d78ab0ea7b59` |
| 146 | "vibe coded" "design" "roast" -site:reddit.com | 10 | `49a8dede-d8bc-42f1-b420-ecadae55c371` |
| 147 | "AI slop" "app" "review" native | 10 | `29734191-8bab-4867-a78d-94a9dcd378fc` |
| 148 | "AI slop" "purple gradient" designer | 10 | `8273e068-fa4f-4a0d-bbe3-f0e8b10d7bb3` |
| 149 | "AI slop" "design" "screenshot" blog | 10 | `4ff72092-190d-4197-a7e0-8413d87f6223` |
| 150 | "AI slop" "website" "identical" | 10 | `f5f2676b-47eb-44bd-87ee-2da1f2abfb02` |

### Video and image-led discussion

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 151 | site:youtube.com "AI slop" "UI design" | 10 | `a0b5d3a8-e8da-4d4d-8b70-6fd02d3b0880` |
| 152 | site:youtube.com "AI slop" "website" roast | 10 | `b62da556-e3aa-4833-84a4-3c7f4e7fc6ec` |
| 153 | site:youtube.com "AI slop" "app" design | 10 | `30d602be-fd93-46b8-af77-44f1fc6e9fd5` |
| 154 | site:youtube.com "vibe coded" "UI" review | 10 | `a7abad17-d557-403b-b3d8-596cc463ba03` |
| 155 | site:youtube.com "AI slop" "macOS" | 10 | `9e04a907-9ad4-4b4d-be80-66930a150a73` |
| 156 | site:tiktok.com "AI slop" "web design" | 10 | `69d5f52c-aae1-4dac-8645-692fda496ffd` |
| 157 | site:instagram.com "AI slop" "UI" | 10 | `b26731fc-7bf9-4e69-a044-52f071545b15` |
| 158 | site:pinterest.com "AI slop" "design" | 10 | `c20eb477-fde2-4613-9ece-c1e92d93c934` |
| 159 | "AI slop" "UI" "imgur" | 10 | `2ce2820d-d1d0-4708-91aa-12449d196297` |
| 160 | "AI slop" "interface" "i.redd.it" | 10 | `4f0757f3-e6a7-41ec-b575-c9f57ba500b4` |

### Text and interaction patterns people call out

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 161 | "AI slop" "Seamless" "app" | 10 | `ffe73c02-669c-4281-b8ea-7d03cd9f42a0` |
| 162 | "AI slop" "empty state" design | 10 | `ba864162-2742-4ab4-8c6d-2665da2b9619` |
| 163 | "AI slop" "toast" "UI" | 10 | `84d62606-3ff7-4dd8-b08b-0c931e1f83fd` |
| 164 | "AI slop" "animation" "website" | 10 | `191523f1-a84b-4341-a16f-55426e8422ca` |
| 165 | "AI slop" "cursor" "website" effects | 10 | `020386ea-01c2-4e60-badb-947ca82042f6` |
| 166 | "AI slop" "fake" "metrics" dashboard | 10 | `0f1f2f56-3d87-4b59-9fcc-0a5661c40056` |
| 167 | "AI slop" "placeholder" "buttons" | 10 | `a9b812cc-c73a-4e09-b5df-a0e74e46193a` |
| 168 | "AI slop" "mobile" "desktop" UI | 10 | `e04f0877-0a48-4edb-8be8-e56eee933de4` |
| 169 | "AI slop" "copy" "UI" "text" | 10 | `a4ef93c4-3d12-4067-ba17-240f9400c091` |
| 170 | "AI slop" "settings page" | 10 | `9d07e5f3-d495-48dc-85b1-45fe56a26d91` |

### Follow-up: task-specific visual criticism

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 171 | site:reddit.com "AI slop" "information hierarchy" | 10 | `a40ed00b-cbf4-49ce-ba3c-19b10b343411` |
| 172 | site:reddit.com "AI slop" "contrast" UI | 10 | `a92953fa-311e-49d6-b744-2675638afe9e` |
| 173 | site:reddit.com "AI slop" "whitespace" app | 10 | `4afe5d39-55ea-4f7e-af2f-11937e4c9797` |
| 174 | site:reddit.com "AI slop" "nested" cards | 10 | `12ca81cf-9b88-43eb-90bf-ae0d7dcf0b0b` |
| 175 | site:reddit.com "AI slop" "chart" dashboard | 10 | `4e105253-1790-445f-892c-4d85c90648d4` |
| 176 | site:reddit.com "AI slop" "calendar" UI | 10 | `e77fcdd7-53f4-43e8-82d7-8aa4c2eb7512` |
| 177 | site:reddit.com "AI slop" "navigation" app | 10 | `c42af921-a097-46a0-ae94-0cfd2f85b052` |
| 178 | site:reddit.com "AI slop" "same size" cards | 10 | `5c58a790-6ba3-4685-9749-524162dcbb69` |
| 179 | site:reddit.com "AI slop" "screenshot" "feedback" design | 10 | `b89fb7e0-df1c-4cc1-a255-16fe60969681` |
| 180 | site:reddit.com "AI slop" "before vs" UI | 10 | `cc1468bc-fef6-4646-ac07-1346c65c7af9` |

### Follow-up: X images and design comparisons

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 181 | site:x.com "AI slop" "UI" "photo/1" | 10 | `7c332846-c066-47fa-9dd5-78055dd9818a` |
| 182 | site:x.com "AI slop" "design" "pic.twitter.com" | 10 | `4e3feb86-bc06-48cc-90d7-d36842a70e12` |
| 183 | site:x.com "AI slop" "before and after" UI | 10 | `a1d6a394-bbde-41db-8a6f-643c8d3c76a5` |
| 184 | site:x.com "slop" "hierarchy" design | 10 | `8df4773f-e8bf-4c48-8e5b-7f61eb1b5a17` |
| 185 | site:x.com "slop" "empty space" UI | 10 | `3f24134e-c7ea-4e73-830b-96bcd0f2f1f2` |
| 186 | site:x.com "slop" "fake metrics" | 10 | `1bed22b4-c59b-4ef3-82a6-5087b63504ff` |
| 187 | site:x.com "slop" "emoji" "UI" | 10 | `274a2517-9e9c-4eb9-acf8-3a629fa6d2e5` |
| 188 | site:x.com "AI slop" "comparison" design | 10 | `1cea1512-46ed-4434-9f42-00faa55cf117` |
| 189 | site:x.com "AI slop" "native" "Mac" | 10 | `82113758-b082-46e1-b7a1-27cf95a9893f` |
| 190 | site:x.com "slop" "UI" "contrast" | 10 | `e40511c4-fa90-402d-9077-14b1ae31f766` |

### Follow-up: independent communities and artifacts

| ID | Query | Results | Context.dev request ID |
| --- | --- | --- | --- |
| 191 | site:lobste.rs "slop" "UI" | 10 | `317996c2-52eb-44a2-bea4-73c5cf70837e` |
| 192 | site:lemmy.world "AI slop" "apps" | 10 | `62e9c458-a47f-4764-b912-7ef1bad4e087` |
| 193 | site:tildes.net "AI slop" "software" | 10 | `16a9952a-59c2-4ed0-9b9e-c6eff65f841d` |
| 194 | site:forums.macrumors.com "AI slop" apps design | 10 | `2dde9521-573c-4818-b548-f0738f53fcc6` |
| 195 | site:discourse.omnigroup.com "AI" "slop" | 10 | `1f253c23-ed43-43d1-814b-40aa8d447dd4` |
| 196 | site:discuss.getbootstrap.com "AI slop" | 10 | `736e3ed8-2869-40ef-b8d2-b43808f97cc8` |
| 197 | site:discuss.huggingface.co "AI slop" "UI" | 10 | `e912cbb8-0adc-483a-9a0e-c0e6273f0211` |
| 198 | site:dev.to "AI slop" "before" "after" | 10 | `bf253e6b-dd3f-46b9-8d18-f1d44837a975` |
| 199 | site:codepen.io "AI slop" | 10 | `774b5c98-0737-468f-8cca-65aef896cf1b` |
| 200 | site:codeberg.org "AI slop" "UI" | 10 | `413e35fe-7baa-45bb-8427-49bb2fd81083` |

## Source links

[R01]: https://www.reddit.com/r/vibecoding/comments/1s37qlp/what_are_the_signs_of_an_ai_slop_ui/
[R02]: https://www.reddit.com/r/vibecoding/comments/1tzie7q/how_to_create_a_ui_which_doesnt_scream_ai_slop/
[R03]: https://www.reddit.com/r/godot/comments/1x5xzug/ui_got_called_ai_slop_i_am_redoing_it_by_hand_and/
[R04]: https://www.reddit.com/r/aigamedev/comments/1x0rbsu/i_tried_to_fix_the_ai_slop_look_of_my_game_before/
[R05]: https://www.reddit.com/r/UXDesign/comments/1s1guo5/what_are_the_top_ai_slop_design_patterns/
[R06]: https://www.reddit.com/r/webdesign/comments/1uhuovu/preventing_the_ai_slop_look/
[R07]: https://www.reddit.com/r/macapps/comments/1tnw0gx/what_are_the_giveaways_of_a_vibecoded_app/
[R08]: https://www.reddit.com/r/macapps/comments/1n35ajs/psa_low_quality_vibe_coding_apps_are_on_the_rise/
[R09]: https://www.reddit.com/r/macapps/comments/1v49g0w/i_made_this_app_for_macos_how_do_you_think_it/
[R10]: https://www.reddit.com/r/macapps/comments/1smddee/kanban_pro_a_fully_native_project_manager/
[R11]: https://www.reddit.com/r/macapps/comments/1x5ckfg/ethical_dilemma_with_vibe_coding/
[R12]: https://www.reddit.com/r/macapps/comments/1o01a9f/rmacapps_rule_updates_on_promotion_vibe_coding/
[R13]: https://www.reddit.com/r/graphic_design/comments/1v8k5jo/getting_called_ai_slop_without_using_ai_has_me/
[R14]: https://www.reddit.com/r/webdesign/comments/1vtb5bw/can_we_stop_calling_every_design_ai_slop/
[R15]: https://www.reddit.com/r/UI_Design/comments/1w70kqb/question_about_websites_looking_like_ai_slop/
[R16]: https://www.reddit.com/r/ClaudeAI/comments/1u9sgj3/unslopui_a_claude_skill_that_flags_and_removes/
[R17]: https://www.reddit.com/r/ClaudeCode/comments/1p9srut/finally_figured_out_why_claudes_ui_generations/
[R18]: https://www.reddit.com/r/EDH/comments/1x56ah0/edhrecs_new_site_redesign_rolls_out_today/
[R19]: https://www.reddit.com/r/UI_Design/comments/1w087wj/ssx_video_game_inspired_weather_app_ai/
[R20]: https://www.reddit.com/r/ProductivityApps/comments/1vsb3t0/what_makes_a_productivity_app_considered_ai_slop/
[R21]: https://www.reddit.com/r/selfhosted/comments/1rmiwgb/apparently_we_cant_call_out_apps_as_ai_slop/
[R22]: https://www.reddit.com/r/lovable/comments/1n4olsd/generic_ai_slop_design/
[R23]: https://www.reddit.com/r/UXDesign/comments/1x7v1je/trying_to_polish_my_aislop_to_a_more_refined_look/
[R24]: https://www.reddit.com/r/AppStoreOptimization/comments/1uv2uvf/soft_launching_my_app_criticize_my_screenshots/
[X01]: https://x.com/designcoursecom/status/2080802781087089047
[X02]: https://x.com/nikunj/status/1956386966129558011
[X03]: https://x.com/alexalbert__/status/1988707509973184516
[X04]: https://x.com/stupidtechtakes/status/2098618333138722898
[X05]: https://x.com/WindowsLatest/status/2033302398488478192
[X06]: https://x.com/anshuc/status/2064829593782210844
[X07]: https://x.com/AmenaiSabuwala/status/2074763090495959200
[X08]: https://x.com/7uomoki/status/2031513147786539391
[X09]: https://x.com/i/status/2075992146356871662
[X10]: https://x.com/kvncnls/status/2089735761679491170
[X11]: https://x.com/brunolemos/status/2088239068979064905
[X12]: https://x.com/steipete/status/1930632081349955660
[O01]: https://news.ycombinator.com/item?id=49117099
[O02]: https://lobste.rs/s/wxmi0v/ai_aesthetic
[O03]: https://forums.macrumors.com/threads/between-a-golden-age-of-macos-apps-and-a-disaster-waiting-to-happen.2488160/
[O04]: https://www.threads.com/@evanyang_yfy/post/DZRjssmEs1R/the-simplest-way-to-avoid-the-ai-slop-design-is-to-stop-using-the-default/
[O05]: https://www.linkedin.com/posts/widyabayu_heres-how-we-avoid-ai-slop-ui-for-the-activity-7505958605905416192-_DSq
[O06]: https://www.producthunt.com/p/athena-5/ai-slop-when-optimization-metrics-replace-human-readability
[O07]: https://www.indiehackers.com/post/design-with-a-soul-how-to-make-your-vibe-coded-site-not-look-like-ai-slop-in-10-steps-7c9b4046a2
[O08]: https://github.com/yuwen-lu/unslop-ui
[O09]: https://github.com/yetone/kill-ai-slop
[O10]: https://world.hey.com/kostac/spot-the-slop-a-ui-designer-s-guide-to-fixing-ai-defaults-4c448c9c
[O11]: https://www.youtube.com/watch?v=j_ghsM_4MZY
[O12]: https://news.ycombinator.com/item?id=49488507
[O13]: https://github.com/JCarterJohnson/vibecoded-design-tells
[R25]: https://www.reddit.com/r/ClaudeAI/comments/1ubc02m/unslopui_v2_a_claude_skill_that_flags_and_removes/
[O14]: https://www.adriankrebs.ch/blog/design-slop/
[O15]: https://performative-ui.cncl.co/
[R26]: https://www.reddit.com/r/godot/comments/1x4yw0g/players_open_my_demo_see_a_field_with_22_possible/
[R27]: https://www.reddit.com/r/ClaudeAI/comments/1rcamhu/i_built_a_local_dashboard_to_track_my_claude_code/
[O16]: https://killaislop.com
[O17]: https://github.com/JCarterJohnson/vibecoded-design-tells/blob/main/unslop-ai-ui/comment_tell_counts.csv
[O18]: https://github.com/JCarterJohnson/vibecoded-design-tells/blob/main/unslop-ai-ui/analyze_comments.py
[O19]: https://github.com/JCarterJohnson/vibecoded-design-tells/blob/main/unslop-ai-ui/DATA_AND_GRAPHS.md
[O20]: https://github.com/JCarterJohnson/vibecoded-design-tells/blob/main/unslop-ai-ui/comment_tell_examples.md
[O21]: https://github.com/JCarterJohnson/vibecoded-design-tells/raw/refs/heads/main/unslop-ai-ui/comment_tell_counts.csv
[O22]: https://github.com/JCarterJohnson/vibecoded-design-tells/blob/main/unslop-ai-ui/analyze.py
