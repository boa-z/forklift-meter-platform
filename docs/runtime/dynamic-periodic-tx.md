# Dynamic periodic TX

## Ownership and resources

App samples Product semantics into staging, then holds a native priority mutex for a bounded deep copy. Protocol copies the whole publication under the same short lock and releases it before encoding. Encoder inputs are const semantic values; no Core, device, IPC, storage, UI or heap access is permitted. TX workers only consume copied messages and return identified results. Protocol alone commits rolling state. App alone changes global mode; a local freshness policy never performs a global transition.

The board budget header declares overridable periodic-slot and semantic-value capacities (defaults 8 and 16); startup rejects oversized Products. Portable contracts do not cap Product tables. There are three semantic arrays and one bounded mailbox per periodic item, independent of ordinary/urgent queues. Mutexes protect copies, not encoding, driver waits or logging. First-version wire state is a Product-defined uint32 token; complex transactions remain in adapters/services.

## Publication and deadlines

Generation isolates mode/session changes. Revision increments only when value/validity/count changes, not on sample time or publish time changes. A same-value new sample refreshes sample time; republishing preserves it. Backward/future valid samples are rejected atomically under the monotonic half-range rule. Values and timestamp units are declared by Product. Revision exhaustion refuses change rather than wrapping identity.

Every periodic message carries generation, ticket, revision, acquisition/publication time, planned deadline and expiry. A generation reset resets wire state and schedules one full period later. The existing planned-deadline late-skip scheduler remains; no catch-up burst or nearest-deadline optimization. Acceptance is the first allowed deadline after Protocol acquires new semantics using that revision or newer. Snapshot acquisition does not wait for a semantic revision change: timestamp-only refreshes also copy.

Default SKIP_BUSY skips deadlines while a slot is pending/inflight. REPLACE_PENDING replaces only a not-yet-started message at a new deadline. Inflight sends cannot be replaced. Expiry defaults to one period and can be shorter; TX checks expiry and generation immediately before entering the driver. A mode change cannot cancel hardware already transmitting. Neither policy accumulates historical frames. Four urgent messages are followed by an ordinary/periodic opportunity; ordinary and periodic compete fairly, with per-entry round robin.

## Freshness and wire commit

Each definition selects its dependency span, maximum sample age (zero disables age timeout), HOLD, ENCODE_INVALID or SUPPRESS. Invalid samples remain invalid even with timeout disabled. HOLD/ENCODE_INVALID deliver fresh=false to the pure Product encoder; Product defines hold, fallback or flags. No automatic global DEGRADED transition is added.

Admission commit advances after mailbox acceptance even if later cancelled. Driver commit advances only when Protocol consumes a matching successful result. Busy, failure, queue rejection, cancelled/expired frames and old results have tested behavior. No remote acknowledgement commit policy is claimed.

The existing SDK interrupt TX path waits for completion. The HAL raises TX_DONE only on TX interrupt with TXB and TXC status set; failures use TX_FAIL. RT-Thread returns written bytes only for its successful completion. This is local controller/driver completion, not a remote application acknowledgement and not a wire timestamp. No SDK modification is required. PCAN native timestamps remain the independent wire measurement.

## Synthetic Demo and evidence

CAN0 0x3C0 at 50 ms carries local brightness, critical, driver commit; 0x2F0 at 100 ms carries speed in 0.01 km/h, ordinary, admission commit. Both use 8 bytes: value little-endian (0..1), freshness (2), counter (3), revision low 16 bits (4..5), inverted byte 0 (6), XOR bytes 0..6 (7). These are public synthetic test frames, not vehicle control. App preserves speed sample time; local brightness is sampled each App iteration.

The shell reports per-entry scheduling lateness, queue residence and driver duration separately; first-message records bind acquisition/deadline/revision/counter to a result. Diagnostic revision/ticket display is low 32 bits, wire revision is low 16 bits. They are correlation aids, not identity for runtime decisions. Physical CAN timestamps come from PCAN captures. The previous approximately 1.04 ms observation is not a requirement.

Host contract, native IPC and threaded deep-copy tests are separate from board HIL. Delivery records exact source/build/package hashes and tested bus coverage; no multi-bus, formal MISRA, rollback or power-loss claims follow from a host pass.
