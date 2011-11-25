#include "fas_tournament.h"
#include "permutations.h"
#include <string.h>
#include <assert.h>

#define ACCURACY 0.001
#define JUST_BRUTE_FORCE_IT 8

void del_fas_tournament(fas_tournament *t){
  free(t->optimal_ordering);
  free(t->orphans);
  free(t);
}

int tournament_compare(tournament *t, size_t i, size_t j){
	double x = tournament_get(t, i, j);
	double y = tournament_get(t, j, i);

	if(x <= y - ACCURACY) return -1;
	if(y <= x - ACCURACY) return +1;
	return 0;
}

double score_fas_tournament(tournament *t, size_t count, size_t *data){
	double score = 0.0;

	for(size_t i = 0; i < count; i++){
		for(size_t j = i + 1; j < count; j++){
			score += tournament_get(t, data[i], data[j]);
		}
	}

	return score;
}

// IMPORTANT: Returns an INDEX not an ELEMENT
size_t pick_a_pivot(tournament *t, size_t count, size_t *items){
	return rand() % count;
}


static inline void swap(size_t *x, size_t *y){
	if(x == y) return;
	size_t z = *x;
	*x = *y;
	*y = z;
}

double brute_force_optimize(tournament *t, size_t n, size_t *items){
	if(n <= 1) return 0.0;
	if(n == 2){
		int c = tournament_compare(t, items[0], items[1]);
		if(c > 0) swap(items, items+1);
		return tournament_get(t, items[0], items[1]);
	}

	double best_score = -1.0;

	size_t *working_buffer = malloc(sizeof(size_t) * n);
	memcpy(working_buffer, items, n * sizeof(size_t));

	while(next_permutation(n, working_buffer) < n){
		double score = score_fas_tournament(t, n, working_buffer);

		if(score > best_score){
			best_score = score;
			memcpy(items, working_buffer, n * sizeof(size_t));
		}
  }

	free(working_buffer);

	return best_score;
}

void kwik_sort(tournament *t, size_t count, size_t *items){
	if(count <= JUST_BRUTE_FORCE_IT){
		brute_force_optimize(t, count, items);
		return;
	}

	size_t i = pick_a_pivot(t, count, items);
	size_t v = items[i];

	swap(items + i, items);

	// Simple three-way partition
	// While sorting we have:
	// [< v | ==v | ??? | > v ]

	size_t *unsorted_start = items + 1;

	size_t *lt_end = items;
	
	// WARNING: Past end of array
	size_t *gt_begin = items + count;

	while(unsorted_start < gt_begin){	
		int c = tournament_compare(t, *unsorted_start, v);

		if(c < 0){
			swap(lt_end, unsorted_start);
			lt_end++;	
			unsorted_start++;
		}
		else if (c == 0){
			unsorted_start++;
		} else {
			gt_begin--;
			swap(unsorted_start, gt_begin);
		}	
	}

	kwik_sort(t, (lt_end - items), items);
	kwik_sort(t, (items + count - gt_begin), gt_begin);

	if((gt_begin - lt_end) < count){
		kwik_sort(t, (gt_begin - lt_end), lt_end);
	}
}




fas_tournament *run_fas_tournament(tournament *t){
	if(t->size == 0) return NULL;

	fas_tournament *ft = malloc(sizeof(fas_tournament));

	ft->num_orphans = 0;
	ft->orphans = NULL;

	size_t n = t->size;

	ft->results = n;
	size_t *results = malloc(sizeof(size_t) * n);
	for(size_t i = 0; i < n; i++){
		results[i] = i;
	}
	ft->optimal_ordering = results;

	kwik_sort(t, n, results);

	ft->score = score_fas_tournament(t, n, results);

	return ft;
}
