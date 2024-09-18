/* #define KERNAL_MODE */
/* #define OLD_KERNAL */

#ifdef KERNAL_MODE

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/sprintf.h>

#define reallocate(p, new_n, new_size, flags) krealloc((p), (new_n) * (new_size), (flags))
#define allocate_zero(new_n, new_size, flags) kzalloc((new_n) * (new_size), (flags))
#define print printk
#define free_mem kfree

#else

#define GFP_KERNEL (void)0

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <limits.h>

#define reallocate(p, new_n, new_size, flags) realloc((p), (new_n)*(new_size))
#define allocate_zero(new_n, new_size, flags) calloc((new_n), (new_size))
#define print printf
#define KERN_INFO
#define free_mem free

#endif

typedef struct {
    void **data;
    int len;
    int capacity;
} Arena;

void *Arena_alloc(Arena *arena, int n, int size);
void *Arena_realloc(Arena *arena, void *p, int n, int size);
void Arena_init(Arena *arena);
void Arena_deinit(Arena *arena);

static Arena arena;

void *Arena_alloc(Arena *arena, int n, int size) {
    void *p;
    if (arena->len >= arena->capacity) {
        if (arena->capacity == 0) arena->capacity = 2;
        else arena->capacity *= 2;
        arena->data = reallocate(arena->data,
                                 arena->capacity,
                                 sizeof(void*),
                                 GFP_KERNEL);
    }
    p = allocate_zero(n, size, GFP_KERNEL);
    arena->data[arena->len++] = p;
    
    return p;
}

