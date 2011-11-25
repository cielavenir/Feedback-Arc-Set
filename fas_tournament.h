#include <stdlib.h>
#include "tournament.h"

typedef struct {
	double score;
	size_t results;
  size_t *optimal_ordering;
  size_t num_orphans;
  size_t *orphans;
} fas_tournament;

void del_fas_tournament(fas_tournament *t);

fas_tournament *run_fas_tournament(tournament *t);