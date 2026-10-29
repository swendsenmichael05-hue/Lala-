# Veil & Vow — browser edition

Standalone, two-player, text-only fantasy game. This isolated branch contains only the browser game and its public-URL smoke tests. The repository's main branch and original project are unchanged.

Play without downloading a file or opening an editor:
https://htmlpreview.github.io/?https://raw.githubusercontent.com/swendsenmichael05-hue/Lala-/veil-vow-browser/index.html

The link uses the open-source GitHub HTML Preview renderer to execute the public HTML source. No player account is needed. This is a browser preview, not a custom-domain deployment. Public availability depends on GitHub and the preview service.

The game stores progress and novelty history locally when permitted. It has no tracking, paid APIs or external assets. New names and world configurations are checked against saved history; narrative structures are finite. Clearing storage clears repeat protection.

The browser workflow tests the actual public play link in Chromium and WebKit with mobile touch emulation, including Start, choices, helping, dice, a complete run, restarting, and resuming. Emulation is not a physical iPhone test.
