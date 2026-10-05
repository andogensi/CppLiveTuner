# Security and migration

LiveTuner runs inside its host process. Anyone allowed to change its configuration
can influence that process. Treat configuration writers as operators with that
authority; the library does not implement user authentication or change approval.

## Deployment

- For local development, configurations and their link targets are trusted project
  files. Reading through symlinks is supported by default, including links from a
  build directory into the source tree. The library still never creates a missing
  configuration or writes through a link.
- Define `LIVETUNER_STRICT_FILE_READS=1` in every translation unit only when the
  deployment requires rejecting final symlinks/reparse points. This is opt-in;
  ordinary local development does not require the service/operator controls below.
- Use fixed absolute paths in a directory owned by the application administrator.
  For services, protect every parent directory and any permitted link target with
  OS permissions/ACLs. Strict reads are not a sandbox for attacker-writable ancestor
  directories, network filesystems, or a malicious filesystem driver.
- Run with the least privileges needed. Do not expose tuning files to untrusted
  users or use the library as the authorization boundary for a privileged service.
- For builds that must not tune at runtime, define `LIVETUNER_ENABLE_RUNTIME=0`
  consistently in all translation units. Reads and watching are disabled,
  bound defaults remain usable, and optional nlohmann saves are disabled.
  The default remains `1` for existing development workflows.
- Application-specific numeric ranges, enums, cross-field constraints, operator
  identities, approvals, audit retention and rollback generations belong to the
  host application. These controls are not provided by this patch.

Windows default reads use normal reparse processing, rather than rejecting every
reparse attribute. Actual OneDrive hydration/synchronization has not been tested;
strict mode can reject such placeholders and explicit saves still reject reparse
points. Filesystem/cloud I/O can block even though polling never adds retry sleeps.
Keep latency-sensitive tuning files on a local filesystem.

## Compatibility changes

Read APIs no longer create missing files. Create the configuration during trusted
setup before calling them. Missing files produce an error; blocking reads wait
for a valid file, and timeout reads remain bounded. JSON must be valid JSON (no
comments or trailing data). The nlohmann adapter retains its empty-file-as-empty-
object behavior.

`Params::update()`/`poll()` and `LiveTuner::try_get()` make at most one bounded
read attempt per call, without retry sleeps. Matching timestamps (including
failed probes) are cached for 10 ms, then contents are rechecked; a newly created
file or changed mtime bypasses that cache. Unchanged consecutive errors are logged
once, without clearing `last_error()`. `Params`' legacy retry settings remain
stored for source compatibility but never introduce waits into polling.
`LiveTuner` retry settings apply to blocking/asynchronous waits only.
`NlohmannParams::update()` also probes periodically so linked targets outside the
watched directory can change; unchanged disk content does not discard unsaved `set()` edits.

The supported INI/YAML subset now rejects the whole file on a malformed data line.
`Params::bind()` updates are all-or-nothing: any invalid bound value rejects the
snapshot, while absent keys use their binding defaults. Neither behavior silently
skips invalid input. See the Quick Start in README for required file setup.

Explicit `NlohmannParams::save()` overwrites only an existing regular single-link
file, after validating the opened handle. It rejects missing files, symlinks,
reparse points and hard links; it checks write errors. Saves are not transactional
filesystem replacements, so publishers should use their own approved atomic
replace workflow where crash-safe persistence is required.

All file readers enforce a 1 MiB actual-byte limit, including growth after opening.
Parsers reject nesting over 64 containers, more than 4096 entries/structural
items, keys over 1024 bytes and values over 64 KiB. Limits are conservative:
JSON escaping counts toward the input string budget and nested containers count
toward the item budget. Flattened keys are also bounded. Numeric overflow is an
error; the embedded numeric parser rejects ERANGE (including underflow).
External backends can differ on representable underflow, but all reject nonfinite
configuration values. Invalid input is never partially published. Params validates
all built-in bound types before committing a snapshot; user-defined extraction
and assignment operators remain trusted application code.
The nlohmann adapter also rejects out-of-range arithmetic conversions, including
elements of vectors, arrays and string-keyed maps. Representable fractional-to-
integer conversions still truncate as before. Custom types and other container
deserializers remain application-controlled and must validate their own ranges.

Both asynchronous APIs snapshot the path and settings when called. Changing
`set_file()` affects later operations. Pending operations move with their owner.
Destruction and `reset()` request cancellation and wait for workers. A cancelled
future throws `livetuner::AsyncCancelled`; a pending callback is not invoked.
Async reads do not mutate the owner's `last_error()`; use the future's exception
or callback diagnostics. A callback may destroy its LiveTuner: the current worker
then finishes using only its independently owned state. Other callbacks are joined.
Keep callback captures alive until completion. Exceptions from callback-style
async are reported through the logger rather than terminating the process.

Params change callbacks still run on the thread calling `update()`/`poll()`.
The background watcher owns a shared change flag, so move construction/assignment
does not retain a pointer to the previous Params. The internal FileWatcher is now
non-movable. Public object destruction/move must not race external member calls,
and a Params change callback must not destroy/move its own Params. Access to bound
application variables from other threads still requires application synchronization.

Logger accessors and configuration/error accessors return snapshots rather than
mutable references. Change the logger with `set_log_callback()`. Diagnostic
callbacks execute after instance locks are released. Recursive logging is
suppressed to prevent infinite recursion; exceptions during deferred diagnostic
delivery are contained and sent to the default diagnostic sink. Log messages
escape control characters and are capped at approximately 2 KiB. Raw ErrorInfo
fields returned to an application must still be escaped by its own log adapter.

## Building and CI

CMake 3.20 or newer is required. Define `LIVETUNER_IMPLEMENTATION` in exactly one
translation unit. `LIVETUNER_TEST_OPTIONAL_BACKENDS=ON` downloads hash-verified
picojson and nlohmann test dependencies; the default library has no network fetch.

```sh
cmake -S . -B build -DLIVETUNER_BUILD_TESTS=ON -DLIVETUNER_TEST_OPTIONAL_BACKENDS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure --no-tests=error
```

CI runs Windows, Linux (GCC and Clang), and macOS in Debug and Release, with
always-active test checks and per-test timeouts. It also runs ASan/UBSan, TSan,
bounded libFuzzer, the installed package consumer and a failure-suppression policy
check. Native watcher shutdown must pass on all three OSes. TSan's Linux CI job
reserves a compatible shadow address range explicitly; sanitizer failures are
never retried or converted to success.

Repository administrators should make **Required CI** a required pull-request
check and protect the default branch against bypass/direct pushes. The workflow
cannot enforce repository-level branch protection by itself. Signing releases,
SBOM/provenance publication, a security response owner and incident procedures
still require project/organization decisions.
