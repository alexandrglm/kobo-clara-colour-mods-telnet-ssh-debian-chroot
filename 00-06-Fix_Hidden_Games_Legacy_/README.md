# Enable Extras (Sudoku & Solitaire) for Kobo via NickelMenu

Modern Kobo firmware still contains unused, hidden system applications for **Sudoku**, **Solitaire**, **Scribble** (word game) and **Unblock it!** (sokoban-alike).  

These native extras can be launched directly from the device menu without installing third-party games or running external scripts.

These native extras can be launched by using `NickelMenu`, or `NickelDBus`


While these features can also be triggered using **KFmon** alongside PNG trigger files, using **NickelMenu** is the cleanest, most efficient method if you already have it installed.

---

## Configuration

Add the following lines to your NickelMenu configuration file (`/mnt/onboard/.adds/nm/games` or whatever nm file is:

```text
menu_item :main :Solitario :nickel_extras :solitaire
menu_item :main :Sudoku    :nickel_extras :sudoku
menu_item :main :Scribble :nickel_extras :word_scramble
menu_item :main :Unblock It :nickel_extras :unblock_it

```

![Solitaire](./screenshot_20260910_125000.png)
![Sudoku](./screenshot_20260910_125008.png)
![Scribble](./screenshot_20260910_125317.png)
![Unblock it!](./screenshot_20260910_125336.png)
