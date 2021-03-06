#include "fas_tournament.h"
#include "permutations.h"
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include "optimisation_table.h"
#include "population.h"

#define ACCURACY 0.001
#define SMOOTHING 0.05
#define MAX_MISSES 5
#define MIN_IMPROVEMENT 0.00001

#define FASDEBUG(...) if(_enable_fas_tournament_debug) fprintf(stderr, __VA_ARGS__);

int _enable_fas_tournament_debug = 0;

void enable_fas_tournament_debug(int x){
  _enable_fas_tournament_debug = x;
}

tournament *new_tournament(size_t n){
  size_t size = sizeof(tournament) + sizeof(double) * n * n;
  tournament *t = malloc(size);
  memset(t, '\0', size);
  t->size = n;
  return t;
}

tournament *normalize_tournament(tournament *t){
    double max_total = 0.0;
    for(size_t i = 0; i < t->size; i++){
        for(size_t j = i + 1; j < t->size; j++){
            double total = tournament_get(t, i, j) + tournament_get(t, j, i);
            if(total > max_total) {
                max_total = total;
            }
        }
    }
    tournament *nt = new_tournament(t->size);
    if(max_total <= 0.0){
        for(size_t i = 0; i < t->size; i++){
            for(size_t j = 0; j < t->size; j++){
                tournament_set(nt, i, j, 0.5);
            }
        }
    } else {
        for(size_t i = 0; i < t->size; i++){
            tournament_set(nt, i, i, 0.5);
            for(size_t j = i + 1; j < t->size; j++){
                double tij = tournament_get(t, i, j);
                double tji = tournament_get(t, j, i);
                double bonus = (max_total - (tij + tji)) / 2;
                tij += bonus;
                tji += bonus;
                tij /= max_total;
                tji /= max_total;
                tournament_set(nt, i, j, tij);
                tournament_set(nt, j, i, tji);
            }
        }
    }

    return nt;
}

void del_tournament(tournament *t){
  free(t);
}

typedef struct {
  size_t *buffer;
  optimisation_table *opt_table;
  tournament *tournament;
} fas_optimiser;

fas_optimiser *new_optimiser(tournament *t){
  fas_optimiser *it = malloc(sizeof(fas_optimiser));
  it->buffer = malloc(sizeof(size_t) * t->size);
  it->opt_table = optimisation_table_new();
  it->tournament = t;
  return it;
}

void del_optimiser(fas_optimiser *o){
  free(o->buffer);
  optimisation_table_del(o->opt_table);
  free(o);
}

void reset_optimiser(fas_optimiser *opt){
  optimisation_table_del(opt->opt_table);
  opt->opt_table = optimisation_table_new();
}

size_t tournament_size(tournament *t){
  return t->size;
}

inline double tournament_get(tournament *t, size_t i, size_t j){
  size_t n = t->size;
  assert(i < n); 
  assert(j < n);
  return t->entries[n * i + j];
}

inline void tournament_set(tournament *t, size_t i, size_t j, double x){
  size_t n = t->size;
  assert(i < n); 
  assert(j < n);
  t->entries[n * i + j] = x;
}

double score_fas_tournament(tournament *t, size_t count, size_t *data){
	double score = 0.0;

	for(size_t i = 0; i < count; i++){
		for(size_t j = i + 1; j < count; j++){
			score +=  t->entries[data[i] * t->size + data[j]];
		}
	}

	return score;
}


static size_t count_tokens(char *c){
  if(*c == '\0') return 0;

  size_t count = 0;
  int in_token = !isspace(*c);
  
  while(*c){
    if(isspace(*c)){
      if(in_token) count++;
      in_token = 0;
    } else {
      in_token = 1;
    }
    c++;
  }

  if(in_token) count++;

  return count;
}

static int read_line(size_t *buffer_size, char **buffer, FILE *f){
  if(!*buffer) *buffer = malloc(*buffer_size);

  size_t written = 0;
  
  for(;;){
    char c = getc(f);

    if(c == EOF){
      if(written) break;
      else return 0;
    }
    if(c == '\n') break;

    if(written == *buffer_size){
      *buffer_size *= 2;
      *buffer = realloc(*buffer, *buffer_size);
    }

    (*buffer)[written++] = c;
  }

  if(written == *buffer_size){
    *buffer_size *= 2;
    *buffer = realloc(*buffer, *buffer_size);
  }

  (*buffer)[written] = '\0';

  return 1;
}

