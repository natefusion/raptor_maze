#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/random.h>

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Creates a struct with the given type T to mimic a C++ vector.
 *   All vector types will begin with "Vec_" and end with the typename literally.
 */
#define Vec_define(T) typedef struct { T *data; int len; int capacity; } Vec_##T

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Allocates a vector with the length set to its capacity.
 */
#define Vec_alloc_full(vec, n)                                      \
    do {                                                            \
        (vec)->data = Arena_alloc(&arena, n, sizeof(*(vec)->data)); \
        (vec)->len = n;                                             \
        (vec)->capacity = n;                                        \
    } while (0)

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Allocates a vector with the length set to zero.
 */
#define Vec_alloc_empty(vec, n)                                     \
    do {                                                            \
        (vec)->data = Arena_alloc(&arena, n, sizeof(*(vec)->data)); \
        (vec)->len = 0;                                             \
        (vec)->capacity = n;                                        \
    } while (0)

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Places a value in the next free place in the Vec.
 */
#define Vec_push(vec, val) (vec)->data[(vec)->len++] = (val)

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Places a value in the next free place in the Vec.
 *   When there are no free places, make some more.
 */
#define Vec_push_extend(vec, val)                                       \
    do {                                                                \
        if ((vec)->len >= (vec)->capacity)  {                           \
            if ((vec)->capacity == 0) (vec)->capacity = 2;              \
            else (vec)->capacity *= 2;                                  \
            (vec)->data = Arena_realloc(&arena,                         \
                                        (vec)->data,                    \
                                        (vec)->capacity,                \
                                        sizeof(*(vec)->data));          \
        }                                                               \
        Vec_push(vec, val);                                             \
    } while(0)

typedef struct {
    int u;
    int v;
} Edge;

typedef struct {
    int data[4];
    int len;
} Vec4_int;

typedef void* voidptr;

Vec_define(voidptr);
Vec_define(Edge);
Vec_define(int);
Vec_define(Vec_int);
Vec_define(Vec4_int);
Vec_define(char);

typedef Vec_char String;
typedef Vec_Vec4_int Graph;
typedef Vec_voidptr Arena;

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Allocates an array on the heap.
 *   Returns a pointer to an array of length 'n' with element size 'size'.
 *   Places the pointer into 'arena' for safekeeping.
 */
void *Arena_alloc(Arena *arena, int n, int size);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Reallocates array p on the heap.
 *   Returns a pointer to an array of length 'n' with element size 'size'.
 *   Places the pointer into the arena for safekeeping.
 */
void *Arena_realloc(Arena *arena, void *p, int n, int size);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Sets all members in 'arena' to zero.
 */
void Arena_init(Arena *arena);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Frees all pointers created by 'Arena_alloc' or 'Arena_realloc'.
 */
void Arena_deinit(Arena *arena);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Creates an array of edges.
 *   Represents a full maze with a wall at every possible location.
 *   It looks like a grid.
 */
void make_grid(Vec_Edge *e, int width, int height);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Sorts the array randomly. Called the Fischer-Yates shuffle.
 */
void randomly_sort(Vec_Edge *e);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Finds the set representative for 'uv' among the disjoint sets.
 */
int set_find(Vec_int parent, int uv);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Takes the union of set u and v.
 */
void set_union(Vec_int *parent, Vec_int *rank, int u, int v);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Inserts an edge into the graph.
 *   Makes a wall in the maze.
 */
void Graph_insert(Graph *g, Edge e);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Returns true if the vertex 'vertex' forms an edge with the vertex associated with 'av', false otherwise.
 */
bool Graph_get_adjacent(Vec4_int av, int vertex);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Performs Kruskal's algorithm, but with a random sort.
 *   'st' for spanning tree.
 */
void kruskal_maze(Vec_Edge *e, Graph *st);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Puts the maze represented by the graph 'st' into the string 's'.
 *   'st' for spanning tree.
 */
void make_maze_line(String *s, Graph *st, int width, int height);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Returns a random number in the range [0, INT_MAX].
 */
int rand(void);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Kernel module init function.
 */
int init(void);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Kernel module exit function.
 */
void deinit(void);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Called when /proc/raptor_maze is read from.
 *   Prints a randomly generated maze to stdout.
 */
ssize_t do_maze(struct file *file, char __user *usr_buf, size_t count, loff_t *pos);

/* Name: Nathan Piel
 * Date: Thu Sep 19 2024
 * Description:
 *   Called when /proc/raptor_maze is written to.
 *   Receives two numbers separated by whitespace.
 *   The first number is the width, the second number is the height.
 *   Represents the width and height of the maze.
 */
ssize_t get_input(struct file *file, const char __user *usr_buf, size_t count, loff_t *pos);

static Arena arena = {0};
static int maze_width = 80;
static int maze_height = 28;

static struct file_operations proc_ops = {
    .owner = THIS_MODULE,
    .read = do_maze,
    .write = get_input,
};

void *Arena_alloc(Arena *arena, int n, int size) {
    void *p;
    if (arena->len >= arena->capacity) {
        if (arena->capacity == 0) arena->capacity = 2;
        else arena->capacity *= 2;
        arena->data = krealloc(arena->data, arena->capacity * sizeof(void*), GFP_KERNEL);
    }
    p = kzalloc(n * size, GFP_KERNEL);
    arena->data[arena->len++] = p;
    
    return p;
}

void *Arena_realloc(Arena *arena, void *p, int n, int size) {
    if (p) {
        int i;
        for (i = 0; i < arena->len; ++i) {
            if (arena->data[i] == p) {
                void *new_p = krealloc(p, n * size, GFP_KERNEL);
                arena->data[i] = new_p;
                return new_p;
            }
        }
    }

    return Arena_alloc(arena, n, size);
}

