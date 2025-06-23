# Doom.sh

Doom running entirely in pure bash.

## How to run the bash version

1. Grab the rundoom.sh from [the latest release](https://github.com/FoxMoss/doom.sh/releases).
2. Install bash if you're on linux refer to your package manager or on windows see [git bash](https://www.stanleyulili.com/git/how-to-install-git-bash-on-windows)
3. Open bash. Either by running the `bash` command or by clicking the executable.
3. Run `bash rundoom.sh` (`cd` into the directory where you installed it)

## Run the reference implementation

1. Go to [foxmoss.github.io/terminaldoom](https://foxmoss.github.io/terminaldoom/)

## Controls

| Key | Mapped Key |
| -------------- | --------------- |
|`4` | `KEY_LEFTARROW` |
|`6` | `KEY_RIGHTARROW` |
|`2` | `KEY_DOWNARROW` |
|`8` | `KEY_UPARROW` |
|`7` | `KEY_ESCAPE` |
|`\n` | `KEY_ENTER` |
|`1` | `KEY_TAB` |
|`!` | `KEY_F1` |
|`@` | `KEY_F2` |
|`#` | `KEY_F3` |
|`$` | `KEY_F4` |
|`%` | `KEY_F5` |
|`^` | `KEY_F6` |
|`&` | `KEY_F7` |
|`*` | `KEY_F8` |
|`(` | `KEY_F9` |
|`)` | `KEY_F10` |
|`_` | `KEY_F11` |
|`+` | `KEY_F12` |
|`-` | `KEY_BACKSPACE` |
|`p` | `KEY_PAUSE` |
|`=` | `KEY_EQUALS` |
|`]` | `KEY_MINUS` |
| -------------- | --------------- |

## Acompanying Repos
[terminaldoom](https://github.com/FoxMoss/terminaldoom) is a copy of the doom source with changes to match the RISCV equivalent.

[rv32emu-doom](https://github.com/FoxMoss/rv32emu-doom) is the reference implantation I used to verify accuracy of the bash implantation.

## Credit
Huge credit to Velzie for starting the bash riscv emulator! And Chlonr for making the reference implantation that the bash emulator was based on, as well as starting embedded doom which this project uses a modified copy.
