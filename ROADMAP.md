# Roadmap

Audionaut is developed by one person, so this is a direction, not a schedule.
Nothing here has a date, and the order can change.

## User requests come first

As a rule of thumb, what users ask for and report goes ahead of the plans
below. If something is missing or gets in your way, say so in
[GitHub Discussions](https://github.com/kvoltmer/Audionaut/discussions) or
open an [issue](https://github.com/kvoltmer/Audionaut/issues).

## Now: effects and plugins

Effects, the mixer and plugin hosting are being finished and landed.

## Next: automation

Once effects and plugins have landed, the next larger piece of work is decent
automation.

## After that

The picture is less clear from here. What I would love to spend more time on
is music information retrieval: better analysis, and a better Auto Edit built
on it.

## Alongside: timeline drop rules

Dropping a region where clips already sit is ambiguous: should the neighbours
move, and what happens to the gaps? A small formal model (Lean 4) works out
which rules are possible at all, and where today's drop, reorder and paste
behaviour is inconsistent. It proposes one rule per gesture; the decision and
the changes in the app are still open. See
[#278](https://github.com/kvoltmer/Audionaut/pull/278).

---

For what Audionaut does today, see [Features](docs/features.md).
