# Known Limitation Entry — Template

> This file is **not** itself a document. It defines the standard format for
> entries appended under the `## Known Limitations` section of a landed module's
> `overview.md`. Copy the block below verbatim and fill in.

---

### <Short title, ≤60 chars>

| Field | Value |
| --- | --- |
| **Discovered** | YYYY-MM-DD at commit `<short-hash>` |
| **Severity** | low \| medium \| high \| blocker |
| **Reproducibility** | always \| frequent \| rare \| theoretical |
| **Roadmap follow-up** | `roadmap/<module-or-v2>/` — one line on what is being explored, or `none yet` |

**Symptom.** What is observed (not what is assumed). One short paragraph.

**Trigger.** The minimal conditions under which it shows up. If you cannot
state them precisely, the limitation is not yet ready to be filed — investigate
further first.

**Why the original design did not foresee this.** One or two sentences. This
prevents future readers from concluding the original authors were careless.
If the original design **did** foresee it but accepted the risk, say so.

**Workaround (if any).** Code-level or operational mitigation currently in
place. `none` is a valid answer.

---

## Rules for appending entries

- **Append only**, never edit older entries. If new information arrives,
  add a fresh entry that references the older one.
- **Severity = blocker** triggers an immediate roadmap module per `AGENTS.md §3`.
- **No more than 8 entries per module.** Beyond that, the module is failing,
  and the right action is a `<module>_v2` rather than further documentation.
