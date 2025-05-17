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

I find ccq works very well with (n)vim; asciinema doesn't capture the screen well within the editor, so I haven't shown it in the demo. You can try yourself with the nvim keybinding below. Select a Chinese word, then hit leader + a.

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


As a challenge, almost all the code consists of low-level system calls (file descriptior reads and writes, byte offset seeks, ...) instead of stdio/string library functions. There is minimal overhead, and ccq is fast, but this approach introduces a large amount of I/O syscalls for searching. Buffering through mmap would make sense here; it's a possible future feature.
