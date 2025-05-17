![ccq_logo](/ccq.png "ccq")
## (存储器 - **c**ún **c**hǔ **q**ì) 

[![ccq_demo](https://asciinema.org/a/2ylrMY2vcHuRhPTXR0VmAML2i.png)](https://asciinema.org/a/2ylrMY2vcHuRhPTXR0VmAML2i)
(asciinema seems unable to capture non-ASCII, multibyte chars well - bear with the visual bugs).

very simple flashcard program for language learning

it works right now; I use it daily. some testing is required to polish edges

ARHGAP11B: reviewer and scheduler. The scheduler uses the FSRS algorithm, as that is the most advanced memory loss minimizing algorithm (technically, "review interval maximizing" algorithm).

FOXP2: database and editor. it includes a number of dictionaries that I have pooled together and reformatted more cleanly, for a total of over 900k entries.

Monolingual:

- 中华成语大词典
- 现代汉语词典
- 现代汉语规范词典
- 两岸词典

Bilingual:

- Oxford English-Chinese dictionary
- Wenlin ABC
- 500 Idioms


more info in respective directories' readmes


#### Notes

**nvim workflow**

I find ccq works very well with (n)vim; asciinema doesn't capture the screen well when I open Chinese text, so I haven't shown it in the demo. You can try yourself with the nvim keybinding below. Select a Chinese word, yank it to clipboard, then hit leader + a.

```lua
function SendClipboardToCmd()
	local text = vim.fn.getreg("+")

	if not text or text == "" then return end

	text = vim.fn.shellescape(text)

	local cmd = "fox -a zh " .. text
	vim.cmd("vsplit")
	vim.cmd("terminal " .. cmd)
end
vim.keymap.set("n", "<leader>a", SendClipboardToCmd, { desc = "send clipboard to fox - a zh" })
```

Combined with the bookmarks nvim add-on, this makes for a very pleasant book-reading experience. You can look words up quickly with a couple key strokes, add definitions for later review, and drop back to the book within the same terminal window. You never need heavy, separate Python apps (Anki) or a whole browser environment (Yomitan).

**code notes**

As a personal challenge, almost all the code consists of low-level system calls (file descriptor reads and writes, byte offset seeks, ...) instead of stdio/string library functions. There is minimal overhead, and ccq is fast, but this approach introduces a large amount of I/O syscalls for searching. Buffering through mmap would make sense here; it's a possible future feature.