void Arena_init(Arena *arena) {
    arena->data = 0;
    arena->len = 0;
    arena->capacity = 0;
}

void Arena_deinit(Arena *arena) {
    int i;
    for (i = 0; i < arena->len; ++i) {
        kfree(arena->data[i]);
    }
    
    kfree(arena->data);
}

int rand(void) {
    int i;
    get_random_bytes(&i, sizeof(i));
    i = (i/2) + INT_MAX/2;
    return i;
}

void make_grid(Vec_Edge *e, int width, int height) {
    int row = 0;
    int col = 0;
    int i;

    Vec_alloc_empty(e, width*(height-1) + (width-1)*height);
    for (i = 0; i < width*height; ++i) {
        row = i / width;
        col = i % width;
        
        if (col < width-1) {
            Edge x; x.u=i; x.v=i+1;
            Vec_push(e, x);
        }
        if (row < height-1) {
            Edge x; x.u=i; x.v=i+width;
            Vec_push(e, x);
        }
    }
}

void randomly_sort(Vec_Edge *e) {
    int i;
    for (i = 0; i <= e->len-2; ++i) {
        int j = rand() % (e->len - i) + i;
        Edge tmp = e->data[i];
        e->data[i] = e->data[j];
        e->data[j] = tmp;
    }
}

int set_find(Vec_int parent, int uv) {
    int x = uv;
    for (;;) {
        if (parent.data[x] == x) break;
        else x = parent.data[x];
    }
    return x;
}

void set_union(Vec_int *parent, Vec_int *rank, int u, int v) {
    if (rank->data[u] < rank->data[v]) {
        parent->data[u] = v;
    } else if (rank->data[u] > rank->data[v]) {
        parent->data[v] = u;
    } else {
        parent->data[v] = u;
        ++rank->data[u];
    }
}

void Graph_insert(Graph *g, Edge e) {
    Vec_push(&g->data[e.u], e.v);
    Vec_push(&g->data[e.v], e.u);
}

bool Graph_get_adjacent(Vec4_int av, int vertex) {
    int i;
    for (i = 0; i < 4; ++i) {
        if (av.data[i] == vertex) return true;
    }
    
    return false;
}

void kruskal_maze(Vec_Edge *e, Graph *st) {
    Vec_int parent, rank;
    int i;

    randomly_sort(e);

    Vec_alloc_full(st, maze_width*maze_height);
    Vec_alloc_full(&parent, st->len);
    Vec_alloc_full(&rank, st->len);

    /* make a set for every vertex */
    for (i = 0; i < parent.len; ++i) {
        parent.data[i] = i;
    }

    for (i = 0; i < e->len; ++i) {
        int u = e->data[i].u;
        int v = e->data[i].v;
        
        int uset = set_find(parent, u);
        int vset = set_find(parent, v);

        if (uset != vset) {
            Graph_insert(st, e->data[i]);
            set_union(&parent, &rank, uset, vset);
        }
    }
}

void make_maze_line(String *s, Graph *st, int width, int height) {
    int i;

    Vec_alloc_empty(s, maze_width*2*maze_height); /* not actually enough to avoid reallocation, but that's why I wasted time writing macros to do it for me */
    
    Vec_push_extend(s, ' ');
    for (i = 0; i < width*2-1; ++i) Vec_push_extend(s, '_');
    Vec_push_extend(s, '\n');
    
    for (i = 0; i < height; ++i) {
        int j;
        
        Vec_push_extend(s, '|');
        for (j = 0; j < width; ++j) {
            int index = i*width+j;
            
            bool eastern_edge = Graph_get_adjacent(st->data[index], index+1);
            bool southern_edge = Graph_get_adjacent(st->data[index], index+width);
            
            if (southern_edge) Vec_push_extend(s, ' ');
            else Vec_push_extend(s, '_');
            
            if (eastern_edge) {
                bool next_southern = Graph_get_adjacent(st->data[index+1], index+1+width);
                if (next_southern) Vec_push_extend(s, ' ');
                else Vec_push_extend(s, '_');
            } else {
                Vec_push_extend(s, '|');
            }
        }
        
        Vec_push_extend(s, '\n');
    }

    Vec_push_extend(s, '\0');
}

ssize_t do_maze(struct file *file, char __user *usr_buf, size_t count, loff_t *pos) {
    Vec_Edge grid = {0};
    Graph spanning_tree = {0};
    String buffer = {0};

    if (*pos > 0) return 0;
    
    Arena_init(&arena);
    
    make_grid(&grid, maze_width, maze_height);
    kruskal_maze(&grid, &spanning_tree);
    make_maze_line(&buffer, &spanning_tree, maze_width, maze_height);
    
    if (copy_to_user(usr_buf, buffer.data, buffer.len)) {
        printk(KERN_INFO "proc/raptor_maze: UH OH\n");
    }
    
    Arena_deinit(&arena);

    *pos = buffer.len;
    
    return buffer.len;
}

ssize_t get_input(struct file *file, const char __user *usr_buf, size_t count, loff_t *pos) {
    if (2 != sscanf(usr_buf, "%d%d", &maze_width, &maze_height)) {
        printk("/proc/raptor_maze: input width then height ONLY! Separated by whitespace\n");
    }
    *pos = count;
    return count;
}

int init(void) {
    proc_create("raptor_maze", 0666, NULL, (struct file_operations*)&proc_ops);
    printk(KERN_INFO "/proc/raptor_maze created\n");
    return 0;
}

void deinit(void) {
    remove_proc_entry("raptor_maze", NULL);
    printk(KERN_INFO "/proc/raptor_maze removed\n");
}

module_init(init);
module_exit(deinit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Maze, but kernal"); /* haha it's spelled wrong */
MODULE_AUTHOR("Nathan Piel");
