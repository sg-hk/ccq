![ccq_logo](/ccq.png "ccq")
# ccq
## (存储器 - **c**ún **c**hǔ **q**ì) is a minimalistic flashcard program in C
#### Description

[WIP!] command line flashcard tool that strives for simplicity

includes [FSRS scheduling](https://github.com/open-spaced-repetition/py-fsrs/blob/main/ALGORITHM.md)


cards are expected to be found in ~/.local/share/ccq/argv[1] and follow the format
```front|back|audio|sentences|image|state|difficulty|stability|retrievability|last_review|due_date```
where:
* each line is a separate card; front is used as key and duplicate cards are not allowed
* audio and image are in the ```media/``` subfolder and are simply the filenames
* state 0 = new, 1 = young, 2 = mature
* D, S, R are float variables used in the scheduler
* last_review and due_date are simple UNIX timestamps

the scheduler works without day boundaries. adding one day to the card's due date will simply add 86 400 seconds. there is no adjusting of the weights yet

to do:
* update relevant variables to wide string types
* add: deck, dictionary, corpus search logic
* scheduler: adjustment of weights based on deck data (FSRS's ML model)
* reviewer: array processing logic for multiple sentence, image, audio
* man page
* link to dictionaries, corpus
* Makefile including man page and dictionaries
