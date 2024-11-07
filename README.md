# ccq
## (存储器 - **c**ún **c**hǔ **q**ì) is a minimalistic flashcard program in C
#### Description

command line flashcard tool that strives for simplicity

includes [FSRS scheduling](https://github.com/open-spaced-repetition/py-fsrs/blob/main/ALGORITHM.md)

WIP but should work as basic reviewer and scheduler

cards are expected to be found in ~/.local/share/ccq/argv[1] and follow the format
```id|front|back|sentence|audio|image|state|difficulty|stability|retrievability|last_review|due_date```
where:
* each line is a separate card
* audio and image are in media/ subfolder and are simply the filenames
* state 0 = new, 1 = young, 2 = mature
* D, S, R are float variables used in the scheduler
* last_review and due_date are simple UNIX timestamps
the scheduler works without day boundaries. adding one day to the card's due date will simply add 86 400 seconds. there is no adjusting of the weights yet

to do:
* adjustment of weights based on deck data
* ccq-add: card creation
* ccq-update: add sentences to existing card
* ccq-parse: TUI for text parsing