static void fail(char *msg){
  fprintf(stderr, "%s\n", msg);
  exit(1);
}

tournament *read_tournament(FILE *f){
  size_t length = 1024;
  char *line = NULL;
  tournament *t;

  if(!read_line(&length, &line, f)){
    fail("No data for read_tournament");
  }

  size_t n = count_tokens(line);

  if(n != 1) fail("Wrong number of entries in header row)");

  char *rest = line;

  n = strtoul(line, &rest, 0);

  if(line == rest){
    fail("I didn't understand the starting line");
  } else if (n <= 0){
    fail("Empty tournament");
  }

  t = new_tournament(n);

  while(read_line(&length, &line, f)){
    char *check = line;
    size_t i = strtoul(line, &rest, 0);
    if(rest == check) fail("failed to parse line"); 
    check = rest;
    size_t j = strtoul(rest, &rest, 0);
    if(rest == check) fail("failed to parse line"); 
    check = rest;
    double f = strtod(rest, &rest);
    if(rest == check) fail("failed to parse line"); 

    if(i >= n || j >= n) fail("index out of bounds");

    t->entries[n * i + j] += f;
  }
  free(line);
  fclose(f);
  return t;
}

static int tournament_compare(tournament *t, size_t i, size_t j){
	double x = tournament_get(t, i, j);
	double y = tournament_get(t, j, i);

  if(x < y + ACCURACY && x > y - ACCURACY) return 0;

	if(x >= y) return -1;
	if(y >= x) return +1;
	return 0;
}

static inline void swap(size_t *x, size_t *y){
	if(x == y) return;
	size_t z = *x;
	*x = *y;
	*y = z;
}

int table_optimise(fas_optimiser *o, size_t n, size_t *items){
  tournament *t = o->tournament;
	if(n <= 1) return 0;
	if(n == 2){
		int c = tournament_compare(t, items[0], items[1]);
		if(c > 0) swap(items, items+1);
		return c > 0;
	}

  ot_entry *ote = optimisation_table_lookup(o->opt_table, n, items);

  double existing_score = score_fas_tournament(t, n, items);

  if(ote->value >= 0){
    // We already have a best calculation for this entry
    if(existing_score < ote->value){
      // We know a better way to order these
      memcpy(items, ote->data, n * sizeof(size_t));
      return 1;
    } else {
      return 0;
    }
  } else {
    size_t *best_value_seen = malloc(n * sizeof(size_t));
    size_t *pristine_copy = malloc(n * sizeof(size_t));
    memcpy(pristine_copy, items, n * sizeof(size_t));
    memcpy(best_value_seen, items, n * sizeof(size_t));

    int changed = 0;

    double best_score_so_far = existing_score;

    for(size_t i = 0; i < n; i++){
      memcpy(items, pristine_copy, n * sizeof(size_t));
      swap(items, items + i);
      table_optimise(o, n-1, items+1);
      double new_score = score_fas_tournament(t, n, items);
      if(new_score > best_score_so_far){
        memcpy(best_value_seen, items, n * sizeof(size_t));
        changed = 1;
        best_score_so_far = new_score;
      }
    }

    ote = optimisation_table_lookup(o->opt_table, n, items);
    memcpy(items, best_value_seen, n * sizeof(size_t));
    ote->value = best_score_so_far;
    memcpy(ote->data, items, n * sizeof(size_t));

    free(best_value_seen);
    free(pristine_copy);
    return changed;
  }
}

int window_optimise(fas_optimiser *o, size_t n, size_t *items, size_t window){
  if(n <= window){
    return table_optimise(o, n, items);
  }
  double last_score = score_fas_tournament(o->tournament, n, items);
  int changed_at_all = 0;
  int changed = 1;
  while(changed){
    changed = 0;
    for(size_t i = 0; i < n - window; i++){
      changed |= table_optimise(o, window, items + i); 
    }
    double new_score = score_fas_tournament(o->tournament, n, items);

    double improvement = (new_score - last_score) / last_score;
    
    changed_at_all |= changed; 
    if(improvement < MIN_IMPROVEMENT) break;
    last_score = new_score;
  }

  return changed_at_all;
}


typedef struct {
  size_t index;
  double score;
} index_with_score;

