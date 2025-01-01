#ifndef CONFIG_H
#define CONFIG_H

#define CCQ_DIRPATH "/.local/share/ccq/"
#define DB_FPATH "/.local/share/ccq/dictionary_masterfile"
#define MEDIA_DIRPATH "/.local/share/ccq/media/"
#define DECK "main"
#define DECK_SIZE 500
#define DB "dictionary_masterfile"
#define DB_SIZE 888888
#define FACTOR (19.0/81.0) // fsrs factor
#define DECAY -0.5 // fsrs decay
#define float w[] = {
	0.4177, 0, 0.9988, 0, // initial stability for grades A/H/G/E
	7.1949, 0.5345, 1.4604,
	0.0046, 1.54575, 0.1192, 1.01925, 1.9395, 0.11, 0.29605,
	2.2698, 0.2315, 2.9898, 0.51655, 0.6621
};

#endif // CONFIG_H