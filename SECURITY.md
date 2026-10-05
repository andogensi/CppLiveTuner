# Security policy

LiveTuner is an in-process development tool. Configuration writers have authority
to influence the host application. Deployment must protect configuration paths
and all parent directories, use a least-privileged process, and validate
application-specific values. Production builds can disable runtime tuning with
`LIVETUNER_ENABLE_RUNTIME=0`; see [deployment and migration](docs/SECURITY_AND_MIGRATION.md).

Security-relevant reports include memory/lifetime errors, concurrent-state
corruption, parser/resource-limit bypasses, unsafe filesystem writes and unsafe
callbacks. Include the revision, platform/compiler, enabled macros, minimal
reproducer, attacker preconditions and observed result. Do not include secrets
or customer configurations in reports. Coordinate privately with the maintainer
before publishing an exploit or customer data. This repository does not promise
an enterprise response SLA or support for old releases.

Changes to these boundaries require regression coverage and successful Required
CI. Compiler warnings, sanitizer failures, empty test discovery and test timeouts
must not be converted to successful checks. CI artifacts are evidence for the
tested revision and do not certify a deployment's ACLs, operator controls or
business-value validation.