int compare_index_with_score(const void *xx, const void *yy){
  index_with_score *x = (index_with_score*)xx;
  index_with_score *y = (index_with_score*)yy;

  if(x->score < y->score) return -1;
  if(x->score > y->score) return 1;
  return 0;
}

// Insertion sort for now. Everything else is O(n^2) anyway
static void move_pointer_right(size_t *x, size_t offset){
  while(offset){
    size_t *next = x + 1;
    swap(x, next);
    x = next;
    offset--;
  }
}

static void move_pointer_left(size_t *x, size_t offset){
  while(offset){
    size_t *next = x - 1;
    swap(x, next);
    x = next;
    offset--;
  }
}

int single_move_optimise(fas_optimiser *o, size_t n, size_t *items){
  tournament *t = o->tournament;
  int changed = 1;
  int changed_at_all = 0;
  while(changed){
    changed = 0;
    for(size_t index_of_interest = 0; index_of_interest < n; index_of_interest++){
      double score_delta = 0;

      if(index_of_interest > 0){
        size_t j = index_of_interest;
        do {
          j--;
          score_delta += tournament_get(t, items[index_of_interest], items[j]);
          score_delta -= tournament_get(t, items[j], items[index_of_interest]);

          if(score_delta > 0){
            move_pointer_left(items+index_of_interest, index_of_interest - j);
            changed = 1; 
            break;
          }
        } while(j > 0);
      }

      for(size_t j = index_of_interest + 1; j < n; j++){
        score_delta += tournament_get(t, items[j], items[index_of_interest]);
        score_delta -= tournament_get(t, items[index_of_interest], items[j]);

        if(score_delta > 0){
          move_pointer_right(items+index_of_interest, j - index_of_interest);
          changed = 1; 
          changed_at_all = 1;
          break;
        }
      }
    }
  }
  return changed_at_all;
}

size_t *integer_range(size_t n){
  size_t *results = malloc(sizeof(size_t) * n);
	for(size_t i = 0; i < n; i++){
		results[i] = i;
	}
  return results;
}

int force_connectivity(fas_optimiser *o, size_t n, size_t *items){
  if(!n) return 0;
  int changed = 0;
  tournament *t = o->tournament;
  for(size_t i = 0; i < n - 1; i++){
    size_t j = i + 1;
    while(j < n && !tournament_compare(t, items[i], items[j])) j++;
    if(j < n){
      changed = 1;
      move_pointer_left(items + j, (j - i - 1));
    }
  }
  return changed;
}


int local_sort(fas_optimiser *o, size_t n, size_t *items){
  tournament *t = o->tournament;
  int changed = 0;
  for(size_t i = 1; i < n; i++){
    size_t j = i;

    while(j > 0 && tournament_compare(t, items[j], items[j - 1]) <= 0){
      changed = 1;
      swap(items + j, items + j - 1);
      j--;
    }
  }
  
  return changed;
}

int stride_optimise(fas_optimiser *o, size_t n, size_t *data, size_t stride){
  int changed = 0;
  while(n > stride){
    changed |= table_optimise(o, stride, data);
    data += stride;
    n -= stride;
  }
  changed |= table_optimise(o, n, data);
  return changed;
}



int kwik_sort(fas_optimiser *o, size_t n, size_t *data, size_t depth){
  if(n <= 1) return 0;
  if(depth >= 10) return 0;

  size_t *lt = malloc(n * sizeof(size_t));
  size_t *gt = malloc(n * sizeof(size_t));

  size_t ltn = 0;
  size_t gtn = 0;

  size_t pivot = data[random_number(n)];
 
  for(size_t i = 0; i < n; i++){
    int c = tournament_compare(o->tournament, data[i], pivot);
    if(c < 0) lt[ltn++] = data[i];
    else if(c == 0){
      if(random_number(2)){
        lt[ltn++] = data[i];
      } else {
        gt[gtn++] = data[i];
      }
    }
    else gt[gtn++] = data[i];
  }
  
  depth++;
  kwik_sort(o, ltn, lt, depth);
  kwik_sort(o, gtn, gt, depth);

  memcpy(data, lt, sizeof(size_t) * ltn);
  memcpy(data + ltn, gt, sizeof(size_t) * gtn);

  free(lt);
  free(gt);
  return 1;
}