void *Arena_realloc(Arena *arena, void *p, int n, int size) {
    int i;
    for (i = 0; i < arena->len; ++i) {
        if (arena->data[i] == p) {
            void *new_p = reallocate(p, n, size, GFP_KERNEL);
            arena->data[i] = new_p;
            return new_p;
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
        free_mem(arena->data[i]);
    }

    free_mem(arena->data);
}

#define Vec_define(T) typedef struct { T *data; int len; int capacity; } Vec_##T

#define Vec_push_arena(vec, val, arena)                                 \
    do {                                                                \
        if ((vec)->len >= (vec)->capacity)  {                           \
            if ((vec)->capacity == 0) (vec)->capacity = 2;              \
            else (vec)->capacity *= 2;                                  \
                                                                        \
            (vec)->data = Arena_realloc(&(arena),                       \
                                        (vec)->data,                    \
                                        (vec)->capacity,                \
                                        sizeof(*(vec)->data));          \
        }                                                               \
        (vec)->data[(vec)->len++] = (val);                              \
    } while(0)

#define Vec_push(vec, val) Vec_push_arena(vec, val, arena)

#define Vec_pop(vec)                            \
    do {                                        \
        if ((vec)->len > 0) (--(vec)->len);     \
    } while (0)                                

#define Vec_contains(set, x, eq, contains)      \
    do {                                        \
        int i;                                  \
        bool broken = false;                    \
        for (i = 0; i < (set)->len; ++i) {      \
            if (eq((set)->data[i], (x))) {      \
                *(contains) = true;             \
                broken = true;                  \
                break;                          \
            }                                   \
        }                                       \
        if (!broken) *(contains) = false;       \
    } while (0)

#define Vec_push_new_arena(set, x, arena, eq)                   \
    do {                                                        \
        bool push = true;                                       \
        int i;                                                  \
        for (i = 0; i < (set)->len; ++i) {                      \
            if (eq((set)->data[i], (x))) { push=false; break; } \
        }                                                       \
        if (push) Vec_push_arena((set), (x), arena);            \
    } while (0)

#define Vec_push_new(set, x, eq) Vec_push_new_arena(set, x, arena, eq)

#define eq(a, b) (a) == (b)

typedef struct {
    int index;
} AdjVertex;

#define AdjVertex_eq(a, b) eq((a).index, (b).index)

typedef struct {
    int u;
    int v;
} Edge;

Vec_define(Edge);
Vec_define(int);
Vec_define(Vec_int);
Vec_define(AdjVertex);
Vec_define(Vec_AdjVertex);
Vec_define(char);

typedef Vec_char String;
typedef Vec_Vec_AdjVertex Graph;

void make_grid(Vec_Edge *, int width, int height);
void randomly_sort(Vec_Edge *e);
int find_set(Vec_Vec_int set, int uv);
void Graph_insert(Graph *g, Edge e);
bool Graph_get_adj(Vec_AdjVertex av, int index);
void kruskal_maze(Vec_Edge *e, Graph *mst);
void make_maze(String *s, Graph *mst, int width, int height, bool graph_mode);
void print_graph(String *s, Graph g);
void print_edges(String *s, Vec_Edge e);

#ifdef KERNAL_MODE
int rand(void);
#endif

#ifdef KERNAL_MODE
ssize_t proc_read(struct file *file, char *buf, size_t count, loff_t *pos);
int init(void);
void deinit(void);
ssize_t do_maze(struct file *file, char __user *usr_buf, size_t count, loff_t *pos);

static struct file_operations proc_ops = {
    .owner = THIS_MODULE,
    .read = do_maze,
};
#endif

#ifdef KERNAL_MODE
int rand(void) {
    int i;
    get_random_bytes(&i, sizeof(i));
    i = (i/2) + INT_MAX/2; /* range of [0, INT_MAX] probably */
    return i;
}
#endif

void make_grid(Vec_Edge *e, int width, int height) {
    int row = 0;
    int col = 0;
    int i;
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

int find_set(Vec_Vec_int set, int uv) {
    int i;
    for (i = 0; i < set.len; ++i) {
        Vec_int *inner = &set.data[i];
        bool contains;
        Vec_contains(inner, uv, eq, &contains);
        if (contains) return i;
    }

    return 0; 
}

void Graph_insert(Graph *g, Edge e) {
    Vec_AdjVertex *eu = &g->data[e.u];
    Vec_AdjVertex *ev = &g->data[e.v];

    AdjVertex av;
    av.index=e.v; Vec_push_new(eu, av, AdjVertex_eq);
    av.index=e.u; Vec_push_new(ev, av, AdjVertex_eq);
}

bool Graph_get_adj(Vec_AdjVertex av, int index) {
    int i;
    for (i = 0; i < av.len; ++i) {
        if (av.data[i].index == index) return true;
    }
    
    return false;
}

void kruskal_maze(Vec_Edge *e, Graph *mst) {
    Vec_Vec_int vertex_sets;
    int i;

    randomly_sort(e);

    vertex_sets.data = Arena_alloc(&arena, mst->len, sizeof(Vec_int));
    vertex_sets.len = mst->len;
    vertex_sets.capacity=mst->len;

    for (i = 0; i < vertex_sets.len; ++i) {
        Vec_int *set = &vertex_sets.data[i];
        Vec_push(set, i);
    }

    for (i = 0; i < e->len; ++i) {
        int u_set = find_set(vertex_sets, e->data[i].u);
        int v_set = find_set(vertex_sets, e->data[i].v);
        if (u_set != v_set) {
            int j;
            Graph_insert(mst, e->data[i]);
            
            for (j = 0; j < vertex_sets.data[v_set].len; ++j) {
                Vec_push(&vertex_sets.data[u_set], vertex_sets.data[v_set].data[j]);
            }
            vertex_sets.data[v_set].len = 0;
        }
    }
}

void print_graph(String *s, Graph g) {
    int i, k;
    k = 'A';
    for (i = 0; i < g.len; ++i) {
        Vec_AdjVertex av = g.data[i];
        int j;
        
        Vec_push(s, '['); Vec_push(s, k); Vec_push(s, ']'); Vec_push(s, '-'); Vec_push(s, '>'); Vec_push(s, '['); 
        for (j = 0; j < av.len; ++j) {
            AdjVertex v = av.data[j];
            Vec_push(s, 'A'+v.index); if (j < av.len-1) Vec_push(s, ' ');

        }
        Vec_push(s, ']');
        Vec_push(s, '\n');
        k++;
    }

    Vec_push(s, '\0');
}

void print_edges(String *s, Vec_Edge e) {
    int i;
    
    for (i = 0; i < e.len; ++i) {
        Vec_push(s, 'A'+e.data[i].u);
        Vec_push(s, 'A'+e.data[i].v);
        Vec_push(s, ' ');
    }
    Vec_push(s, '\n');
    Vec_push(s, '\0');
}

void make_maze(String *s, Graph *mst, int width, int height, bool line_mode) {
    int i;

    if (line_mode) {
        Vec_push(s, ' ');
        for (i = 0; i < width*2-1; ++i) Vec_push(s, '_');
        Vec_push(s, '\n');
    
        for (i = 0; i < height; ++i) {
            int j;
        
            Vec_push(s, '|');
            for (j = 0; j < width; ++j) {
                int index = i*width+j;
            
                bool eastern_edge = Graph_get_adj(mst->data[index], index+1);
                bool southern_edge = Graph_get_adj(mst->data[index], index+width);

                if (southern_edge) Vec_push(s, ' ');
                else Vec_push(s, '_');

                if (eastern_edge) {
                    bool next_southern = Graph_get_adj(mst->data[index+1], index+1+width);
                    if (next_southern) Vec_push(s, ' ');
                    else Vec_push(s, '_');
                } else {
                    Vec_push(s, '|');
                }
            }

            Vec_push(s, '\n');
        }
    } else {
        Vec_push(s, ' ');
        for (i = 0; i < width*2-1; ++i) Vec_push(s, '@');
        Vec_push(s, '\n');

        for (i = 0; i < height; ++i) {
            int j;
        
            Vec_push(s, '@');
            for (j = 0; j < width; ++j) {
                int index = i*width+j;
            
                bool eastern_edge = Graph_get_adj(mst->data[index], index+1);

                Vec_push(s, ' ');
                if (eastern_edge) {
                    Vec_push(s, ' ');
                } else if (j < width-1){
                    Vec_push(s, '#');
                }
            }

            Vec_push(s, '@');
            Vec_push(s, '\n');
            Vec_push(s, '@');
            for (j = 0; j < width; ++j) {
                int index = i*width+j;

                bool southern_edge = Graph_get_adj(mst->data[index], index+width);
                if (southern_edge) Vec_push(s, ' ');
                else Vec_push(s, '#');

                if (j < width-1){
                    Vec_push(s, '#');
                }
            }

            Vec_push(s, '@');
            Vec_push(s, '\n');
        }
    }
    Vec_push(s, '\0');
}

#ifdef KERNAL_MODE
ssize_t do_maze(struct file *file, char __user *usr_buf, size_t count, loff_t *pos)
#else
    int main(void)
#endif
{
    int width = 9;
    int height = 9;
    Vec_Edge grid = {0};
    Graph spanning_tree;
    String buffer = {0};
    /* String buffer_two = {0}; */
    /* String buffer_three = {0}; */
    #ifdef KERNAL_MODE
    unsigned long bytes_not_copied;
    #endif
    Arena_init(&arena);

    spanning_tree.data = Arena_alloc(&arena, width*height, sizeof(Vec_AdjVertex));

    spanning_tree.len = width*height;
    spanning_tree.capacity=width*height;

    #ifndef KERNAL_MODE
    srand(time(NULL));
    #endif
    
    make_grid(&grid, width, height);
    kruskal_maze(&grid, &spanning_tree);
    make_maze(&buffer, &spanning_tree, width, height, true);

    /* print_graph(&buffer_two, spanning_tree); */
    /* print_edges(&buffer_three, grid); */

    #ifdef KERNAL_MODE
    bytes_not_copied = copy_to_user(usr_buf, buffer.data, buffer.len);
    if (bytes_not_copied > 0) {
        print(KERN_INFO "proc/raptor_maze: UH OH\n");
    }
    #else
    printf("%s", buffer.data);
    /* printf("%s", buffer_two.data); */
    /* printf("%s", buffer_three.data); */
    #endif    
    
    Arena_deinit(&arena);

    #ifdef KERNAL_MODE
    return buffer.len;
    #else
    return 0;
    #endif
}

#ifdef KERNAL_MODE
int init(void) {
    proc_create("raptor_maze", 0, NULL,
                #ifndef OLD_KERNAL
                (struct proc_ops*)
                #endif
                &proc_ops
        );
    print(KERN_INFO "/proc/raptor_maze created\n");
    return 0;
}

void deinit(void) {
    remove_proc_entry("raptor_maze", NULL);
    print(KERN_INFO "proc/raptor_maze removed\n");
}


module_init(init);
module_exit(deinit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Maze, but kernal");
MODULE_AUTHOR("Nathan Piel");

#endif
