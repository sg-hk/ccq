# ccq
## (存储器 - **c**ún **c**hǔ **q**ì) is a minimalistic flashcard program in C
#### Description
/!\ very early stage work /!\\

As memory-light a vocabulary review app as I can come up with. ccq runs in the terminal, reads and writes to a simple text file, and uses an external algorithm to change just one field, the review date. More functionalities will be added in time.

#### Functionalities
ccq:
- reads a csv file where every word is one line
- loads to memory the lines where the last integer (YYYYMMDD) <= today
- parses the line's fields, and quizzes the user
- \<a\>gain, \<h\>ard, \<g\>ood, and \<e\>asy let ccq know your performance *\[to change to just pass/fail\]*
- calls the fsrs algorithm when reviews end *\[for now just implementing I*2 algo\]*
- overwrites the last integer *\[updating to be implemented\]*

ccq-parse: *\[all functionalities yet to be implemented\]*
- takes string(s) of Chinese (Mandarin) text as argument
- lets user look up parsed entries in yomitan dictionaries
- words can then be added to the ccq database for review

ccq-mine: *\[all functionalities yet to be implemented\]*
- transforms mpv subtitles so that ccq-parse can search them
additionally, for non-video based media (song lyrics; audiobooks), it turns the subtitles into "books" by displaying previous/next subtitles as a page
