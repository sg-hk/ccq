# ccq
## (存储器 - **c**ún **c**hǔ **q**ì) is a minimalistic flashcard program in C
#### Description

simple, command line flashcard tool that strives for efficiency and minimalism:
the flash card utility does the following
- parses a deck in json format
- lets user review due cards
- reschedules the cards accordingly (fsrs will be implemented at some point)
the dictionary utility does the following
- lets user input a string (argv[1]) and then choose a starting point for the dictionary search
- recursively looks up characters for dictionary hits
- returns the reading and the definition

the command line nature of the tool allows for easy piping and integration in a language learning workflow

to do:
- card creation utility
- add -h flag and/or man page
- fsrs scheduler
