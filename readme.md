![ccq_logo](/ccq.png "ccq")
## 存储器 
### overview

**c**ún **c**hǔ **q**ì is a very simple flashcard program for language learning. it's anki and yomitan in ~1000 lines of C!

[note!] there are probably still some bugs lurking. i use it daily though

i made this because i wanted to have my whole language learning workflow inside the terminal:
- flat text files (no sql) means grep, awk, etc. work great, and you can see exactly how your data is stored and managed
- CLI nature means it can be called from anywhere (eg, from vim to look up a word, scripts, ...)
- hackable, transparent: the codebase is intended to be simple enough to fit in a single `main.c` file and understand from A to Z

you can keep your study lists in sync across devices by push/pulling them with git. since they're plain text, you have a full view of the diffs / commit history too, which is neat

you can easily edit or delete entries by directly modifying the text files in your `~/.local/share/ccq/` folder (or your configured data directory)

`ccq` doesn't get in the way, doesn't require a separate GUI, or a whole browser environment. it's just you and some bytes waiting to be read

### usage

`ccq` operates in two main modes: review and query.

* **review:** `ccq [-n | -o | -r] [study_list]`
* **query:** `ccq -q <key> [-d <database>] [-s <study_list>]`

for detailed information on command-line options, file formats, and misc:
* `man ccq`: overview of commands and usage
* `man 5 ccq`: detailed description of the study list and  database file formats
* `man 7 ccq`: information on the internals, design choices, and FSRS implementation

(don't forget to run `mandb` after installing if these commands don't work)

a sample study list called `sample` is included and will be installed along with the database. you can run the commands on it if you like


### installation

there are no dependencies, besides:
* gcc/clang/tcc/... some C compiler
* meson, ninja

ensure you have those, then run
```bash
meson setup build --prefix "$HOME/.local"
```
for user-only install. this is preferable! installing to the default `/usr/local` means `ccq` needs to be run as root to update your cards

then:
```bash
ninja -C build
ninja -C build install
```
the latter requiring sudo if not writing in `$HOME`

that's it!

*note:* you can choose your default database and study list names in config.h.in, or pass them on through command line flags otherwise

### misc
#### dictionaries included

db includes the dictionaries i use. they're in a simple format, pipe-delimited just like the study lists, which makes awk and grep happy, and which makes the database a breeze to browse and hack into whatever workflow you enjoy

Monolingual:

* 中华成语大词典
* 现代汉语词典
* 现代汉语规范词典
* 两岸词典

Bilingual:

* Oxford English-Chinese dictionary
* Wenlin ABC
* 500 Idioms

### nvim

select a word in nvim, yank it to clipboard, then call this function to query a word within a split terminal

```lua
function SendClipboardToCmd()
	local text = vim.fn.getreg("+")

	if not text or text == "" then 
        print("ccq query: empty clipboard!")
        return 
    end

	text = vim.fn.shellescape(text)

    -- option 1: default database query, add to default study list
    local cmd = "ccq -q " .. text

    -- option 2: set variables to have specific dbs/sls
    -- local sl = "CHOOSE YOUR STUDY LIST HERE"
    -- local db   = "CHOOSE YOUR DATABASE HERE"
    -- local cmd = string.format("ccq -q %s -s %s -d %s", text, sl, db);

	vim.cmd("vsplit")
	vim.cmd("terminal " .. cmd)
end
```

and to bind this to `<leader>c`, for example:
```lua
vim.keymap.set("n", "<leader>c", SendClipboardToCmd, { desc = "send clipboard to ccq" })
```
this way you can:
- look words up quickly with a couple key strokes
- add definitions for later review
- drop back to the book within the same terminal window

if you would like to read books this way, i highly recommend the [bookmarks.nvim](https://github.com/crusj/bookmarks.nvim) plug-in. `ccq` and `bookmarks` together make for as pleasant a book-reading experience as any fuller-featured reader

let's reading!

#### to do

using ```mmap(2)``` to read the study list and database files, and drastically reduce I/O syscall overhead, would be more elegant and improve performance (on extremely large files; performance is already below perception threshold now)
