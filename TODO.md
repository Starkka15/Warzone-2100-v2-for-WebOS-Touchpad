# TODO

## Open bugs

- **"Error! (Check your logs!)" banner**: `pie_ImageFileID: Out of range 1: 24914`,
  seen in campaign mission 1 (cam1a) around the time the second enemy base falls.
  The image is skipped and the game carries on. Since 2.3.1 the message carries the
  caller's address; map it with `arm-none-linux-gnueabi-addr2line` on the unstripped
  binary of that exact build.

## Features from newer Warzone 2100 (potential)

Taken from the upstream ChangeLog (2.3.9 to 4.7.0) as features that can be rebuilt
on this 2.3 engine. The upstream code itself mostly cannot be reused: newer
versions moved AI, campaign and much of the game logic to JavaScript and the
stats to JSON.

Suggested first batch (what is awkward without a keyboard):

- [ ] Quicksave and autosave (3.4): one-tap save, and protection against crashes
      and the app being swiped away
- [ ] "Continue last save" on the main menu (3.4)
- [ ] Unit groups panel (4.4): groups are Ctrl+1..9 only, impossible on the
      TouchPad; tap buttons to select and assign
- [ ] Units from a factory joining a group automatically (4.5)
- [ ] Rotate a building during placement (4.0), as a touch button
- [ ] Quit confirmation and more information on the pause screen (4.1, 4.0)

Smaller extras:

- [ ] Indicator for unused oil derricks (4.0)
- [ ] Selected units and structures blink on the minimap (4.2)
- [ ] Show the rank a unit will have if produced next (4.3)
- [ ] Alert when the laser satellite is ready (4.5)
- [ ] Unit and kill counts (3.4)

Larger:

- [ ] Diagonal line building; building modules and derricks in a row (4.0, 4.1)
- [ ] Formation speed limiting: a group moves at its slowest unit's pace (4.5)
- [ ] Break units out of endless circling after a while (4.6)
- [ ] Secondary orders Pursue / Guard / Hold and range orders (3.3)
- [ ] Newer soundtracks, Aftermath (4.3) and Reclamation (4.7), as Ogg Vorbis
      (check their licences first)

Not portable (they need upstream's JavaScript scripting): the Cobra, BoneCrusher
and newer Nexus AIs; the campaign remaster, difficulty overhaul and tweaks; the
Reclamation and Fractured Kingdom campaigns; Ultimate Scavengers; the in-game
guide. Online play needs the current lobby and network protocol, which 2.3 cannot
speak.

## Other

- [ ] Larger menus and text for touch (2.3.0 had them; that source is lost)
