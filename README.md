# ccq
## (存储器 - **c**ún **c**hǔ **q**ì) is a minimalistic flashcard program in C
#### Description
/!\ very early stage work /!\\

As memory-light a vocabulary review app as I can come up with. ccq runs in the terminal, reads and writes to a simple text file, and uses an external algorithm to change just one field, the review date. The ccq-mine fennel script is meant to be compiled to lua and used with mpv to populate ccq's database. Details below

#### Functionalities
ccq:
- reads a csv file where every word is one line
- loads to memory the lines where the last integer (YYYYMMDD) >= today
- selects at random one line with date =< today
- \<return\> reveals the definition, and optionally other info
- \<p\> and \<f\> let ccq know you Passed or Failed
- calls the fsrs algorithm when reviews end
- overwrites the last integer

ccq-mine:
- adds vim-like bindings to mpv
- directly searches subtitles in yomitan dictionaries
- enables user to choose strings within the definitions
- appends the word and its information to ccq's csv file
additionally, for non-video based media (song lyrics; audiobooks), it turns the subtitles into "books" by displaying previous/next subtitles
