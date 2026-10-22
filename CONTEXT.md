# Computer monitoring

The product monitors and assists managed computers. The main workflow opens a computer list and acts directly on authorized computers. Rooms, classes, seats, courses, and lessons are not required domain objects.

## People and deployment

**Organization**: The owner of a self-hosted deployment's computers, accounts, and policies. The first rollout can use school-owned computers without imposing school structure on the product.

**Operator**: A person authorized to view or act on specified computers.
_Avoid_: Teacher, master, controller when naming this product role.

**Computer user**: The person using a monitored computer. Their identity is distinct from the computer and its OS account label.
_Avoid_: Student or learner as a required product role.

**Administrator**: A person authorized to configure accounts, computer access, enrollment, and policy.

**IT operator**: A person authorized to install, update, back up, and recover the deployment. This role does not inherently grant screen observation.

**Administrative delegation**: Explicit authority to grant particular capabilities within a defined account/computer scope and ceiling. It is separate from permission to perform those operations.

**Security authority**: The deliberately trusted administrative capability for protected accounts, delegation ceilings, and exceptional trust/recovery review. It can explicitly approve new operation rights; ordinary IT roles do not inherit it.

**Recovery-only credential**: Bounded verification material usable only to re-establish an independently verified account's credentials. It cannot authenticate normal computer operations or let resetting IT become that account.

**Server**: The organization-operated coordination service for identities, permissions, configuration, and action metadata. Media can travel directly or through an authorized relay.

**Active authority**: The single exact trusted server identity currently admitted by a peer. Copied databases, addresses, or larger claimed generations do not replace it.

**Authority cutover**: Independently approved movement to a fresh issuing identity, with old-host fencing and separately verified peer adoption. Offline peers remain Pending Trust Update until reconciled.

## Computers and access

**Computer**: An enrolled physical endpoint with its own identity and capabilities.
_Avoid_: Participant when referring to hardware.

**Computer inventory**: The saved, preconfigured collection of enrolled computers. Being listed is distinct from being online or currently observable.

**Monitoring inclusion**: The remembered choice of which authorized computers appear in the overview. Optional group and computer checkboxes edit this choice. It grants no permission.

**Displayed computers**: Included computers after the current search and status filters. This is the default target set for a new bulk action when no tiles are selected.

**Action selection**: Temporary selected overview tiles for the next command. This is separate from monitoring inclusion and clears when inclusion, search, or status filters change.

**Desktop session**: A particular signed-in operating-system desktop on a computer. Version 1 selects only the physical/primary console shown on that computer's own screen. User, lock, attachment, boot, or worker changes invalidate its old generation; a session number is not reusable authority. RDP and background/virtual user sessions are unsupported.

**Access scope**: The explicit computer IDs on which an account may perform specified operations. Names and discovery results do not confer authority.

**Monitoring session**: One operator's bounded authority to observe a chosen set of computers. It is separate from their login session and each computer's OS desktop session.
_Avoid_: Lesson or class session.

**Observation**: Authorized live viewing through a preview or detailed window.
_Avoid_: Recording when nothing is archived.

**Assistance**: A temporary interaction in which an authorized operator controls a computer's current desktop.

**Screen sharing**: Sending an authorized display/window to a fixed set of receiving computers.

**Sharing receiver branch**: One recipient's independently admitted presentation. Only its sharing owner or scoped authorized IT can explicitly stop it. Automatic safety teardown still applies. Ending one branch does not end other recipients or grant remote-control authority.

**Attention mode**: A temporary managed-computer state displaying an operator's message and restricting ordinary interaction where the platform and policy permit it.
_Avoid_: OS lock when referring to this product's temporary state.

**Enrollment**: The administrator-approved association of a unique computer or manager identity with a deployment.

**Installation incarnation**: The independently provisioned local identity of one approved endpoint installation, bound to its endpoint key. It is not a hostname, address, or claimed hardware fingerprint.

**Identity quarantine**: A denied-access state after authenticated competing endpoint claims. IT must verify the intended machine, retire the copied key, and approve fresh identities/access before use.

**Recovery quarantine**: Restored identities/access remain inactive until complete trusted later history or independent fresh IT validation establishes current authority.

**Security revision**: A signed release authority's monotonic security counter. It is separate from a displayed application version and its compatibility.

**Security floor**: The lowest security revision an installer/updater may accept. Valid signatures and authorized functional rollback do not lower this floor.

**Capability**: An operation that is supported, currently available, and permitted for a particular computer/desktop.

**File request**: A scoped request for computer users to submit chosen files. It does not authorize browsing their home directories.

**File recipient scope**: Delivery bound before dispatch to the verified intended local user and enrolled computer. Queued files wait for that same user to return before expiry; a new user cannot become their recipient or inherit their submission.

**Intended-user inbox**: A product-owned private delivery destination for the verified recipient on an enrolled computer. User change pauses unfinished delivery; it never converts into a shared computer inbox.

**Security State Unavailable**: Required trusted persistence cannot be read/committed reliably. New authority/effects stop while leases and independent safety cleanup remain effective; scoped repair and durable reconciliation are required before resuming.

**Action target snapshot**: The immutable computer IDs captured for one action. Search, selection, and later connections cannot silently expand it.
