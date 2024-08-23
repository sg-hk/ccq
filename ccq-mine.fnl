;; author: sg-hk
;; project: ccq-mine
;; an mpv script to mine words to ccq (存储器)
;; functionalities to add later at end of doc


(local mp (require "mp"))
(var insert-mode? false)
(var visual-mode? false)
(var visline-mode? false)
(local ignored-searches [个,了,点,着,完,什么,不])

;; INSERT mode <i>
;; search subtitles
;; <h>/<n> to move cursor
;; <i> again to move to entries
;; <t>/<s> to scroll entries 
;; <esc to exit
(mp.add_key_binding "i"
  (fn enter-insert []
    (set insert-mode? true)))

;; highlight longest entry found
;; dynamically display entries for all hits up to longest

;; VISUAL mode <v>
;; within insert mode, entries
;; select with <h> <t> <s> <n>
;; <return> to mine word and exit
;; <esc> to exit
(mp.add_key_binding "v"
  (fn enter-visual []
    (set visual-mode? true)))


(fn mine-word [word, reading, sentence]
  (
  ;; append to ~/.local/share/ccq/deck
  ;; word,reading,definition,sentence,path/to/audio
 )) 

;; VISUAL LINE MODE <V>
;; display subtitles as page
;; center current line; prev/next above/below
;; <t>/<s> to change to prev/next
;; <esc> to exit
(mp.add_key_binding "V"
  (fn enter-visline []
    (set visline-mode? true)))


;; to add later
;; mpvacious style sentence audio and snapshot
