# ccq
## (存储器 - **c**ún **c**hǔ **q**ì) is a minimalistic flashcard program in C
#### Description
/!\ very early stage work /!\\

As memory-light a vocabulary review app as I can come up with. ccq runs in the terminal, reads and writes to a simple text file, and uses an external algorithm to change just one field, the review date. The ccq-mine fennel script is meant to be compiled to lua and used with mpv to populate ccq's database. Details below

#### Functionalities
ccq:
- reads a csv file where every word is one line
- loads to memory the lines where the last integer (YYYYMMDD) <= today
- parses the line's fields, and quizzes the user randomly *\[randomness to be implemented\]*
- \<a\>gain, \<h\>ard, \<g\>ood, and \<e\>asy let ccq know your performance
- calls the fsrs algorithm when reviews end *\[scheduler to be implemented\]*
- overwrites the last integer *\[updating to be implemented\]*

ccq-mine: *\[all functionalities yet to be implemented\]*
- adds vim-like bindings to mpv
- directly searches subtitles in yomitan dictionaries
- enables user to choose strings within the definitions
- appends the word and its information to ccq's csv file
additionally, for non-video based media (song lyrics; audiobooks), it turns the subtitles into "books" by displaying previous/next subtitles