size_t *copy_items(size_t n, size_t *items){
  size_t *copy = calloc(n, sizeof(size_t));
  memcpy(copy, items, n * sizeof(size_t));
  return copy;
}

population *build_population(fas_optimiser *o,
                             size_t n,
                             size_t *items,
                             size_t ps){
  population *p = population_new(ps, n);

  for(size_t i = 0; i < ps; i++){
    size_t *data = copy_items(n, items);
    kwik_sort(o, n, data, 0), 
    p->members[i].data = data;
    p->members[i].score = score_fas_tournament(o->tournament, n, data);
  }

  population_heapify(p);
  return p;
}

int coin_flip(){
  return random_number(2);
}

void mutate(fas_optimiser *o, size_t n, size_t *data){
  size_t i = random_number(n);
  size_t j;
  do{ j = random_number(n); } while(i == j);
  if(j < i){
    size_t k = i;
    i = j;
    j = k;
  }
  switch(random_number(5)){
    case 0:
      reverse(data + i, data + j);  
      break;
    case 1: 
      swap(data + i, data + j);  
      break;
    case 2:
      if(coin_flip()){
        move_pointer_right(data + i, j - i);
      } else {
        move_pointer_left(data + j, j - i);
      }
      break;
    case 3:
      if(j > i + 12) j = i + 12;
      table_optimise(o, j - i, data + i);
      break;
    case 4:
      local_sort(o, j - i, data + i);
      break;
  }
}

void improve_population(fas_optimiser *o, population *p, size_t count){
  tournament *t = o->tournament;
  size_t n = t->size;
  size_t *data = malloc(n * sizeof(size_t));

  for(size_t i = 0; i < count; i++){
    size_t *candidate = p->members[random_number(p->population_count)].data;
    memcpy(data, candidate, n * sizeof(size_t));
    mutate(o, n, data);
    double score = score_fas_tournament(t, t->size, candidate);
    
    if(!population_contains(p, score, data)){
      population_push(p, score, data);
    }
  }

  free(data);
}

void population_optimise(fas_optimiser *o,
                         size_t n,
                         size_t *items,
                         size_t initial_size,
                         size_t generations){
  population *p = build_population(o, n, items, initial_size);
  improve_population(o, p, generations);
  memcpy(items, fittest_member(p).data, n * sizeof(size_t));
  population_del(p);
}

void comprehensive_smoothing(fas_optimiser *o, size_t n, size_t *results){
  stride_optimise(o, n, results, 11); 
  local_sort(o, n, results);
  stride_optimise(o, n, results, 13); 
  local_sort(o, n, results);
  reset_optimiser(o);
   
  for(int i = 0; i < 10; i++){
    int changed = 0;
    changed |= stride_optimise(o, n, results, 12);
    changed |= stride_optimise(o, n, results, 7);
    changed |= local_sort(o, n, results);
    reset_optimiser(o);
    if(!changed) break;
    single_move_optimise(o,n,results);
  } 
}

size_t *optimal_ordering(tournament *t, size_t *results){
  fas_optimiser *o = new_optimiser(t);
  size_t n = t->size;
  if(results == NULL){
    results = integer_range(n);
  }

  if(n <= 15){
    table_optimise(o, n, results);
    del_optimiser(o);
    return results;
  }

  population_optimise(o, n, results, 500, 1000);
  comprehensive_smoothing(o, n, results);
  window_optimise(o, n, results, 10);
  local_sort(o, n, results);

  del_optimiser(o);
  return results;
}

size_t tie_starting_from(tournament *t, size_t n, size_t *items, size_t start_index){
  for(size_t i = start_index+1; i < n; i++){
    for(size_t j = start_index; j < i; j++){
      int c = tournament_compare(t, items[i], items[j]);

      if(c) return i;
    }
  }

  return n;
}

size_t condorcet_boundary_from(tournament *t, size_t n, size_t *items, size_t start_index){
  size_t boundary = start_index;

  int boundary_change = 0;
  do {
    boundary_change = 0;

    for(size_t i = start_index; i <= boundary; i++){
      for(size_t j = boundary + 1; j < n; j++){
        if(tournament_compare(t, items[j], items[i]) <= 0){
          boundary = j;
          boundary_change = 1;
          break; 
        }
      }     
    }
  } while(boundary_change);

  return boundary;
}

