# VisualPacker

## What is it about

VisualPacker is a tool that allows linux users to be able to visually manage their packages, for this we will need 2 things, first the project needs to be in C, sencondly it needs to have a GUI, this GUI needs to run as a desktop app, non electron (accepting ideas on how to do this).

## How would this work

When you open VisualPacker you are going to get into the GUI, the GUI is a simple tree of files where every file is a package, every package when you click it shows it dependencies.

### How to recognize dependencies

If a package isn't a dependencie to any other package then it has his own branch, if a package is inside another bigger package it only will show as a branch from another branch (I need to find the name for this), every dependency that is a dependecy for another package also, it needs to have a "*" at the end of its name and below everything it needs to clarify what the * means.

## Utilities
Besides every package there needs to be 2 buttons, the first one to look for updates, the other one to delete.

Those are the basics for this project

---

# Decisions

These are settled. If one needs to change, change it here first.

| Question | Decision |
|---|---|
| Language | C (C11). No C++ anywhere, including in libraries we link. |
| GUI toolkit | **GTK4** — a pure C API, and its tree widgets expand lazily, which this project needs. |
| Package data | **libalpm**, the C library pacman itself is built from. No shelling out, no parsing text. |
| Multi-distro | One backend interface. pacman is the only implementation for now. |
| Root access | The GUI **never** runs as root. Privileged commands go through `pkexec` (polkit). |
| `*` means | The package is required by **two or more** other packages. |
| Top-level branch | A package that **nothing** depends on. A toggle switches to "only what I installed". |

**The name you were looking for:** a package that appears under another package is a
**transitive dependency**, and the whole structure is a **dependency graph**. Calling it a
tree is the useful simplification for the UI, but see the warning below.

## Two things that are not obvious

**1. It is a graph, not a tree.** Dependencies form loops — on this machine `krb5` and
`lib32-krb5` require each other. A naive "draw the tree" function never finishes. Two rules
fix it, and both are required:
- build a package's children only when its row is actually expanded, never ahead of time
- if a package already appears among its own parents, draw it but refuse to expand it

**2. Per-package upgrades can break Arch.** Installing one package against a newer
repository state is called a *partial upgrade* and is the most common way Arch systems
break. Detecting updates is completely safe; *applying* one is not. See Step 8.

---

# Roadmap

Each step is small enough to build, run and look at on its own, and ends in one commit.
Nothing moves to the next step until the current one runs.

### Step 0 — Skeleton
Makefile with a release build (optimised, hardened) and a debug build
(AddressSanitizer + UndefinedBehaviorSanitizer + `-Werror`), plus `make clean`.
*Done when:* `make` succeeds on an empty `main.c` that prints a line.

### Step 1 — Read the packages
Open the pacman database with libalpm and print every installed package with its version
and size to the terminal. No GUI, no dependencies yet.
*Done when:* the count matches `pacman -Q | wc -l`.

### Step 2 — Build the dependency graph
For every package, resolve what it depends on, including virtual names (something that
depends on `sh` must find `bash`). Then compute the reverse direction: who requires whom.
From that falls out everything the UI needs — which packages are top-level branches
(nothing requires them) and which get a `*` (two or more require them).
*Done when:* a CLI can print the dependency tree of any package, cycles included, and the
numbers agree with `pacman -Qi`.

### Step 3 — A window on screen
A GTK4 window listing the top-level packages in a flat list. No expanding yet.
*Done when:* it opens on your desktop and shows real package names.

### Step 4 — The tree
Make rows expandable, building children only on expand. Mark `*` on shared packages,
stop at cycles, and put the legend explaining `*` below the tree.
*Done when:* you can click down several levels without the app freezing.

### Step 5 — Details for the selected package
A side pane: version, description, installed size, how many packages it depends on, and
the full list of packages that require it.
*Done when:* clicking any row fills the pane.

### Step 6 — Finding things
A search box over the top-level branches, and a toggle between "nothing depends on it"
and "only what I installed".
*Done when:* typing narrows the list as you type.

### Step 7 — Detecting updates (read-only)
Compare installed versions against the repository databases and show, per package, that a
newer version exists and which repository it comes from. No root, no network, nothing
changed on your system.
*Done when:* our list of updates matches `pacman -Qu` exactly.

### Step 8 — The update button ⚠️ *decision needed*
Wire the first button from the spec. Must be decided before it is written:
- (a) the button runs a **full** `pacman -Syu` — the only safe option on Arch, or
- (b) it upgrades **just that package**, matching the spec literally, with a warning, or
- (c) it only shows status and copies the command for you to run.

### Step 9 — The delete button ⚠️ *decision needed*
Wire the second button. Because we already have the whole graph, we can show — with no
root and no risk — exactly what would break and which dependencies would be left orphaned
before anything happens. Then either `pacman -R` (just this one) or `pacman -Rs` (plus its
now-unused dependencies), run through `pkexec`.

### Step 10 — Make it a real desktop app
A `.desktop` file so it appears in your launcher, an icon, keyboard navigation,
`make install`, and a clear error window when the package database cannot be read.

### Step 11 — A second distro
Add a Debian or Fedora backend behind the existing interface. Deliberately last: it cannot
be tested on this machine, and untested code that claims to work is worse than none.

---

# Ground rules for the code

- **Performance and memory safety come first.** Prefer designs where a whole class of bug
  is impossible over designs that merely avoid it carefully.
- **Every step runs under the sanitizers** before it is committed.
- **Nothing in the GUI knows what distro it is on.** Only one file does.
- **Nothing in VisualPacker runs as root.** It builds commands; polkit runs them.
- **No untested claims.** If something was not actually run, the commit says so.

## Building

```sh
make          # optimised, hardened build
make debug    # AddressSanitizer + UndefinedBehaviorSanitizer
make check    # run the test suites under the sanitizers
```
