![ccq_logo](/ccq.png "ccq")
## (存储器 - **c**ún **c**hǔ **q**ì) 

[![ccq_demo](https://asciinema.org/a/hNx7iqkgS4Yo0wg5bDPBEUGQQ.png)](https://asciinema.org/a/hNx7iqkgS4Yo0wg5bDPBEUGQQ)

very simple flashcard program for language learning. as a challenge, almost all the code consists of low-level system calls (write, read, lseek, ...) instead of stdio/string libraries. one of the consequences is that ccq is *extremely* fast.

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
