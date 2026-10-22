# Third-party provenance

SvitloBoard 2 is an independent firmware, not a Tasmota fork or distribution. Its module
boundaries and user-facing behavior are specific to SvitloBoard.

The implementation intentionally follows proven embedded-system ideas used by Tasmota,
including cooperative periodic work, last-known-good Wi-Fi selection, explicit network lifecycle
events, rotating validated configuration records, recovery access, and compact asynchronous web
updates. Any future source fragment adapted directly from Tasmota must keep its original copyright,
file-level SPDX identifier, and a link to the exact upstream revision in this document.

Upstream project: https://github.com/arendst/Tasmota
Upstream license: GPL-3.0-or-later

At this foundation revision, the source files in `firmware/` are newly written for SvitloBoard;
no Tasmota source file has been copied verbatim.
